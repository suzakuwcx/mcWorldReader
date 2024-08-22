import struct
import gzip
import zlib
import nbtlib
import io

def parse(file):
    with open(file, mode='rb') as f:
        buff = f.read()

    """
    Parsing Region file header, we only parsing chunk data

    Like file system, a region file are divided into blocks, each block size is 4KB, 
    and the first 2 block are superblock, the first superblock container the metadata
    of the chunk ( coordinate to inode )

    | chunk    |   
    +----------+-------------+----------+
    | 0 - 4095 | 4096 - 8191 | 8192 - ? |
    +----------+-------------+----------+
    """

    #
    # A region file contains 32 * 32 chunks, each chunk take 4bit location in inode 0
    #
    locations_bytes = struct.unpack_from('>{}'.format('4s' * 1024), buff, offset=0)

    for location in locations_bytes:
        # 
        # | 3bit   |    1bit      |
        # | inode  | sector_count |, Big ending
        #
        offset_bytes, sector_count = struct.unpack_from('>3s1B', location)
        offset = int.from_bytes(offset_bytes, byteorder='big')

        if offset == 0 and sector_count == 0:
            continue

        #
        # read sector that contain chunk file
        #
        sector = buff[offset * 4096:(offset + sector_count) * 4096]

        #
        # chunk file format, big ending:
        # |   4bit          | 1 bit   | 5 bit -
        # |  data length    | 1 bit   | 5 bit - 
        #
        chunk_file_length, chunk_compress_type = struct.unpack_from('>IB', sector)
        chunk_file = sector[5:5 + chunk_file_length]
        
        #
        # compress type:
        # 1 	GZip (RFC1952)
        # 2 	Zlib (RFC1950)
        # 3 	not compress
        # 4 	LZ4
        # 127 	preserve
        #
        if chunk_compress_type == 1:
            chunk_file = gzip.decompress(chunk_file)
        elif chunk_compress_type == 2:
            chunk_file = zlib.decompress(chunk_file)
        else:
            pass

        chunk_nbt = nbtlib.File.from_fileobj(io.BytesIO(chunk_file))

        world = {}

        #
        # the metadata offset of the chunk (x, z) is "x + 32z"
        #
        for chunk in chunk_nbt["sections"]:
            if 'block_states' not in chunk:
                continue

            states = chunk['block_states']
            palette = states['palette']

            blocks = [[[0] * 16 for _ in range(16)] for _ in range(16)]

            #
            # If entire chunk has only one types of block(e.g. air), only this block will be stored
            #
            if 'data' not in states:
                for x in range(16):
                    for y in range(16):
                        for z in range(16):
                            blocks[x][y][z] = palette[0]["Name"].unpack()

            #
            # 'data' container 4096 element and split into long integer list to storage.
            # the value of the element is indicating the index of the block in palette.
            # the len of the element is determined by the number of palette in this chunk
            # and the mininal len is 4bit, e.g. if the number of palette is 17
            # because of that 2^4 = 16, so the len of a element is 5
            #
            else:
                data_len = max(4, (len(palette) - 1).bit_length())
                blocks_per_long = 64 // data_len

                data = states['data']

                for i, long_integer in enumerate(data):
                    bit_map = format(long_integer.as_unsigned, '064b')

                    for j in range(blocks_per_long):
                        index = i * blocks_per_long + j
                        if index == 4096:
                            break

                        palette_index = int(bit_map[64 - (j + 1) * data_len:64 - j * data_len], 2)
                        y = index // (16 * 16)
                        z = (index - y * 16 * 16) // 16
                        x = index % 16
                        blocks[x][y][z] = palette[palette_index]["Name"].unpack()
                
            world["{}|{}|{}".format(chunk_nbt["xPos"].unpack(), chunk["Y"].unpack(), chunk_nbt["zPos"].unpack())] = blocks

    return

