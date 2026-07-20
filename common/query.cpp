#include "common/query.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace bench {

std::vector<Query> read_queries(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<Query> queries;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string op_str;
        size_t n = 0;
        if (!(iss >> op_str >> n)) continue;
        Query q;
        if (op_str == "AND") q.op = QueryOp::And;
        else if (op_str == "OR") q.op = QueryOp::Or;
        else throw std::runtime_error("unknown op: " + op_str);
        q.ids.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            size_t id;
            if (!(iss >> id))
                throw std::runtime_error("bad query line: " + line);
            q.ids.push_back(id);
        }
        queries.push_back(std::move(q));
    }
    return queries;
}

void write_queries(const std::string& path, const std::vector<Query>& queries) {
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write " + path);
    for (const auto& q : queries) {
        out << query_op_str(q.op) << " " << q.ids.size();
        for (size_t id : q.ids) out << " " << id;
        out << "\n";
    }
}

} // namespace bench
