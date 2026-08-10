#include "nodes/strategies/replayStrategy.h"

#include <fstream>
#include <iostream>
#include <sstream>

ReplayStrategy::ReplayStrategy(ExchangeClient& client, const ParamMap& params)
    : _client(client),
      _speed(getParamD(params, "replay-speed", 10.0)),
      _size(getParamI(params, "replay-size", 5)),
      _loop(getParamB(params, "replay-loop", true)) {
    std::string path = getParam(params, "replay-file", "nodes/strategies/replay_data/sample_series.csv");
    loadCheckpoints(path);
    _startTime = std::chrono::steady_clock::now();
}

void ReplayStrategy::loadCheckpoints(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "ReplayStrategy: could not open " << path << std::endl;
        return;
    }

    std::string line;
    std::getline(file, line); // header
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        std::stringstream ss(line);
        std::string offsetStr, priceStr;
        std::getline(ss, offsetStr, ',');
        std::getline(ss, priceStr, ',');
        _checkpoints.emplace_back(std::stoull(offsetStr), std::stod(priceStr));
    }
}

void ReplayStrategy::onMarketData(const market::MarketDataEvent& event) {
    if (!event.has_trade()) {
        return;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    _lastPrice = event.trade().price();
}

void ReplayStrategy::onTick() {
    if (_checkpoints.empty()) {
        return;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - _startTime).count();
    double scaledElapsedMs = static_cast<double>(elapsed) * _speed;

    while (_nextIndex < _checkpoints.size() &&
           static_cast<double>(_checkpoints[_nextIndex].first) <= scaledElapsedMs) {
        double target = _checkpoints[_nextIndex].second;
        double current;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            current = _lastPrice.value_or(_checkpoints[0].second);
        }

        if (target > current) {
            _client.submitOrder(market::BID, market::LIMIT, target, _size);
        } else if (target < current) {
            _client.submitOrder(market::ASK, market::LIMIT, target, _size);
        }

        ++_nextIndex;
    }

    if (_nextIndex >= _checkpoints.size() && _loop) {
        _nextIndex = 0;
        _startTime = std::chrono::steady_clock::now();
    }
}
