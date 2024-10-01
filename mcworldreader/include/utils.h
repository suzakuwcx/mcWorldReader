#include <vector>


namespace zlib {
    void decompress(std::vector<unsigned char> &output);
}


namespace gzip {
    void decompress(std::vector<unsigned char> &output);
}
