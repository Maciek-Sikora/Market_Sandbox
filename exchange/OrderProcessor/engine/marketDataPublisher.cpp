#include "exchange/OrderProcessor/engine/marketDataPublisher.h"

MarketDataPublisher::SubscriberId MarketDataPublisher::subscribe(std::shared_ptr<MPSCQueue<MarketEvent>> queue) {
    std::lock_guard<std::mutex> lock(_mutex);
    SubscriberId id = _nextId++;
    _subscribers[id] = std::move(queue);
    return id;
}

void MarketDataPublisher::unsubscribe(SubscriberId id) {
    std::lock_guard<std::mutex> lock(_mutex);
    _subscribers.erase(id);
}

void MarketDataPublisher::publish(const std::vector<MarketEvent>& events) {
    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& [id, queue] : _subscribers) {
        for (const auto& event : events) {
            queue->enqueue(event);
        }
    }
}
