#include "nodes/strategies/noiseTraderStrategy.h"

NoiseTraderStrategy::NoiseTraderStrategy(ExchangeClient& client, const ParamMap& params)
    : _client(client),
      _sizeMax(getParamI(params, "noise-size-max", 5)),
      _actionProb(getParamD(params, "noise-action-prob", 0.3)),
      _cancelProb(getParamD(params, "noise-cancel-prob", 0.2)),
      _priceJitterPct(getParamD(params, "noise-price-jitter-pct", 0.01)),
      _startPrice(getParamD(params, "start-price", 100.0)),
      _rng(std::random_device{}()) {}

void NoiseTraderStrategy::onMarketData(const market::MarketDataEvent& event) {
    if (!event.has_trade()) {
        return;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    _lastPrice = event.trade().price();
}

void NoiseTraderStrategy::onTick() {
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    double roll;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        roll = unit(_rng);
    }
    if (roll >= _actionProb) {
        return;
    }

    double cancelRoll;
    std::string toCancel;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        cancelRoll = unit(_rng);
        if (cancelRoll < _cancelProb && !_restingOrderIds.empty()) {
            std::uniform_int_distribution<size_t> pick(0, _restingOrderIds.size() - 1);
            size_t idx = pick(_rng);
            toCancel = _restingOrderIds[idx];
            _restingOrderIds.erase(_restingOrderIds.begin() + static_cast<long>(idx));
        }
    }

    if (!toCancel.empty()) {
        _client.cancelOrder(toCancel);
        return;
    }

    double basePrice;
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int64_t> qtyDist(1, _sizeMax);
    std::uniform_real_distribution<double> jitterDist(-_priceJitterPct, _priceJitterPct);
    std::uniform_real_distribution<double> typeDist(0.0, 1.0);

    market::OrderSide side;
    int64_t qty;
    double jitter;
    bool useMarket;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        basePrice = _lastPrice.value_or(_startPrice);
        side = sideDist(_rng) == 0 ? market::BID : market::ASK;
        qty = qtyDist(_rng);
        jitter = jitterDist(_rng);
        useMarket = typeDist(_rng) < 0.3;
    }

    double price = basePrice * (1.0 + jitter);
    SubmitResult result = useMarket
        ? _client.submitOrder(side, market::MARKET, 0.0, qty)
        : _client.submitOrder(side, market::LIMIT, price, qty);

    if (result.status == market::QUEUED || result.status == market::PARTIAL) {
        std::lock_guard<std::mutex> lock(_mutex);
        _restingOrderIds.push_back(result.orderId);
    }
}
