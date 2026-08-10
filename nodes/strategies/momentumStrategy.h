#pragma once
#include <deque>
#include <mutex>

#include "nodes/common/strategy.h"
#include "nodes/common/exchangeClient.h"
#include "nodes/common/params.h"

class MomentumStrategy : public Strategy {
public:
    MomentumStrategy(ExchangeClient& client, const ParamMap& params);

    void onMarketData(const market::MarketDataEvent& event) override;
    void onTick() override;
    int tickIntervalMs() const override { return 750; }

private:
    ExchangeClient& _client;
    size_t _window;
    double _thresholdPct;
    int64_t _size;

    std::mutex _mutex;
    std::deque<double> _prices;
};
