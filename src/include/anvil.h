#ifndef _ANVIL_H_
#define _ANVIL_H_ 1

#include "tag_compound.h"
#include "value.h"

#include <memory>
#include <vector>
#include <cstdint>
#include <regex>
#include <string>
#include <tuple>
#include <sys/types.h>
#include <unordered_map>
#include <boost/python/object_protocol_core.hpp>
#include <boost/python/self.hpp>
#include <boost/python/init.hpp>
#include <boost/python.hpp>

struct region_header {
    uint32_t st_ino : 24;
    uint32_t st_blocks : 8;
};

struct chunk_header {
    uint32_t st_size;
    uint8_t st_mode;
};

#define bswap_24(x) (\
    (x & 0xff) << 16 | \
    (x & 0xff00) | \
    (x & 0xff0000) >> 16 \
)

class Section {
private:
    std::unique_ptr<std::vector<std::string>> palette;
    std::vector<uint8_t> bitmap;
public:
    Section();
    Section(nbt::value &val);
    ~Section();

    std::string &get(int x, int y, int z);
};


class Chunk {
private:
    std::vector<std::shared_ptr<Section>> vec;
public:
    Chunk();
    Chunk(const std::unique_ptr<nbt::tag_compound> &ptr);
    ~Chunk();

    bool is_null();
    std::shared_ptr<Section> &get(int y);
};


class Region {
private:
    std::string file;
    bool cache = false;
    std::unique_ptr<std::vector<std::unique_ptr<Chunk>>> chunks_map;

    static std::unique_ptr<nbt::tag_compound> parse_region_file(std::vector<unsigned char> *buff, struct region_header r_head);
public:
    Region(const Region &);
    Region(std::string &&file);
    ~Region();

    Chunk &getitem(int x, int z);
    void mkcache();
    void empty_cache();
};


BOOST_PYTHON_MODULE(canvil) {
    boost::python::class_<Region>("Region", boost::python::init<const char *>())
        .def("mkcache", &Region::mkcache);
}


class World {
private:
    std::string path;
    std::string region_path;
    std::regex mca_pattern;

    std::vector<std::tuple<int32_t, int32_t> > mca_coord_list;
    std::vector<Region> regions;
    std::unordered_map<uint64_t, int32_t> region_index;

    static std::regex number_pattern;
    static std::tuple<int32_t, int32_t> parsing_coord(const std::basic_string<char> &file);
    static uint64_t get_key(int32_t x, int32_t z);
public:
    World(std::string &&path);
    ~World();

    Region &get_region(int32_t x, int32_t z);
    std::string &get_block(int32_t x, int32_t y, int32_t z);
};

#endif /* _ANVIL_H_ */
