import struct
import gzip
import zlib
import nbtlib
import io
import numpy as np


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
            palette = np.array([p["Name"].unpack() for p in states['palette']])

            #
            # If entire chunk has only one types of block(e.g. air), only this block will be stored
            #
            if 'data' not in states:
                blocks = np.full((16, 16, 16), palette[0])

            #
            # 'data' container 4096 element and split into long integer list to storage.
            # the value of the element is indicating the index of the block in palette.
            # the len of the element is determined by the number of palette in this chunk
            # and the mininal len is 4bit, e.g. if the number of palette is 17
            # because of that 2^4 = 16, so the len of a element is 5
            #
            else:
                data_len = max(4, (len(palette) - 1).bit_length())
                padding_per_long = 64 % data_len
                # 
                data = states['data']

                #
                # References:
                # https://github.com/MestreLion/mcworldlib/blob/main/mcworldlib/chunk.py
                # https://wiki.vg/Chunk_Format
                #
                # For example, assume data_len is 5:
                # LongArray([Long(2459565876494606864), Long(2459565876494606883), ... , Long(2459565876494606882)
                # The first Long will contain block 12 - 0, second will contain 24 - 13, ...
                # The bit distributed in each Long will be
                #
                # | Index  | PAD|12 |11 |10 | 9 |      | 0 |
                # +---------------------------------------------------
                # | bitmap | PPPAAAABBBBCCCCDDDD........LLLL
                #
                # Because of that bit operation in python is really difficult, so we need to unpackbits first
                # unpackbits example: 0x03 ---> 0B00000011 ---> [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01]
                # So, First, 
                #   reverse LongArray to make index continuous(from 11 - 0, 24 - 13 ... 4096 - 4084 to 4096 - 4084, 4083 - 4072, ..., 11 - 0 )
                #   change the view to uint8(8bit, like AAAA) then reshape by Long length(64)
                #   remove padding bit on the left([:, padding_per_long:64])
                #   spilt with data_len to parsing each block (4095, 4094, ... 1, 0)
                #   padding 0 value to make the value of each block back to uint64 length for unpack to real value
                #   unpack to real index value
                #   reverse again to make value match index (0, 1, 4094, 4095)
                #
                bit_map = np.unpackbits(data[::-1].astype(f">i8").view("uint8").unpack()).reshape(-1, 64)[:, padding_per_long:64].reshape(-1, data_len)
                bit_map = np.packbits(np.pad(bit_map, [(0, 0), (64 - data_len, 0)], 'constant'), axis=1)
                bit_map = bit_map.view(dtype=">q")[::-1][:4096]

                # (z, x, y) -> (x, y, z)
                blocks = palette[bit_map.flatten()].reshape(16, 16, 16).transpose(2, 0, 1)
                
            world["{}|{}|{}".format(chunk_nbt["xPos"].unpack(), chunk["Y"].unpack(), chunk_nbt["zPos"].unpack())] = blocks

    return

