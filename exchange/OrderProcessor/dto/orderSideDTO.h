#pragma once
#include "exchange/OrderProcessor/entity/orderSide.h"
#include "generated-proto/order.pb.h"

class OrderSideDTO {
public:
    static ORDER_SIDE protoToOrderSide(::market::OrderSide side) {
        return static_cast<ORDER_SIDE>(side);
    }

    static ::market::OrderSide orderSideToProto(ORDER_SIDE side) {
        return static_cast<::market::OrderSide>(side);
    }
};
