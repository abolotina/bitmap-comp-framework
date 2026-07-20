#pragma once
// Small helpers shared by the backends and the benchmark driver.
// A "raw bitmap" everywhere in this project is a std::vector<uint64_t> holding
// ceil(num_bits / 64) little-endian words; bit b lives in word b / 64 at
// position b % 64. Bits past num_bits in the last word are always zero.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bench {

inline size_t words_for_bits(uint64_t num_bits) {
    return static_cast<size_t>((num_bits + 63ULL) / 64ULL);
}

inline int popcount64(uint64_t w) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(w);
#else
    int c = 0;
    while (w) { w &= w - 1; ++c; }
    return c;
#endif
}

// Index of the lowest set bit. Undefined for w == 0, like the intrinsic.
inline int ctz64(uint64_t w) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(w);
#else
    int c = 0;
    while ((w & 1ULL) == 0) { w >>= 1; ++c; }
    return c;
#endif
}

inline bool test_bit(const std::vector<uint64_t>& words, uint64_t bit) {
    return (words[bit / 64] >> (bit % 64)) & 1ULL;
}

inline void set_bit(std::vector<uint64_t>& words, uint64_t bit) {
    words[bit / 64] |= (1ULL << (bit % 64));
}

// Zero the bits past num_bits in the final word. Every function that produces a
// raw bitmap must end with this, otherwise two representations of the same set
// can compare unequal during verification.
inline void mask_tail(std::vector<uint64_t>& words, uint64_t num_bits) {
    const size_t tail = static_cast<size_t>(num_bits % 64);
    if (tail != 0 && !words.empty())
        words.back() &= (1ULL << tail) - 1ULL;
}

inline uint64_t cardinality(const std::vector<uint64_t>& words) {
    uint64_t c = 0;
    for (uint64_t w : words) c += static_cast<uint64_t>(popcount64(w));
    return c;
}

} // namespace bench
