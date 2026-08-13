#pragma once
#include <string>
#include <type_traits>
#include <variant>

#include <capnp/message.h>

#include "exchange/OrderProcessor/entity/orderSide.h"
#include "exchange/OrderProcessor/engine/marketEvent.h"
#include "generated-capnp/marketdata.capnp.h"

class MarketEventCapnp {
public:
    static void write(const MarketEvent& event, mktfeed::MarketDataEvent::Builder builder) {
        std::visit([&builder](auto&& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, Trade>) {
                auto t = builder.initTrade();
                t.setTradeId(text(e.tradeId));
                t.setPrice(e.price);
                t.setQuantity(e.quantity);
                t.setAggressorSide(sideToCapnp(e.aggressorSide));
                t.setAggressorOrderId(text(e.aggressorOrderId));
                t.setRestingOrderId(text(e.restingOrderId));
                t.setTimestamp(e.timestamp);
            } else if constexpr (std::is_same_v<T, TopOfBook>) {
                auto tob = builder.initTopOfBook();
                tob.setHasBid(e.bidPrice.has_value());
                if (e.bidPrice) tob.setBidPrice(*e.bidPrice);
                if (e.bidQuantity) tob.setBidQuantity(*e.bidQuantity);
                tob.setHasAsk(e.askPrice.has_value());
                if (e.askPrice) tob.setAskPrice(*e.askPrice);
                if (e.askQuantity) tob.setAskQuantity(*e.askQuantity);
                tob.setTimestamp(e.timestamp);
            }
        }, event);
    }

private:
    static capnp::Text::Reader text(const std::string& s) {
        return capnp::Text::Reader(s.data(), s.size());
    }

    static mktfeed::OrderSide sideToCapnp(ORDER_SIDE side) {
        switch (side) {
            case BID: return mktfeed::OrderSide::BID;
            case ASK: return mktfeed::OrderSide::ASK;
            default:  return mktfeed::OrderSide::UNSPECIFIED;
        }
    }
};
