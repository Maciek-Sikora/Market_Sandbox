#pragma once
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "nodes/common/strategy.h"
#include "nodes/common/exchangeClient.h"
#include "nodes/common/params.h"

class ReplayStrategy : public Strategy {
public:
    ReplayStrategy(ExchangeClient& client, const ParamMap& params);

    void onMarketData(const market::MarketDataEvent& event) override;
    void onTick() override;

private:
    ExchangeClient& _client;
    double _speed;
    int64_t _size;
    bool _loop;

    std::vector<std::pair<uint64_t, double>> _checkpoints;
    size_t _nextIndex = 0;
    std::chrono::steady_clock::time_point _startTime;

    std::mutex _mutex;
    std::optional<double> _lastPrice;

    void loadCheckpoints(const std::string& path);
};
