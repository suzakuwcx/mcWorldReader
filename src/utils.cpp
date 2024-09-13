#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/traits.hpp>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <istream>
#include <iterator>
#include <memory>
#include <sstream>
#include <vector>

namespace bio = boost::iostreams;

namespace zlib {
    void decompress(std::vector<unsigned char> &output) {
        std::string src(output.begin(), output.end());
        std::istringstream iss(src, std::ios::binary | std::ios::in);
        std::vector<unsigned char> vec;
        char buff[4096];

        memset(buff, 0, sizeof(buff));

        bio::filtering_istreambuf in_filter;
        in_filter.push(bio::zlib_decompressor());
        in_filter.push(iss);

        std::istream in(&in_filter);

        while (!in.eof()) {
            in.read(buff, 4096);
            vec.insert(vec.end(), buff, buff + in.gcount());
        }

        output.clear();
        output.insert(output.end(), vec.data(), vec.data() + vec.size());
    }
}

namespace gzip {
    void decompress(std::vector<unsigned char> &output) {
        std::string src(output.begin(), output.end());
        std::istringstream iss(src, std::ios::binary | std::ios::in);
        std::vector<unsigned char> vec;
        char buff[4096];

        memset(buff, 0, sizeof(buff));

        bio::filtering_istreambuf in_filter;
        in_filter.push(bio::gzip_decompressor());
        in_filter.push(iss);

        std::istream in(&in_filter);

        while (!in.eof()) {
            in.read(buff, 4096);
            vec.insert(vec.end(), buff, buff + in.gcount());
        }

        output.clear();
        output.insert(output.end(), vec.data(), vec.data() + vec.size());
    }
}
