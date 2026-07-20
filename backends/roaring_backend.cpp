#include "backends/roaring_backend.hpp"

#include <stdexcept>

namespace bench {

namespace {

class RoaringResult : public QueryResult {
public:
    RoaringResult(roaring::Bitmap bm, uint64_t num_bits)
        : bitmap_(std::move(bm)), num_bits_(num_bits) {}

    uint64_t cardinality() const override { return bitmap_.cardinality(); }
    std::vector<uint64_t> to_words() const override {
        return bitmap_.to_words(num_bits_);
    }

private:
    roaring::Bitmap bitmap_;
    uint64_t num_bits_;
};

} // namespace

void RoaringBackend::build(const std::vector<std::vector<uint64_t>>& bitmaps,
                           uint64_t num_bits) {
    // Chunk keys are 16-bit, so the addressable universe is 2^32 bits.
    if (num_bits > (1ULL << 32))
        throw std::runtime_error("roaring backend: num_bits exceeds 2^32");
    num_bits_ = num_bits;
    bitmaps_.clear();
    bitmaps_.reserve(bitmaps.size());
    for (const auto& raw : bitmaps)
        bitmaps_.push_back(roaring::Bitmap::from_words(raw, num_bits));
}

QueryResultPtr RoaringBackend::query_and(const std::vector<size_t>& ids) const {
    roaring::Bitmap acc = bitmaps_[ids[0]];
    for (size_t k = 1; k < ids.size(); ++k)
        acc = roaring::bitmap_and(acc, bitmaps_[ids[k]]);
    return std::make_unique<RoaringResult>(std::move(acc), num_bits_);
}

QueryResultPtr RoaringBackend::query_or(const std::vector<size_t>& ids) const {
    roaring::Bitmap acc = bitmaps_[ids[0]];
    for (size_t k = 1; k < ids.size(); ++k)
        acc = roaring::bitmap_or(acc, bitmaps_[ids[k]]);
    return std::make_unique<RoaringResult>(std::move(acc), num_bits_);
}

size_t RoaringBackend::serialized_bytes() const {
    size_t bytes = 0;
    for (const auto& bm : bitmaps_) bytes += bm.serialized_bytes();
    return bytes;
}

} // namespace bench
