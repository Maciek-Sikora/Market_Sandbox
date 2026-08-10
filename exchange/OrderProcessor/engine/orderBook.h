#pragma once
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "exchange/OrderProcessor/entity/order.h"
#include "exchange/OrderProcessor/entity/orderSide.h"
#include "exchange/OrderProcessor/engine/engineResult.h"
#include "exchange/OrderProcessor/engine/marketEvent.h"

class OrderBook {
public:
    EngineResult submit(Order order, uint64_t timestamp, std::vector<MarketEvent>& events);
    EngineResult cancel(const std::string& orderId, uint64_t timestamp, std::vector<MarketEvent>& events);

private:
    struct OrderLocation {
        ORDER_SIDE side;
        double price;
        std::list<Order>::iterator it;
    };

    // best bid = begin() (highest price first)
    std::map<double, std::list<Order>, std::greater<double>> _bids;
    // best ask = begin() (lowest price first)
    std::map<double, std::list<Order>> _asks;
    std::unordered_map<std::string, OrderLocation> _orderIndex;

    void restBid(const Order& order);
    void restAsk(const Order& order);
    EngineResult submitBid(Order order, uint64_t timestamp, std::vector<MarketEvent>& events);
    EngineResult submitAsk(Order order, uint64_t timestamp, std::vector<MarketEvent>& events);
    TopOfBook currentTopOfBook(uint64_t timestamp) const;
};
