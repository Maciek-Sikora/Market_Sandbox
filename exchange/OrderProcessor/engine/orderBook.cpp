#include "exchange/OrderProcessor/engine/orderBook.h"

#include <algorithm>
#include <iterator>

#include "exchange/OrderProcessor/entity/orderStatus.h"
#include "exchange/OrderProcessor/entity/orderType.h"
#include "exchange/OrderProcessor/engine/tradeIdGenerator.h"

void OrderBook::restBid(const Order& order) {
    auto& level = _bids[order.getPrice()];
    level.push_back(order);
    auto it = std::prev(level.end());
    _orderIndex[order.getOrderId()] = OrderLocation{ORDER_SIDE::BID, order.getPrice(), it};
}

void OrderBook::restAsk(const Order& order) {
    auto& level = _asks[order.getPrice()];
    level.push_back(order);
    auto it = std::prev(level.end());
    _orderIndex[order.getOrderId()] = OrderLocation{ORDER_SIDE::ASK, order.getPrice(), it};
}

TopOfBook OrderBook::currentTopOfBook(uint64_t timestamp) const {
    TopOfBook tob;
    tob.timestamp = timestamp;

    if (!_bids.empty()) {
        int64_t qty = 0;
        for (const auto& o : _bids.begin()->second) {
            qty += o.getQuantity();
        }
        tob.bidPrice = _bids.begin()->first;
        tob.bidQuantity = qty;
    }

    if (!_asks.empty()) {
        int64_t qty = 0;
        for (const auto& o : _asks.begin()->second) {
            qty += o.getQuantity();
        }
        tob.askPrice = _asks.begin()->first;
        tob.askQuantity = qty;
    }

    return tob;
}

EngineResult OrderBook::submitBid(Order order, uint64_t timestamp, std::vector<MarketEvent>& events) {
    EngineResult result;
    result.timestamp = timestamp;
    result.orderId = order.getOrderId();

    int64_t incomingQty = order.getQuantity();
    int64_t filledQty = 0;
    double filledNotional = 0.0;

    auto levelIt = _asks.begin();
    while (levelIt != _asks.end() && filledQty < order.getQuantity()) {
        double levelPrice = levelIt->first;
        if (order.getOrderType() == ORDER_TYPE::LIMIT && levelPrice > order.getPrice()) {
            break;
        }

        std::list<Order>& resting = levelIt->second;
        auto restingIt = resting.begin();
        while (restingIt != resting.end() && filledQty < order.getQuantity()) {
            int64_t remaining = order.getQuantity() - filledQty;
            int64_t tradeQty = std::min(remaining, restingIt->getQuantity());

            filledQty += tradeQty;
            filledNotional += static_cast<double>(tradeQty) * levelPrice;
            restingIt->reduceQuantity(tradeQty);

            events.push_back(Trade{
                TradeIdGenerator::next(),
                levelPrice,
                tradeQty,
                order.getSide(),
                order.getOrderId(),
                restingIt->getOrderId(),
                timestamp
            });

            if (restingIt->getQuantity() == 0) {
                _orderIndex.erase(restingIt->getOrderId());
                restingIt = resting.erase(restingIt);
            } else {
                ++restingIt;
            }
        }

        if (resting.empty()) {
            levelIt = _asks.erase(levelIt);
        } else {
            ++levelIt;
        }
    }

    result.filledQuantity = filledQty;
    result.avgPrice = filledQty > 0 ? filledNotional / static_cast<double>(filledQty) : 0.0;

    if (order.getOrderType() == ORDER_TYPE::MARKET) {
        if (filledQty == incomingQty) {
            result.status = ORDER_STATUS::FILLED;
        } else {
            result.status = ORDER_STATUS::REJECTED;
            result.rejectionReason = "insufficient liquidity: filled " + std::to_string(filledQty) +
                                      " of " + std::to_string(incomingQty);
        }
        events.push_back(currentTopOfBook(timestamp));
        return result;
    }

    // LIMIT
    if (filledQty == incomingQty) {
        result.status = ORDER_STATUS::FILLED;
    } else {
        order.reduceQuantity(filledQty);
        restBid(order);
        result.status = (filledQty == 0) ? ORDER_STATUS::QUEUED : ORDER_STATUS::PARTIAL;
    }
    events.push_back(currentTopOfBook(timestamp));
    return result;
}

