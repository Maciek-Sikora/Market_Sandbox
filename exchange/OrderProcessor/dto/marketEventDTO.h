#pragma once
#include <type_traits>
#include <variant>
#include "exchange/OrderProcessor/engine/marketEvent.h"
#include "exchange/OrderProcessor/dto/orderSideDTO.h"
#include "generated-proto/marketdata.pb.h"

class MarketEventDTO {
public:
    static ::market::MarketDataEvent marketEventToProto(const MarketEvent& event) {
        ::market::MarketDataEvent proto;
        std::visit([&proto](auto&& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, Trade>) {
                auto* t = proto.mutable_trade();
                t->set_trade_id(e.tradeId);
                t->set_price(e.price);
                t->set_quantity(e.quantity);
                t->set_aggressor_side(OrderSideDTO::orderSideToProto(e.aggressorSide));
                t->set_aggressor_order_id(e.aggressorOrderId);
                t->set_resting_order_id(e.restingOrderId);
                t->set_timestamp(e.timestamp);
            } else if constexpr (std::is_same_v<T, TopOfBook>) {
                auto* tob = proto.mutable_top_of_book();
                if (e.bidPrice) tob->set_bid_price(*e.bidPrice);
                if (e.bidQuantity) tob->set_bid_quantity(*e.bidQuantity);
                if (e.askPrice) tob->set_ask_price(*e.askPrice);
                if (e.askQuantity) tob->set_ask_quantity(*e.askQuantity);
                tob->set_timestamp(e.timestamp);
            }
        }, event);
        return proto;
    }
};
