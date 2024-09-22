from canvil import Region
import glob
import re


class ERegion(Region):
    def __init__(self, file):
        super().__init__(file)
    
    def __iter__(self):
        self.index = 0
        self.mkcache()
        return self
    
    def __next__(self):
        if self.index >= 1024:
            raise StopIteration

        x = self.index // 32
        z = self.index % 32
        self.index += 1
        return (x, z), self[x, z]


    def iter_all_blocks(self, skip_one_block_section=True):
        for chunk_coord, chunk in self:
            if skip_one_block_section and chunk.is_null():
                continue

            for Y in range(-4, 20):
                section = chunk.get(Y)
                if section.is_one_block_section():
                    continue

                for i in range(1024):
                    x = i // 256
                    y = (i % 256) // 16
                    z = i % 16
                    block = section.get(x, y, z).c_str()
                    x += chunk_coord[0] * 16
                    y += Y * 16
                    z += chunk_coord[1] * 16
                    yield (x, y, z), block
                    

class World():
    def __init__(self, path) -> None:
        self.region = '{}/region'.format(path)

        regions = []
        mca_coord_list = []
        regions_index = {}
        for mca in glob.iglob('{}/r.*.*.mca'.format(self.region)):
            x, z = [int(n) for n in re.findall(r'-?\d+', mca)]
            regions_index[(x, z)] = len(regions)
            regions.append(ERegion(mca))
            mca_coord_list.append((x, z))
            
        self.mca_coord_list = mca_coord_list
        self.regions_index = regions_index
        self.regions = regions

    
    def get_region(self, x, z):
        #
        # x and z means region x and region z
        #
        try:
            index = self.regions_index[(x, z)]
        except:
            return None
        return self.regions[index]
    

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

        region = self.get_region(regions_x, regions_z)
        chunks = region[chunks_x, chunks_z]
        if chunks.is_null():
            return None

        sections = chunks.get(chunks_y)
        block = sections.get(in_chunks_x, in_chunks_y, in_chunks_z)

        return block.c_str()
    
    def __iter__(self):
        self.index = 0
        return self

    def __next__(self):
        if self.index >= len(self.regions):
            raise StopIteration

        i = self.index
        self.index += 1
        return self.mca_coord_list[i], self.regions[i]


    def iter_all_blocks(self, skip_one_block_section=True):
        for region_coord, region in self:
            for coord, block in region.iter_all_blocks(skip_one_block_section):
                x = region_coord[0] * 512 + coord[0]
                y = coord[1]
                z = region_coord[1] * 512 + coord[2]
                yield (x, y, z), block

            region.empty_cache()

