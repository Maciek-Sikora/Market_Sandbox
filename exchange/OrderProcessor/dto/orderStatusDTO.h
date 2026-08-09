#pragma once
#include "exchange/OrderProcessor/entity/orderStatus.h"
#include "generated-proto/order.pb.h"

class OrderStatusDTO {
public:
    static ::market::OrderStatus orderStatusToProto(ORDER_STATUS status) {
        return static_cast<::market::OrderStatus>(status);
    }

    static ORDER_STATUS protoToOrderStatus(::market::OrderStatus status) {
        return static_cast<ORDER_STATUS>(status);
    }
};
