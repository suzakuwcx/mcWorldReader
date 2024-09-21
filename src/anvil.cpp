#include "anvil.h"
#include "pool.hpp"

#include "tag_list.h"
#include "tag.h"
#include "tagfwd.h"
#include "utils.h"

#include "io/stream_reader.h"
#include "tag_compound.h"
#include "tag_string.h"
#include "tag_array.h"
#include "value.h"

#include <boost/asio/thread_pool.hpp>
#include <boost/python/extract.hpp>
#include <boost/python/handle.hpp>
#include <boost/python/tuple.hpp>
#include <boost/asio/post.hpp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <ios>
#include <iostream>
#include <iterator>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <tuple>
#include <utility>
#include <vector>
#include <arpa/inet.h>
#include <byteswap.h>
#include <make_unique.h>
#include <filesystem>


Section::Section(nbt::value &val) : bitmap(4096, 0)
{
    nbt::tag_compound &compound = static_cast<nbt::tag_compound &>(val.get());
    if (!compound.has_key("block_states"))
        return;

    nbt::tag_compound &bs = static_cast<nbt::tag_compound &>(compound.at("block_states").get());

    nbt::tag_list &plt = static_cast<nbt::tag_list &>(bs.at("palette").get());

    this->palette = std::make_unique<std::vector<std::string>>(plt.size());

    for (int i = 0; i < plt.size(); ++i)
        this->palette->at(i) = static_cast<nbt::tag_string &>(plt.at(i).at("Name").get());

    if (!bs.has_key("data"))
        return;

    size_t s = plt.size() - 1;
    int data_len = 0;
    uint64_t mask = 0xffffffffffffffff;

    if (s < 16) {
        mask <<= 4;
        data_len  = 4;
    } else {
        while (s != 0) {
            s >>= 1;
            mask <<= 1;
            ++data_len;
        }
    }

    int block_per_long = 64 / data_len;
    mask = ~mask;

    nbt::tag_long_array &data = static_cast<nbt::tag_long_array &>(bs.at("data").get());
    int index;

    for (int i = 0; i < data.size(); ++i) {
        uint64_t bit_map = data.at(i);
        for (int j = 0; j < block_per_long; ++j) {
            index = block_per_long * i + j;
            if (index >= 4096)
                break;

            this->bitmap.at(index) = bit_map & mask;
            bit_map >>= data_len;
        }
    }
}


Section::Section(const Section& sec) : bitmap(sec.bitmap)
{
    this->palette = std::make_unique<std::vector<std::string>>(*(sec.palette));
}


Section::~Section() = default;


std::string &Section::get(int x, int y, int z)
{
    return ((*(this->palette))[(this->bitmap)[256 * x + 16 * y + z]]);
}


Chunk::Chunk(): vec(0) {}


Chunk::Chunk(const Chunk &ch): vec(24)
{
    for (int i = 0; i < vec.size(); ++i)
        vec[i] = std::make_unique<Section>(*(ch.vec[i]));
}


Chunk::Chunk(const std::unique_ptr<nbt::tag_compound> &ptr): vec(24)
{
    auto val(ptr->at("sections"));
    int i;

    if (static_cast<int>(val.at(0).at("Y")) == -5) {
        for (i = -4; i < 20; ++i)
            this->vec[i + 4] = std::make_unique<Section>(val.at(i + 5));        
    } else {
        for (i = -4; i < 20; ++i)
            this->vec[i + 4] = std::make_unique<Section>(val.at(i + 4));
    }
}


Chunk::~Chunk() = default;


bool Chunk::is_null()
{
    return vec.size() == 0;
}


Section &Chunk::get(int y)
{
    return *((this->vec)[y + 4]);
}


Region::Region(const Region &r)
{
    this->cache = r.cache;
    this->file = r.file;
    if (this->cache)
        this->mkcache();
}


Region::Region(std::string &&f)
{
    this->chunks_map = std::make_unique<std::vector<std::unique_ptr<Chunk>>>(1024);
    this->file = f;
}


Region::~Region() = default;


std::unique_ptr<nbt::tag_compound> Region::parse_region_file(std::vector<unsigned char> *buff, struct region_header r_head)
{
    if (r_head.st_blocks == 0)
        return nullptr;

    struct chunk_header c_head;
    unsigned char *begin = buff->data() + 4096 * r_head.st_ino;

    memcpy(&c_head, begin, 5);
    c_head.st_size = bswap_32(c_head.st_size);

    /* leave some space to save decompressed NBT */
    std::vector<unsigned char> chunk_data(c_head.st_size);
    chunk_data.reserve(c_head.st_size * 10);

    begin += 5;

    memcpy(chunk_data.data(), begin, c_head.st_size);

    if (c_head.st_mode == 1)
        gzip::decompress(chunk_data);
    else if (c_head.st_mode == 2)
        zlib::decompress(chunk_data);
    else
        throw std::runtime_error("Unknown compression type:" + std::to_string(c_head.st_mode));

    std::string nbt_src(chunk_data.begin(), chunk_data.end());
    std::istringstream file(nbt_src, std::ios::binary);
    nbt::io::stream_reader reader(file);
    auto pair = reader.read_compound();

    return std::move(pair.second);
}


