@0xe70cacf93345538e;

using Cxx = import "/capnp/c++.capnp";


enum OrderSide {
  unspecified @0;
  bid @1;
  ask @2;
}

struct Trade {
  tradeId @0 :Text;
  price @1 :Float64;
  quantity @2 :Int64;
  aggressorSide @3 :OrderSide;
  aggressorOrderId @4 :Text;
  restingOrderId @5 :Text;
  timestamp @6 :UInt64;
}

struct TopOfBook {
  bidPrice @0 :Float64;
  bidQuantity @1 :Int64;
  askPrice @2 :Float64;
  askQuantity @3 :Int64;
  timestamp @4 :UInt64;

  hasBid @5 :Bool;
  hasAsk @6 :Bool;
}

struct MarketDataEvent {
  union {
    trade @0 :Trade;
    topOfBook @1 :TopOfBook;
  }
}
