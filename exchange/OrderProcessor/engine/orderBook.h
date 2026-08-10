#pragma once
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <unordered_map>

#include "exchange/OrderProcessor/entity/order.h"
#include "exchange/OrderProcessor/entity/orderSide.h"
#include "exchange/OrderProcessor/engine/engineResult.h"

class OrderBook {
public:
    EngineResult submit(Order order, uint64_t timestamp);
    EngineResult cancel(const std::string& orderId, uint64_t timestamp);

private:
    struct OrderLocation {
        ORDER_SIDE side;
        double price;
        std::list<Order>::iterator it;
    };

    std::map<double, std::list<Order>, std::greater<double>> _bids;
    std::map<double, std::list<Order>> _asks;
    std::unordered_map<std::string, OrderLocation> _orderIndex;

    void restBid(const Order& order);
    void restAsk(const Order& order);
    EngineResult submitBid(Order order, uint64_t timestamp);
    EngineResult submitAsk(Order order, uint64_t timestamp);
};
