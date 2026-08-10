#pragma once
#include "generated-proto/marketdata.pb.h"

class Strategy {
public:
    virtual ~Strategy() = default;
    virtual void onMarketData(const market::MarketDataEvent& event) = 0;
    virtual void onTick() = 0;
    virtual int tickIntervalMs() const { return 1000; }
};
