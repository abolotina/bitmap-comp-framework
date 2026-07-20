#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace bench {

enum class QueryOp { And, Or };

struct Query {
    QueryOp op;
    std::vector<size_t> ids;
};

inline size_t query_width(const Query& q) { return q.ids.size(); }

inline const char* query_op_str(QueryOp op) {
    return op == QueryOp::And ? "AND" : "OR";
}

std::vector<Query> read_queries(const std::string& path);
void write_queries(const std::string& path, const std::vector<Query>& queries);

} // namespace bench
