#pragma once
#include <mutex>
#include <optional>
#include <random>
#include <vector>

#include "nodes/common/strategy.h"
#include "nodes/common/exchangeClient.h"
#include "nodes/common/params.h"

class NoiseTraderStrategy : public Strategy {
public:
    NoiseTraderStrategy(ExchangeClient& client, const ParamMap& params);

    void onMarketData(const market::MarketDataEvent& event) override;
    void onTick() override;
    int tickIntervalMs() const override { return 300; }

private:
    ExchangeClient& _client;
    int64_t _sizeMax;
    double _actionProb;
    double _cancelProb;
    double _priceJitterPct;
    double _startPrice;

    std::mutex _mutex;
    std::optional<double> _lastPrice;
    std::vector<std::string> _restingOrderIds;
    std::mt19937 _rng;
};
