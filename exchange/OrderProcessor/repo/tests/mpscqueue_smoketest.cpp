#include "exchange/OrderProcessor/repo/mpscqueue.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include <unordered_set>
#include <mutex>

static void fail(const char* msg) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    std::exit(1);
}

static void testSingleThreadRoundTrip() {
    MPSCQueue<int64_t> queue;
    constexpr int64_t count = 5000; // > bufferSize, forces multiple buffer rollovers

    for (int64_t i = 0; i < count; ++i) {
        queue.enqueue(i);
    }

    for (int64_t i = 0; i < count; ++i) {
        int64_t data = -1;
        if (!queue.dequeue(data)) {
            fail("testSingleThreadRoundTrip: unexpected empty dequeue");
        }
        if (data != i) {
            fail("testSingleThreadRoundTrip: FIFO order violated");
        }
    }

    int64_t data = -1;
    if (queue.dequeue(data)) {
        fail("testSingleThreadRoundTrip: expected empty queue after draining all items");
    }

    std::printf("testSingleThreadRoundTrip OK\n");
}

static void testConcurrentProducersSingleConsumer() {
    MPSCQueue<int64_t> queue;
    constexpr int producers = 4;
    constexpr int64_t itemsPerProducer = 5000;
    constexpr int64_t total = producers * itemsPerProducer;

    std::vector<std::thread> producerThreads;
    for (int p = 0; p < producers; ++p) {
        producerThreads.emplace_back([&queue, p]() {
            int64_t base = static_cast<int64_t>(p) * itemsPerProducer;
            for (int64_t i = 0; i < itemsPerProducer; ++i) {
                queue.enqueue(base + i);
            }
        });
    }

    std::unordered_set<int64_t> seen;
    seen.reserve(total);
    int64_t received = 0;
    bool producersDone = false;
    std::vector<std::thread> joined;

    std::thread consumer([&]() {
        while (received < total) {
            int64_t data;
            if (queue.dequeue(data)) {
                if (!seen.insert(data).second) {
                    fail("testConcurrentProducersSingleConsumer: duplicate item received");
                }
                ++received;
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    });

    for (auto& t : producerThreads) {
        t.join();
    }
    consumer.join();

    if (received != total) {
        fail("testConcurrentProducersSingleConsumer: did not receive all items");
    }
    if (static_cast<int64_t>(seen.size()) != total) {
        fail("testConcurrentProducersSingleConsumer: seen-set size mismatch");
    }

    std::printf("testConcurrentProducersSingleConsumer OK\n");
}

int main() {
    testSingleThreadRoundTrip();
    testConcurrentProducersSingleConsumer();
    std::printf("ALL OK\n");
    return 0;
}
