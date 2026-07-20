#include "common/bitmap_io.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace bench {

namespace fs = std::filesystem;

std::string bitmap_filename(size_t index) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "bitmap_%03zu.bin", index);
    return std::string(buf);
}

DatasetMetadata read_metadata(const std::string& dir) {
    fs::path mp = fs::path(dir) / "metadata.txt";
    std::ifstream in(mp);
    if (!in) throw std::runtime_error("cannot open " + mp.string());
    DatasetMetadata m;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "num_bits")         m.num_bits = std::stoull(val);
        else if (key == "num_bitmaps") m.num_bitmaps = std::stoull(val);
        else if (key == "word_bits")   m.word_bits = std::stoull(val);
        else if (key == "format")      m.format = val;
        else if (key == "kind")        m.kind = val;
        else if (key == "params")      m.params = val;
        else if (key == "density")     m.density = std::stod(val);
        else if (key == "seed")      { m.seed = std::stoull(val); m.seed_present = true; }
        // Unknown keys are ignored on purpose: a generator may record whatever
        // else it wants without breaking the reader.
    }
    if (m.word_bits != 64)
        throw std::runtime_error("only word_bits=64 supported");
    if (m.format != "raw_uint64_le")
        throw std::runtime_error("only format=raw_uint64_le supported");
    return m;
}

void write_metadata(const std::string& dir, const DatasetMetadata& meta) {
    fs::create_directories(dir);
    fs::path mp = fs::path(dir) / "metadata.txt";
    std::ofstream out(mp);
    if (!out) throw std::runtime_error("cannot write " + mp.string());
    out << "num_bits=" << meta.num_bits << "\n";
    out << "num_bitmaps=" << meta.num_bitmaps << "\n";
    out << "word_bits=" << meta.word_bits << "\n";
    out << "format=" << meta.format << "\n";
    if (!meta.kind.empty())   out << "kind=" << meta.kind << "\n";
    if (!meta.params.empty()) out << "params=" << meta.params << "\n";
    if (meta.density >= 0.0)  out << "density=" << meta.density << "\n";
    if (meta.seed_present)    out << "seed=" << meta.seed << "\n";
}

std::vector<uint64_t> read_bitmap(const std::string& path, size_t num_words) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<uint64_t> words(num_words);
    in.read(reinterpret_cast<char*>(words.data()),
            static_cast<std::streamsize>(num_words * sizeof(uint64_t)));
    if (static_cast<size_t>(in.gcount()) != num_words * sizeof(uint64_t))
        throw std::runtime_error("short read in " + path);
    return words;
}

void write_bitmap(const std::string& path, const std::vector<uint64_t>& words) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot write " + path);
    out.write(reinterpret_cast<const char*>(words.data()),
              static_cast<std::streamsize>(words.size() * sizeof(uint64_t)));
}

std::vector<std::vector<uint64_t>> load_dataset(const std::string& dir,
                                                DatasetMetadata& out_meta) {
    out_meta = read_metadata(dir);
    size_t nw = words_for_bits(out_meta.num_bits);
    std::vector<std::vector<uint64_t>> bms;
    bms.reserve(out_meta.num_bitmaps);
    for (size_t i = 0; i < out_meta.num_bitmaps; ++i) {
        fs::path p = fs::path(dir) / bitmap_filename(i);
        bms.emplace_back(read_bitmap(p.string(), nw));
    }
    return bms;
}

} // namespace bench
