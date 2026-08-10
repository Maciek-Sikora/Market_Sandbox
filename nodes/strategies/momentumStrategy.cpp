#include "nodes/strategies/momentumStrategy.h"

MomentumStrategy::MomentumStrategy(ExchangeClient& client, const ParamMap& params)
    : _client(client),
      _window(static_cast<size_t>(getParamI(params, "mom-window", 20))),
      _thresholdPct(getParamD(params, "mom-threshold-pct", 0.001)),
      _size(getParamI(params, "mom-size", 5)) {}

void MomentumStrategy::onMarketData(const market::MarketDataEvent& event) {
    if (!event.has_trade()) {
        return;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    _prices.push_back(event.trade().price());
    while (_prices.size() > _window) {
        _prices.pop_front();
    }
}

void MomentumStrategy::onTick() {
    double front, back;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_prices.size() < _window) {
            return;
        }
        front = _prices.front();
        back = _prices.back();
    }

    if (front == 0.0) {
        return;
    }
    double momentum = (back - front) / front;

    if (momentum > _thresholdPct) {
        _client.submitOrder(market::BID, market::MARKET, 0.0, _size);
    } else if (momentum < -_thresholdPct) {
        _client.submitOrder(market::ASK, market::MARKET, 0.0, _size);
    }
}
