#pragma once
#include <chrono>
#include <cstdint>

namespace bench {

class Timer {
public:
    Timer() : start_(clock::now()) {}
    void reset() { start_ = clock::now(); }
    int64_t elapsed_ns() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   clock::now() - start_).count();
    }
    double elapsed_ms() const { return static_cast<double>(elapsed_ns()) / 1e6; }
private:
    using clock = std::chrono::steady_clock;
    std::chrono::time_point<clock> start_;
};

} // namespace bench
