#include "common/query.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
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

void usage() {
    std::cerr << "usage: generate_queries --out=PATH --num-bitmaps=N "
              << "--num-queries=Q --min-width=A --max-width=B --seed=S\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string out;
    size_t num_bitmaps = 0, num_queries = 0;
    size_t min_w = 2, max_w = 5;
    uint64_t seed = 0;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (starts_with(a, "--out=")) out = arg_value(a, "--out=");
        else if (starts_with(a, "--num-bitmaps="))
            num_bitmaps = std::stoull(arg_value(a, "--num-bitmaps="));
        else if (starts_with(a, "--num-queries="))
            num_queries = std::stoull(arg_value(a, "--num-queries="));
        else if (starts_with(a, "--min-width="))
            min_w = std::stoull(arg_value(a, "--min-width="));
        else if (starts_with(a, "--max-width="))
            max_w = std::stoull(arg_value(a, "--max-width="));
        else if (starts_with(a, "--seed="))
            seed = std::stoull(arg_value(a, "--seed="));
        else { usage(); return 2; }
    }
    if (out.empty() || num_bitmaps == 0 || num_queries == 0
        || min_w < 1 || max_w < min_w || max_w > num_bitmaps) {
        usage();
        return 2;
    }

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> width_d(min_w, max_w);
    std::bernoulli_distribution op_d(0.5);

    std::vector<size_t> pool(num_bitmaps);
    for (size_t j = 0; j < num_bitmaps; ++j) pool[j] = j;

    std::vector<bench::Query> queries;
    queries.reserve(num_queries);
    for (size_t i = 0; i < num_queries; ++i) {
        bench::Query q;
        q.op = op_d(rng) ? bench::QueryOp::And : bench::QueryOp::Or;
        size_t w = width_d(rng);
        for (size_t k = 0; k < w; ++k) {
            std::uniform_int_distribution<size_t> d(k, pool.size() - 1);
            std::swap(pool[k], pool[d(rng)]);
            q.ids.push_back(pool[k]);
        }
        queries.push_back(std::move(q));
    }
    bench::write_queries(out, queries);
    std::cout << "wrote " << num_queries << " queries to " << out << "\n";
    return 0;
}
