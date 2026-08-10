#pragma once
#include <mutex>
#include <optional>
#include <string>

#include "nodes/common/strategy.h"
#include "nodes/common/exchangeClient.h"
#include "nodes/common/params.h"

class MarketMakerStrategy : public Strategy {
public:
    MarketMakerStrategy(ExchangeClient& client, const ParamMap& params);

    void onMarketData(const market::MarketDataEvent& event) override;
    void onTick() override;

private:
    ExchangeClient& _client;
    double _spread;
    int64_t _size;
    double _requoteThresholdPct;
    double _startPrice;

    std::mutex _mutex;
    std::optional<double> _lastTradePrice;
    std::optional<double> _lastBid;
    std::optional<double> _lastAsk;
    std::string _bidOrderId;
    std::string _askOrderId;
    double _quotedMid = 0.0;
    bool _hasQuoted = false;

    double computeMid();
    void requote(double mid);
};
