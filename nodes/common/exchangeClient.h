#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "generated-proto/trading.pb.h"
#include "generated-proto/trading.grpc.pb.h"
#include "generated-proto/marketdata.pb.h"
#include "generated-proto/marketdata.grpc.pb.h"

struct SubmitResult {
    market::OrderStatus status = market::ORDER_STATUS_UNSPECIFIED;
    std::string orderId;
    int64_t quantity = 0;
    double avgPrice = 0.0;
    uint64_t timestamp = 0;
    std::string rejectionReason;
};

class ExchangeClient {
public:
    ExchangeClient(const std::string& serverAddress, std::string nodeId, bool verbose = true);
    ~ExchangeClient();

    SubmitResult submitOrder(market::OrderSide side, market::OrderType type, double price, int64_t quantity);
    bool cancelOrder(const std::string& orderId);

    using MarketDataCallback = std::function<void(const market::MarketDataEvent&)>;
    void startMarketDataStream(MarketDataCallback callback);
    void stop();

private:
    std::string _nodeId;
    bool _verbose;
    std::shared_ptr<grpc::Channel> _channel;
    std::unique_ptr<market::SubmitOrder::Stub> _submitStub;
    std::unique_ptr<market::CancelOrder::Stub> _cancelStub;
    std::unique_ptr<market::MarketData::Stub> _marketDataStub;

    std::thread _streamThread;
    std::atomic<bool> _streaming{false};
    std::unique_ptr<grpc::ClientContext> _streamContext;

    void streamLoop(MarketDataCallback callback);
};
