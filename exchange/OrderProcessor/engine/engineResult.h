#pragma once
#include <cstdint>
#include <string>
#include "exchange/OrderProcessor/entity/orderStatus.h"

struct EngineResult {
    ORDER_STATUS status = ORDER_STATUS_UNSPECIFIED;
    std::string orderId;
    int64_t filledQuantity = 0;
    double avgPrice = 0.0;
    uint64_t timestamp = 0;
    std::string rejectionReason;
    bool cancelSuccessful = false;
};
