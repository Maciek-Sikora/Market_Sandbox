#include "exchange/OrderProcessor/engine/matchingEngine.h"

#include <chrono>

void MatchingEngine::run() {
    QueueMessage msg;
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
    if (auto* submit = std::get_if<SubmitPayload>(&msg.payload)) {
        result = _book.submit(std::move(submit->order), result.timestamp);
    } else if (auto* cancel = std::get_if<CancelPayload>(&msg.payload)) {
        result = _book.cancel(cancel->orderId, result.timestamp);
    }
    if (msg.resultPromise) {
        msg.resultPromise->set_value(result);
    }
}

uint64_t MatchingEngine::nowMillis() {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}
