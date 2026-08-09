#pragma once
#include "exchange/OrderProcessor/entity/orderSide.h"
#include "exchange/OrderProcessor/entity/orderType.h"


class Order {
private:
    ORDER_SIDE _side;
    ORDER_TYPE _orderType;
    double _price;
public:
    Order(ORDER_SIDE side, ORDER_TYPE orderType, double price): _side(side), _orderType(orderType), _price(price) {}

    ORDER_SIDE getSide() const {
        return _side;
    }

    ORDER_TYPE getOrderType() const {
        return _orderType;
    }

    double getPrice() const {
        return _price;
    }
};