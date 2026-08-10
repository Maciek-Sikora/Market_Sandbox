#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "exchange/OrderProcessor/repo/mpscqueue.h"
#include "exchange/OrderProcessor/engine/marketEvent.h"

class MarketDataPublisher {
public:
    using SubscriberId = uint64_t;

    SubscriberId subscribe(std::shared_ptr<MPSCQueue<MarketEvent>> queue);
    void unsubscribe(SubscriberId id);
    void publish(const std::vector<MarketEvent>& events);

private:
    std::mutex _mutex;
    std::unordered_map<SubscriberId, std::shared_ptr<MPSCQueue<MarketEvent>>> _subscribers;
    SubscriberId _nextId = 1;
};
