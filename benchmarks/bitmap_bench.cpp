// bitmap_bench -- the example benchmark driver.
//
// Loads one dataset, builds it with one backend, runs a query file against it
// and writes one CSV row per (query, repetition). Everything the report needs
// is in that CSV: build time, per-query time split by AND/OR, and the sizes the
// compression ratio is computed from.
//
// What is timed and what is not:
//   build_time_ms   - the whole build() call, i.e. converting the uncompressed
//                     dataset into the backend's representation.
//   query_time_ns   - one query_and()/query_or() call and nothing else. The
//                     result stays compressed; counting its bits and decoding
//                     it for verification happen outside the timer.
// Verification (--verify=1) and warmup runs are never timed.
#include "backends/bitmap_backend.hpp"
#include "backends/raw_backend.hpp"
#include "backends/roaring_backend.hpp"
#include "backends/wah_backend.hpp"
#include "common/bitmap_io.hpp"
#include "common/query.hpp"
#include "common/timing.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool starts_with(const std::string& s, const std::string& p) {
    return s.size() >= p.size()
           && std::memcmp(s.data(), p.data(), p.size()) == 0;
}

std::string arg_value(const std::string& a, const std::string& key) {
    return a.substr(key.size());
}

// Register new backends here.
std::unique_ptr<bench::BitmapBackend> make_backend(const std::string& name) {
    if (name == "raw")     return std::make_unique<bench::RawBackend>();
    if (name == "roaring") return std::make_unique<bench::RoaringBackend>();
    if (name == "wah")     return std::make_unique<bench::WahBackend>();
    throw std::runtime_error("unknown backend: " + name
                             + " (available: raw, roaring, wah)");
}

// Last path component, tolerating the trailing slash a shell may append to a
// directory argument (which would otherwise leave the CSV column empty).
std::string leaf_name(const std::string& path) {
    std::string s = path;
    while (s.size() > 1 && (s.back() == '/' || s.back() == '\\')) s.pop_back();
    return std::filesystem::path(s).filename().string();
}

// Metadata strings end up in a CSV field; keep the file trivially parseable.
std::string csv_safe(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        if (c == ',' || c == '"' || c == '\n') c = '_';
    return out;
}

