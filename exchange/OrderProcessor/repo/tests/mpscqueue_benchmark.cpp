#include "exchange/OrderProcessor/repo/mpscqueue.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

static double elapsedSeconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double>(end - start).count();
}

static void benchmarkSingleThread() {
    MPSCQueue<int64_t> queue;
    constexpr int64_t count = 2'000'000;

    auto start = Clock::now();
    for (int64_t i = 0; i < count; ++i) {
        queue.enqueue(i);
    }
    for (int64_t i = 0; i < count; ++i) {
        int64_t v;
        queue.dequeue(v);
    }
    auto end = Clock::now();

    double secs = elapsedSeconds(start, end);
    double totalOps = static_cast<double>(count) * 2.0;
    std::printf("[mpscqueue] single-thread enqueue+dequeue: %.0f ops in %.4fs -> %.0f ops/sec\n",
                totalOps, secs, totalOps / secs);
}

static void benchmarkMultiProducer() {
    MPSCQueue<int64_t> queue;
    constexpr int producers = 4;
    constexpr int64_t perProducer = 500'000;
    constexpr int64_t total = producers * perProducer;

    std::atomic<bool> startFlag{false};
    std::vector<std::thread> producerThreads;
    for (int p = 0; p < producers; ++p) {
        producerThreads.emplace_back([&queue, &startFlag, p]() {
            while (!startFlag.load(std::memory_order_acquire)) {
            }
            int64_t base = static_cast<int64_t>(p) * perProducer;
            for (int64_t i = 0; i < perProducer; ++i) {
                queue.enqueue(base + i);
            }
        });
    }

    int64_t received = 0;
    auto start = Clock::now();
    startFlag.store(true, std::memory_order_release);
    while (received < total) {
        int64_t v;
        if (queue.dequeue(v)) {
            ++received;
        }
    }
    auto end = Clock::now();

    for (auto& t : producerThreads) {
        t.join();
    }

    double secs = elapsedSeconds(start, end);
    std::printf("[mpscqueue] 4 producers -> 1 consumer: %lld ops in %.4fs -> %.0f ops/sec\n",
                static_cast<long long>(total), secs, static_cast<double>(total) / secs);
}

int main() {
    benchmarkSingleThread();
    benchmarkMultiProducer();
    return 0;
}
