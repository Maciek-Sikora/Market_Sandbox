#pragma once
#include <future>
#include <memory>
#include <string>
#include <variant>
#include "exchange/OrderProcessor/entity/order.h"
#include "exchange/OrderProcessor/engine/engineResult.h"

struct SubmitPayload {
    Order order;
};

struct CancelPayload {
    std::string orderId;
    std::string nodeId;
};

struct QueueMessage {
    std::variant<std::monostate, SubmitPayload, CancelPayload> payload;
    std::shared_ptr<std::promise<EngineResult>> resultPromise;
};
