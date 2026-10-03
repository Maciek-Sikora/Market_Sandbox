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
    const bool telemetryOn = _telemetry.hasSubscribers();
    const int64_t tDequeue = telemetryOn ? telemetryNowMicros() : 0;

    EngineResult result;
    result.timestamp = nowMillis();
    std::vector<MarketEvent> events;

    std::shared_ptr<TelemetryRecord> record;
    if (telemetryOn) {
        record = std::make_shared<TelemetryRecord>();
        record->seq = ++_seq;
        record->timestamp = result.timestamp;
        record->tRpcIn = msg.tRpcIn;
        record->tEnqueue = msg.tEnqueue;
        record->tDequeue = tDequeue;
    }

    if (auto* submit = std::get_if<SubmitPayload>(&msg.payload)) {
        if (record) {
            const Order& o = submit->order;
            record->isCancel = false;
            record->orderId = o.getOrderId();
            record->nodeId = o.getNodeId();
            record->side = o.getSide();
            record->type = o.getOrderType();
            record->price = o.getPrice();
            record->quantity = o.getQuantity();
        }
        result = _book.submit(std::move(submit->order), result.timestamp, events);
    } else if (auto* cancel = std::get_if<CancelPayload>(&msg.payload)) {
        if (record) {
            record->isCancel = true;
            record->orderId = cancel->orderId;
            record->nodeId = cancel->nodeId;
        }
        result = _book.cancel(cancel->orderId, result.timestamp, events);
    }
    const int64_t tMatchDone = telemetryOn ? telemetryNowMicros() : 0;

    if (msg.resultPromise) {
        msg.resultPromise->set_value(result);
    }
    const int64_t tPromiseSet = telemetryOn ? telemetryNowMicros() : 0;

    if (!events.empty()) {
        _publisher.publish(events);
    }

    if (record) {
        record->tMatchDone = tMatchDone;
        record->tPromiseSet = tPromiseSet;
        record->tPublish = telemetryNowMicros();
        record->result = std::move(result);
        record->events = std::move(events);
        record->depth = _book.depth(10);
        _telemetry.publish(std::move(record));
    }
}

uint64_t MatchingEngine::nowMillis() {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}
