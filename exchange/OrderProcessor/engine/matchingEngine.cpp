#include "exchange/OrderProcessor/engine/matchingEngine.h"

#include <chrono>
#include <vector>

void MatchingEngine::run() {
    QueueMessage msg;
    // Busy-spin: this is the only consumer standing between a producer's
    // enqueue and its blocked future.get(), so latency beats sparing a core.
    while (true) {
        if (_queue.dequeue(msg)) {
            process(msg);
        } else if (!_running.load()) {
            break;
        }
    }
}

void MatchingEngine::process(QueueMessage& msg) {
    EngineResult result;
    result.timestamp = nowMillis();
    std::vector<MarketEvent> events;

    if (auto* submit = std::get_if<SubmitPayload>(&msg.payload)) {
        result = _book.submit(std::move(submit->order), result.timestamp, events);
    } else if (auto* cancel = std::get_if<CancelPayload>(&msg.payload)) {
        result = _book.cancel(cancel->orderId, result.timestamp, events);
    }

    if (msg.resultPromise) {
        msg.resultPromise->set_value(result);
    }
    if (!events.empty()) {
        _publisher.publish(events);
    }
}

uint64_t MatchingEngine::nowMillis() {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}
