#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include "exchange/OrderProcessor/entity/orderSide.h"

struct Trade {
    std::string tradeId;
    double price = 0.0;
    int64_t quantity = 0;
    ORDER_SIDE aggressorSide = SIDE_UNSPECIFIED;
    std::string aggressorOrderId;
    std::string restingOrderId;
    uint64_t timestamp = 0;
};

struct TopOfBook {
    std::optional<double> bidPrice;
    std::optional<int64_t> bidQuantity;
    std::optional<double> askPrice;
    std::optional<int64_t> askQuantity;
    uint64_t timestamp = 0;
};

using MarketEvent = std::variant<std::monostate, Trade, TopOfBook>;
