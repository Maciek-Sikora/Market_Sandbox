#pragma once
#include "exchange/OrderProcessor/entity/order.h"
#include "exchange/OrderProcessor/dto/orderSideDTO.h"
#include "exchange/OrderProcessor/dto/orderTypeDTO.h"
#include <grpcpp/grpcpp.h>
#include "generated-proto/trading.pb.h"
#include "generated-proto/order.pb.h"

class OrderDTO {
public:
    static Order protoToOrder(const ::market::SubmitOrderRequest* request) {
        Order order{
            OrderSideDTO::protoToOrderSide(request->order().orderside()),
            OrderTypeDTO::protoToOrderType(request->order().ordertype()),
            request->order().price(),
            request->order().quantity()
        };
        return order;
    }
};
