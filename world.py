from anvil import Region
import glob
import multiprocessing
import re
from scipy.sparse import lil_matrix

class World():
    def __init__(self, path) -> None:
        self.region = '{}/region'.format(path)

        mca_list = []
        mca_coord_list = []
        x_min = 0
        x_max = 0
        z_min = 0
        z_max = 0
        for mca in glob.iglob('{}/r.*.*.mca'.format(self.region)):
            x, z = [int(n) for n in re.findall(r'-?\d+', mca)]
            if x > x_max: x_max = x
            if x < x_min: x_min = x
            if z > z_max: z_max = z
            if z < z_min: z_min = z
            mca_coord_list.append((x, z))
            mca_list.append(mca)

        regions_index = lil_matrix((x_max - x_min + 1, z_max - z_min + 1), dtype=int)
        regions = []
        with multiprocessing.Pool() as p:
            result = p.map(Region.load, mca_list)

        for coord, r in zip(mca_coord_list, result):
            regions_index[coord[0], coord[1]] = len(regions)
            regions.append(r)
            
        self.mca_coord_list = mca_coord_list
        self.regions_index = regions_index
        self.regions = regions

    def get_block(self, x, y, z):
        #
        # a region block size is 512 x 512 (32 x 32 chunks, each chunks 16 x 16)
        #
        regions_x = x // 512 
        regions_z = z // 512

        in_regions_x = x % 512
        in_regions_z = z % 512

        chunks_x = in_regions_x // 16
        chunks_y = y // 16
        chunks_z = in_regions_z // 16

        in_chunks_x = in_regions_x % 16
        in_chunks_y = y % 16
        in_chunks_z = in_regions_z % 16

        region = self.regions[self.regions_index[regions_x, regions_z]]
        chunks = region[chunks_x, chunks_z]
        sections = chunks[chunks_y]
        block = sections[in_chunks_x, in_chunks_y, in_chunks_z]

        return block
    
    def __iter__(self):
        self.index = 0
        return self

    def __next__(self):
        if self.index >= len(self.regions):
            raise StopIteration

        i = self.index
        self.index += 1
        return self.mca_coord_list[i], self.regions[i]


    def iter_all_blocks(self, skip_one_block_section=True, skip_block_list=None):
        for region_coord, region in self:
            for chunk_coord, chunks in region:
                if chunks is None:
                    continue

                for section_coord, section in chunks:
                    if section.is_one_block_section():
                        continue

                    for block_coord, block in section:
                        if skip_block_list is not None and block in skip_block_list:
                            continue

                        x = region_coord[0] * 32 * 16 + chunk_coord[0] * 16 + block_coord[0]
                        y = section_coord * 16 + block_coord[1]
                        z = region_coord[1] * 32 * 16 + chunk_coord[1] * 16 + block_coord[2]
                        yield (x, y, z), block

            region.empty_cache()

