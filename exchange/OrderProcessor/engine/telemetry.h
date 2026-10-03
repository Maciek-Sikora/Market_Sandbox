#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "exchange/OrderProcessor/entity/orderSide.h"
#include "exchange/OrderProcessor/entity/orderType.h"
#include "exchange/OrderProcessor/engine/engineResult.h"
#include "exchange/OrderProcessor/engine/marketEvent.h"
#include "exchange/OrderProcessor/repo/mpscqueue.h"



inline int64_t telemetryNowMicros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct DepthLevel {
    double price = 0.0;
    int64_t quantity = 0;
    uint32_t orders = 0;
};

struct BookDepth {
    std::vector<DepthLevel> bids;
    std::vector<DepthLevel> asks;
};

struct TelemetryRecord {
    uint64_t seq = 0;
    uint64_t timestamp = 0;
    bool isCancel = false;

    std::string orderId;
    std::string nodeId;
    ORDER_SIDE side = SIDE_UNSPECIFIED;
    ORDER_TYPE type = ORDER_TYPE_UNSPECIFIED;
    double price = 0.0;
    int64_t quantity = 0;
    EngineResult result;

    int64_t tRpcIn = 0;
    int64_t tEnqueue = 0;
    int64_t tDequeue = 0;
    int64_t tMatchDone = 0;
    int64_t tPromiseSet = 0;
    int64_t tPublish = 0;

    std::vector<MarketEvent> events;
    BookDepth depth;
};

using TelemetryQueue = MPSCQueue<std::shared_ptr<const TelemetryRecord>>;

class TelemetryPublisher {
public:
    using SubscriberId = uint64_t;

    SubscriberId subscribe(std::shared_ptr<TelemetryQueue> queue) {
        std::lock_guard<std::mutex> lock(_mutex);
        SubscriberId id = _nextId++;
        _subscribers[id] = std::move(queue);
        _count.store(_subscribers.size(), std::memory_order_relaxed);
        return id;
    }

    void unsubscribe(SubscriberId id) {
        std::lock_guard<std::mutex> lock(_mutex);
        _subscribers.erase(id);
        _count.store(_subscribers.size(), std::memory_order_relaxed);
    }

    bool hasSubscribers() const {
        return _count.load(std::memory_order_relaxed) != 0;
    }

    void publish(std::shared_ptr<const TelemetryRecord> record) {
        std::lock_guard<std::mutex> lock(_mutex);
        for (const auto& [id, queue] : _subscribers) {
            queue->enqueue(record);
        }
    }

private:
    std::mutex _mutex;
    std::unordered_map<SubscriberId, std::shared_ptr<TelemetryQueue>> _subscribers;
    std::atomic<size_t> _count{0};
    SubscriberId _nextId = 1;
};
