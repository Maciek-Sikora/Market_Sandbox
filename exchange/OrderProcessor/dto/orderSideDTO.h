#pragma once
#include "exchange/OrderProcessor/entity/orderSide.h"
#include "generated-proto/order.pb.h"

class OrderSideDTO {
public:
    static ORDER_SIDE protoToOrderSide(::market::OrderSide side) {
        return static_cast<ORDER_SIDE>(side);
    }
};