void usage() {
    std::cerr << "usage: bitmap_bench --backend=raw|roaring|wah --dataset=DIR "
                 "--queries=PATH --out=CSV\n"
                 "                    [--repetitions=N] [--warmup=N] "
                 "[--verify=0|1]\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string backend_name, dataset_dir, queries_path, out_path;
    bool verify = false;
    int repetitions = 1;
    int warmup = 0;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (starts_with(a, "--backend="))      backend_name = arg_value(a, "--backend=");
        else if (starts_with(a, "--dataset=")) dataset_dir  = arg_value(a, "--dataset=");
        else if (starts_with(a, "--queries=")) queries_path = arg_value(a, "--queries=");
        else if (starts_with(a, "--out="))     out_path     = arg_value(a, "--out=");
        else if (starts_with(a, "--verify="))  verify = (arg_value(a, "--verify=") != "0");
        else if (starts_with(a, "--repetitions=")) repetitions = std::stoi(arg_value(a, "--repetitions="));
        else if (starts_with(a, "--warmup="))  warmup = std::stoi(arg_value(a, "--warmup="));
        else { usage(); return 2; }
    }
    if (backend_name.empty() || dataset_dir.empty()
        || queries_path.empty() || out_path.empty()) {
        usage();
        return 2;
    }
    if (repetitions < 1) repetitions = 1;
    if (warmup < 0) warmup = 0;

    bench::DatasetMetadata meta;
    std::vector<std::vector<uint64_t>> bitmaps;
    std::vector<bench::Query> queries;
    try {
        bitmaps = bench::load_dataset(dataset_dir, meta);
        queries = bench::read_queries(queries_path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    // Every query index must address an existing bitmap, otherwise a backend
    // would read out of bounds.
    for (size_t qi = 0; qi < queries.size(); ++qi) {
        if (queries[qi].ids.empty()) {
            std::cerr << "error: query " << qi << " has no operands\n";
            return 1;
        }
        for (size_t id : queries[qi].ids) {
            if (id >= meta.num_bitmaps) {
                std::cerr << "error: query " << qi << " refers to bitmap " << id
                          << ", dataset has " << meta.num_bitmaps << "\n";
                return 1;
            }
        }
    }

    std::unique_ptr<bench::BitmapBackend> backend;
    try {
        backend = make_backend(backend_name);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }

    bench::Timer build_timer;
    backend->build(bitmaps, meta.num_bits);
    const double build_ms = build_timer.elapsed_ms();

    // The uncompressed dataset is the reference point for the compression
    // ratio, and (under --verify=1) the reference answer for every query.
    bench::RawBackend reference;
    reference.build(bitmaps, meta.num_bits);
    const size_t raw_bytes = reference.serialized_bytes();
    const size_t compressed_bytes = backend->serialized_bytes();
    const double ratio = compressed_bytes
                             ? static_cast<double>(raw_bytes) / static_cast<double>(compressed_bytes)
                             : 0.0;

    const auto out_dir = std::filesystem::path(out_path).parent_path();
    if (!out_dir.empty()) std::filesystem::create_directories(out_dir);
    std::ofstream csv(out_path);
    if (!csv) {
        std::cerr << "error: cannot write " << out_path << "\n";
        return 1;
    }
    csv << "backend,dataset,kind,params,num_bits,num_bitmaps,density,seed,"
           "query_file,query_index,query_type,query_width,repetition,"
           "build_time_ms,query_time_ns,result_cardinality,"
           "raw_bytes,compressed_bytes,compression_ratio\n";

    const std::string dataset_name = leaf_name(dataset_dir);
    const std::string queries_name = leaf_name(queries_path);

    auto run_query = [&](const bench::BitmapBackend& b, const bench::Query& q) {
        return (q.op == bench::QueryOp::And) ? b.query_and(q.ids)
                                             : b.query_or(q.ids);
    };

    size_t mismatches = 0;
    for (size_t qi = 0; qi < queries.size(); ++qi) {
        const auto& q = queries[qi];

        if (verify) {
            const auto got      = run_query(*backend, q)->to_words();
            const auto expected = run_query(reference, q)->to_words();
            if (got != expected) {
                ++mismatches;
                std::cerr << "verify: mismatch at query " << qi << "\n";
            }
        }

        for (int w = 0; w < warmup; ++w) (void)run_query(*backend, q);

        for (int r = 0; r < repetitions; ++r) {
            bench::Timer query_timer;
            bench::QueryResultPtr result = run_query(*backend, q);
            const int64_t query_ns = query_timer.elapsed_ns();

            csv << backend->name()          << ","
                << csv_safe(dataset_name)   << ","
                << csv_safe(meta.kind)      << ","
                << csv_safe(meta.params)    << ","
                << meta.num_bits            << ","
                << meta.num_bitmaps         << ","
                << meta.density             << ","
                << meta.seed                << ","
                << csv_safe(queries_name)   << ","
                << qi                       << ","
                << bench::query_op_str(q.op) << ","
                << q.ids.size()             << ","
                << r                        << ","
                << build_ms                 << ","
                << query_ns                 << ","
                << result->cardinality()    << ","
                << raw_bytes                << ","
                << compressed_bytes         << ","
                << ratio                    << "\n";
        }
    }

    std::cout << "backend=" << backend->name()
              << " dataset=" << dataset_name
              << " queries=" << queries.size()
              << " reps=" << repetitions
              << " build=" << build_ms << "ms"
              << " size=" << compressed_bytes << "B"
              << " ratio=" << ratio
              << " -> " << out_path << "\n";
    if (verify) {
        std::cout << "verify: " << mismatches << " mismatches out of "
                  << queries.size() << " queries\n";
        if (mismatches > 0) return 1;
    }
    return 0;
}