void Region::mkcache()
{
    if (this->cache)
        return;

    std::ifstream in(this->file, std::ios::binary | std::ios::ate);
    std::vector<unsigned char> buff(in.tellg());
    std::vector<struct region_header> location_bytes(1024);
    struct region_header header;

    this->chunks_map = std::make_unique<std::vector<std::unique_ptr<Chunk>>>(1024);

    memset(buff.data(), 0, buff.size());
    memset(location_bytes.data(), 0, location_bytes.size() * sizeof(struct region_header));

    in.seekg(0);
    in.read((char *)buff.data(), buff.size());
    in.close();

    for (int i = 0; i < 1024; ++i) {
        memcpy(&header, buff.data() + i * sizeof(struct region_header), sizeof(struct region_header));

        header.st_ino = bswap_24(header.st_ino);
        location_bytes.at(i) = header;
    }

    std::vector<std::future<std::unique_ptr<nbt::tag_compound> > > vec(1024);

    for (int i = 0; i < 1024; ++i) {
        vec[i] = Pool::submit(&Region::parse_region_file, &buff, location_bytes[i]);
    }

    for (int i = 0; i < 1024; ++i) {
        std::unique_ptr<nbt::tag_compound> ptr = vec[i].get();

        if (ptr == nullptr)
            (*(this->chunks_map))[i] = std::make_unique<Chunk>();
        else
            (*(this->chunks_map))[i] = std::make_unique<Chunk>(ptr);
    }

    this->cache = true;
}


void Region::empty_cache()
{
    if (!this->cache)
        return;

    this->cache = false;
    this->chunks_map.reset();
}


Chunk &Region::getitem(boost::python::tuple index)
{
    this->mkcache();
    int x = boost::python::extract<int>(index[0]);
    int z = boost::python::extract<int>(index[1]);
    return (*((*(this->chunks_map))[x * 32 + z]));
}


std::regex World::number_pattern("-?\\d+");


World::World(std::string &&path)
{
    this->path = path;
    this->region_path = path + "/region";
    this->mca_pattern = this->region_path + R"(/r\.-?\d+\.-?\d+\.mca)";

    std::smatch m;

    for (const auto &entry : std::filesystem::directory_iterator(this->region_path)) {
        if (std::regex_match(entry.path().native(), m, this->mca_pattern)) {
            std::tuple<int32_t, int32_t> tp = parsing_coord(entry.path().native().substr(region_path.length() + 1));
            this->mca_coord_list.push_back(tp);
            this->region_index[get_key(std::get<0>(tp), std::get<1>(tp))] = regions.size();
            this->regions.emplace_back(entry.path().c_str());
        }
    }
}


World::~World() = default;


std::tuple<int32_t, int32_t> World::parsing_coord(const std::basic_string<char> &file)
{
    int32_t x = 0;
    int32_t z = 0;

    auto iter_begin = std::sregex_iterator(file.begin(), file.end(), number_pattern);
    auto iter_end = std::sregex_iterator();

    if (std::distance(iter_begin, iter_end) != 2)
        throw std::runtime_error("invalid mca file name:" + file);

    x = std::stoi((*iter_begin).str());
    z = std::stoi((*(++iter_begin)).str());

    return std::tuple<int32_t, int32_t>(x, z);
}


uint64_t World::get_key(int32_t x, int32_t z)
{
    uint64_t ret = static_cast<uint64_t>(x);
    ret <<= 32;
    ret |= static_cast<uint64_t>(z);
    return ret;
}


Region &World::get_region(int32_t x, int32_t z)
{
    return this->regions[region_index[get_key(x, z)]];
}


std::string &World::get_block(int32_t x, int32_t y, int32_t z)
{
    /*
     * to prevent deviation, e.g. block y=-15 is at chunk Y=-1, 
     * but -15 / 16 = 0, so we add 64 offset, then subtract 4
     * at final. e.g. y = -15 + 64 = 49, then 49 / 16 = 3, 3 - 4 = -1
     */
    y += 64;

    int32_t regions_x = x / 512;
    int32_t regions_z = z / 512;

    int32_t in_regions_x = x % 512;
    int32_t in_regions_z = z % 512;


    int32_t chunks_x = in_regions_x / 16;
    int32_t chunks_y = y / 16;
    int32_t chunks_z = in_regions_z / 16;

    chunks_y -= 4;

    int32_t in_chunks_x = in_regions_x % 16;
    int32_t in_chunks_y = y % 16;
    int32_t in_chunks_z = in_regions_z % 16;

    Region &r = this->get_region(regions_x, regions_z);
    Chunk &c = r.getitem(boost::python::make_tuple<int, int>(chunks_x, chunks_z));
    if (c.is_null()) {
        char buff[128];
        snprintf(buff, sizeof(buff), "Uninitilize block (%d, %d, %d)", x, y, z);
        throw std::runtime_error(buff);
    }
    Section &s = c.get(chunks_y);
    return s.get(in_chunks_x, in_chunks_y, in_chunks_z);
}
