#include "backends/raw_backend.hpp"

#include "common/bits.hpp"

namespace bench {

namespace {

// A result that is already in raw form; to_words() is then free.
class RawResult : public QueryResult {
public:
    explicit RawResult(std::vector<uint64_t> words) : words_(std::move(words)) {}
    uint64_t cardinality() const override { return bench::cardinality(words_); }
    std::vector<uint64_t> to_words() const override { return words_; }

private:
    std::vector<uint64_t> words_;
};

} // namespace

void RawBackend::build(const std::vector<std::vector<uint64_t>>& bitmaps,
                       uint64_t num_bits) {
    bitmaps_ = bitmaps;
    num_bits_ = num_bits;
    num_words_ = words_for_bits(num_bits);
}

QueryResultPtr RawBackend::query_and(const std::vector<size_t>& ids) const {
    std::vector<uint64_t> result = bitmaps_[ids[0]];
    for (size_t k = 1; k < ids.size(); ++k) {
        const auto& b = bitmaps_[ids[k]];
        for (size_t i = 0; i < num_words_; ++i) result[i] &= b[i];
    }
    mask_tail(result, num_bits_);
    return std::make_unique<RawResult>(std::move(result));
}

QueryResultPtr RawBackend::query_or(const std::vector<size_t>& ids) const {
    std::vector<uint64_t> result(num_words_, 0);
    for (size_t id : ids) {
        const auto& b = bitmaps_[id];
        for (size_t i = 0; i < num_words_; ++i) result[i] |= b[i];
    }
    mask_tail(result, num_bits_);
    return std::make_unique<RawResult>(std::move(result));
}

// Exactly what the .bin files on disk hold: one packed word array per bitmap.
size_t RawBackend::serialized_bytes() const {
    return bitmaps_.size() * num_words_ * sizeof(uint64_t);
}

} // namespace bench