EngineResult OrderBook::submitAsk(Order order, uint64_t timestamp, std::vector<MarketEvent>& events) {
    EngineResult result;
    result.timestamp = timestamp;
    result.orderId = order.getOrderId();

    int64_t incomingQty = order.getQuantity();
    int64_t filledQty = 0;
    double filledNotional = 0.0;

    auto levelIt = _bids.begin();
    while (levelIt != _bids.end() && filledQty < order.getQuantity()) {
        double levelPrice = levelIt->first;
        if (order.getOrderType() == ORDER_TYPE::LIMIT && levelPrice < order.getPrice()) {
            break;
        }

        std::list<Order>& resting = levelIt->second;
        auto restingIt = resting.begin();
        while (restingIt != resting.end() && filledQty < order.getQuantity()) {
            int64_t remaining = order.getQuantity() - filledQty;
            int64_t tradeQty = std::min(remaining, restingIt->getQuantity());

            filledQty += tradeQty;
            filledNotional += static_cast<double>(tradeQty) * levelPrice;
            restingIt->reduceQuantity(tradeQty);

            events.push_back(Trade{
                TradeIdGenerator::next(),
                levelPrice,
                tradeQty,
                order.getSide(),
                order.getOrderId(),
                restingIt->getOrderId(),
                timestamp
            });

            if (restingIt->getQuantity() == 0) {
                _orderIndex.erase(restingIt->getOrderId());
                restingIt = resting.erase(restingIt);
            } else {
                ++restingIt;
            }
        }

        if (resting.empty()) {
            levelIt = _bids.erase(levelIt);
        } else {
            ++levelIt;
        }
    }

    result.filledQuantity = filledQty;
    result.avgPrice = filledQty > 0 ? filledNotional / static_cast<double>(filledQty) : 0.0;

    if (order.getOrderType() == ORDER_TYPE::MARKET) {
        if (filledQty == incomingQty) {
            result.status = ORDER_STATUS::FILLED;
        } else {
            result.status = ORDER_STATUS::REJECTED;
            result.rejectionReason = "insufficient liquidity: filled " + std::to_string(filledQty) +
                                      " of " + std::to_string(incomingQty);
        }
        events.push_back(currentTopOfBook(timestamp));
        return result;
    }

    // LIMIT
    if (filledQty == incomingQty) {
        result.status = ORDER_STATUS::FILLED;
    } else {
        order.reduceQuantity(filledQty);
        restAsk(order);
        result.status = (filledQty == 0) ? ORDER_STATUS::QUEUED : ORDER_STATUS::PARTIAL;
    }
    events.push_back(currentTopOfBook(timestamp));
    return result;
}

EngineResult OrderBook::submit(Order order, uint64_t timestamp, std::vector<MarketEvent>& events) {
    if (order.getSide() == ORDER_SIDE::BID) {
        return submitBid(std::move(order), timestamp, events);
    }
    return submitAsk(std::move(order), timestamp, events);
}

EngineResult OrderBook::cancel(const std::string& orderId, uint64_t timestamp, std::vector<MarketEvent>& events) {
    EngineResult result;
    result.timestamp = timestamp;
    result.orderId = orderId;

    auto found = _orderIndex.find(orderId);
    if (found == _orderIndex.end()) {
        result.cancelSuccessful = false;
        result.rejectionReason = "order not found: " + orderId;
        return result;
    }

    const OrderLocation& loc = found->second;
    if (loc.side == ORDER_SIDE::BID) {
        auto levelIt = _bids.find(loc.price);
        levelIt->second.erase(loc.it);
        if (levelIt->second.empty()) {
            _bids.erase(levelIt);
        }
    } else {
        auto levelIt = _asks.find(loc.price);
        levelIt->second.erase(loc.it);
        if (levelIt->second.empty()) {
            _asks.erase(levelIt);
        }
    }

    _orderIndex.erase(found);
    result.cancelSuccessful = true;
    events.push_back(currentTopOfBook(timestamp));
    return result;
}
