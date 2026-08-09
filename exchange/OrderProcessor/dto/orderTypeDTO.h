#pragma once
#include "exchange/OrderProcessor/entity/orderType.h"
#include "generated-proto/order.pb.h"

class OrderTypeDTO {
public:
    static ORDER_TYPE protoToOrderType(::market::OrderType type) {
        return static_cast<ORDER_TYPE>(type);
    }
};
