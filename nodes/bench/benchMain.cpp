#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "nodes/common/exchangeClient.h"

using Clock = std::chrono::steady_clock;

static double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) {
        return 0.0;
    }
    size_t idx = static_cast<size_t>(p * static_cast<double>(sorted.size() - 1));
    return sorted[idx];
}

static void sequentialLatencyBenchmark(const std::string& server) {
    ExchangeClient client(server, "bench-seq", /*verbose=*/false);
    constexpr int warmup = 200;
    constexpr int count = 3000;

    for (int i = 0; i < warmup; ++i) {
        client.submitOrder(i % 2 == 0 ? market::BID : market::ASK, market::LIMIT,
                            100.0 + static_cast<double>(i % 20) * 0.01, 1);
    }

    std::vector<double> latenciesMs;
    latenciesMs.reserve(count);
    auto start = Clock::now();
    for (int i = 0; i < count; ++i) {
        auto t0 = Clock::now();
        client.submitOrder(i % 2 == 0 ? market::BID : market::ASK, market::LIMIT,
                            100.0 + static_cast<double>(i % 20) * 0.01, 1);
        auto t1 = Clock::now();
        latenciesMs.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    auto end = Clock::now();

    double totalSecs = std::chrono::duration<double>(end - start).count();
    std::sort(latenciesMs.begin(), latenciesMs.end());

    std::printf("[bench] sequential SubmitOrder: %d calls in %.3fs -> %.0f req/sec\n",
                count, totalSecs, static_cast<double>(count) / totalSecs);
    std::printf("[bench] latency ms: min=%.3f p50=%.3f p90=%.3f p99=%.3f max=%.3f\n",
                latenciesMs.front(), percentile(latenciesMs, 0.50),
                percentile(latenciesMs, 0.90), percentile(latenciesMs, 0.99),
                latenciesMs.back());
}

static void concurrentThroughputBenchmark(const std::string& server) {
    constexpr int threadsCount = 8;
    constexpr int perThread = 1500;

    std::vector<std::thread> workers;
    auto start = Clock::now();
    for (int t = 0; t < threadsCount; ++t) {
        workers.emplace_back([&server, t]() {
            ExchangeClient client(server, "bench-conc-" + std::to_string(t), /*verbose=*/false);
            for (int i = 0; i < perThread; ++i) {
                client.submitOrder((t + i) % 2 == 0 ? market::BID : market::ASK, market::LIMIT,
                                    100.0 + static_cast<double>(i % 20) * 0.01, 1);
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    auto end = Clock::now();

    double secs = std::chrono::duration<double>(end - start).count();
    int64_t total = static_cast<int64_t>(threadsCount) * perThread;
    std::printf("[bench] concurrent SubmitOrder: %d threads x %d calls = %lld total in %.3fs -> %.0f req/sec\n",
                threadsCount, perThread, static_cast<long long>(total), secs, static_cast<double>(total) / secs);
}

int main(int argc, char** argv) {
    std::string server = argc > 1 ? argv[1] : "localhost:50051";
    sequentialLatencyBenchmark(server);
    concurrentThroughputBenchmark(server);
    return 0;
}
