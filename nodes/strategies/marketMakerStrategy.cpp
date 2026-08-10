#include "nodes/strategies/marketMakerStrategy.h"

#include <cmath>

MarketMakerStrategy::MarketMakerStrategy(ExchangeClient& client, const ParamMap& params)
    : _client(client),
      _spread(getParamD(params, "mm-spread", 0.50)),
      _size(getParamI(params, "mm-size", 10)),
      _requoteThresholdPct(getParamD(params, "mm-requote-threshold-pct", 0.002)),
      _startPrice(getParamD(params, "start-price", 100.0)) {}

void MarketMakerStrategy::onMarketData(const market::MarketDataEvent& event) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (event.has_trade()) {
        _lastTradePrice = event.trade().price();
    } else if (event.has_top_of_book()) {
        const auto& tob = event.top_of_book();
        _lastBid = tob.has_bid_price() ? std::optional<double>(tob.bid_price()) : std::nullopt;
        _lastAsk = tob.has_ask_price() ? std::optional<double>(tob.ask_price()) : std::nullopt;
    }
}

double MarketMakerStrategy::computeMid() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_lastBid && _lastAsk) {
        return (*_lastBid + *_lastAsk) / 2.0;
    }
    if (_lastTradePrice) {
        return *_lastTradePrice;
    }
    return _startPrice;
}

void MarketMakerStrategy::onTick() {
    double mid = computeMid();

    bool needsQuote;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        needsQuote = !_hasQuoted ||
            (_quotedMid > 0.0 && std::abs(mid - _quotedMid) / _quotedMid > _requoteThresholdPct);
    }

    if (needsQuote) {
        requote(mid);
    }
}

void MarketMakerStrategy::requote(double mid) {
    std::string oldBid, oldAsk;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        oldBid = _bidOrderId;
        oldAsk = _askOrderId;
    }
    if (!oldBid.empty()) {
        _client.cancelOrder(oldBid);
    }
    if (!oldAsk.empty()) {
        _client.cancelOrder(oldAsk);
    }

    double bidPrice = mid - _spread / 2.0;
    double askPrice = mid + _spread / 2.0;

    SubmitResult bidResult = _client.submitOrder(market::BID, market::LIMIT, bidPrice, _size);
    SubmitResult askResult = _client.submitOrder(market::ASK, market::LIMIT, askPrice, _size);

    std::lock_guard<std::mutex> lock(_mutex);
    _bidOrderId = (bidResult.status == market::QUEUED || bidResult.status == market::PARTIAL) ? bidResult.orderId : "";
    _askOrderId = (askResult.status == market::QUEUED || askResult.status == market::PARTIAL) ? askResult.orderId : "";
    _quotedMid = mid;
    _hasQuoted = true;
}
