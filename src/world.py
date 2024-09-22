from canvil import Region

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
                    yield (x, y, z), block
                    
