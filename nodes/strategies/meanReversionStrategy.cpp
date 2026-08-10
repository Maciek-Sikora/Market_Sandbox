#include "nodes/strategies/meanReversionStrategy.h"

#include <numeric>

MeanReversionStrategy::MeanReversionStrategy(ExchangeClient& client, const ParamMap& params)
    : _client(client),
      _window(static_cast<size_t>(getParamI(params, "mr-window", 30))),
      _deviationPct(getParamD(params, "mr-deviation-pct", 0.01)),
      _size(getParamI(params, "mr-size", 5)) {}

void MeanReversionStrategy::onMarketData(const market::MarketDataEvent& event) {
    if (!event.has_trade()) {
        return;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    _prices.push_back(event.trade().price());
    while (_prices.size() > _window) {
        _prices.pop_front();
    }
}

void MeanReversionStrategy::onTick() {
    double sma, last;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_prices.size() < _window) {
            return;
        }
        double sum = std::accumulate(_prices.begin(), _prices.end(), 0.0);
        sma = sum / static_cast<double>(_prices.size());
        last = _prices.back();
    }

    if (sma == 0.0) {
        return;
    }

    if (last > sma * (1.0 + _deviationPct)) {
        _client.submitOrder(market::ASK, market::LIMIT, last, _size);
    } else if (last < sma * (1.0 - _deviationPct)) {
        _client.submitOrder(market::BID, market::LIMIT, last, _size);
    }
}
