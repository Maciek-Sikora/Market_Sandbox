#pragma once
#include <atomic>
#include <cstdint>
#include <string>

class TradeIdGenerator {
    static inline std::atomic<uint64_t> _counter{0};
public:
    static std::string next() {
        uint64_t n = _counter.fetch_add(1, std::memory_order_relaxed);
        return "TRD-" + std::to_string(n);
    }
};
