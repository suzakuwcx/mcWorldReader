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
#include <boost/python/tuple.hpp>
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
    /*
     * Boost Python binding must need a copy constructor
     */
    Section(const Section &);
    Section(nbt::value &val);
    ~Section();

    std::string &get(int x, int y, int z);
};


class Chunk {
private:
    std::vector<std::unique_ptr<Section>> vec;
public:
    Chunk();
    Chunk(const Chunk &);
    Chunk(const std::unique_ptr<nbt::tag_compound> &ptr);
    ~Chunk();

    bool is_null();
    Section &get(int y);
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

    Chunk &getitem(boost::python::tuple index);
    void mkcache();
    void empty_cache();
};


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


BOOST_PYTHON_MODULE(canvil) {
    boost::python::class_<std::string>("std_string")
        .def("c_str", &std::string::c_str);

    boost::python::class_<Section>("Section", boost::python::init<nbt::value &>())
        .def("get", &Section::get, boost::python::return_value_policy<boost::python::reference_existing_object>());

    boost::python::class_<Chunk>("Chunk", boost::python::init<>())
        .def("is_null", &Chunk::is_null)
        .def("get", &Chunk::get, boost::python::return_value_policy<boost::python::reference_existing_object>());


    boost::python::class_<Region>("Region", boost::python::init<const char *>())
        .def("mkcache", &Region::mkcache)
        .def("empty_cache", &Region::empty_cache)
        .def("__getitem__", &Region::getitem, boost::python::return_value_policy<boost::python::reference_existing_object>());

    boost::python::class_<World>("World", boost::python::init<const char *>())
        .def("get_region", &World::get_region, boost::python::return_value_policy<boost::python::reference_existing_object>())
        .def("get_block", &World::get_block, boost::python::return_value_policy<boost::python::reference_existing_object>());
}


#endif /* _ANVIL_H_ */
