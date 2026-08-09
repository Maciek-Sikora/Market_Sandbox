#pragma once
#include <cstdint>
#include <string>
#include "exchange/OrderProcessor/entity/orderSide.h"
#include "exchange/OrderProcessor/entity/orderType.h"

class Order {
private:
    ORDER_SIDE _side;
    ORDER_TYPE _orderType;
    double _price;
    int64_t _quantity;
    std::string _orderId;
    std::string _nodeId;
public:
    Order() : _side(SIDE_UNSPECIFIED), _orderType(ORDER_TYPE_UNSPECIFIED), _price(0.0), _quantity(0) {}
    Order(ORDER_SIDE side, ORDER_TYPE orderType, double price, int64_t quantity)
        : _side(side), _orderType(orderType), _price(price), _quantity(quantity) {}

    ORDER_SIDE getSide() const {
        return _side;
    }

    ORDER_TYPE getOrderType() const {
        return _orderType;
    }

    double getPrice() const {
        return _price;
    }

    int64_t getQuantity() const {
        return _quantity;
    }

    const std::string& getOrderId() const {
        return _orderId;
    }

    const std::string& getNodeId() const {
        return _nodeId;
    }

    void setOrderId(std::string id) {
        _orderId = std::move(id);
    }

    void setNodeId(std::string id) {
        _nodeId = std::move(id);
    }

    void reduceQuantity(int64_t filled) {
        _quantity -= filled;
    }
};
