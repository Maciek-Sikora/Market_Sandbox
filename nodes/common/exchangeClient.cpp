#include "nodes/common/exchangeClient.h"

#include <iostream>

namespace {
const char* statusName(market::OrderStatus status) {
    switch (status) {
        case market::FILLED: return "FILLED";
        case market::PARTIAL: return "PARTIAL";
        case market::QUEUED: return "QUEUED";
        case market::REJECTED: return "REJECTED";
        default: return "UNSPECIFIED";
    }
}
}

ExchangeClient::ExchangeClient(const std::string& serverAddress, std::string nodeId, bool verbose)
    : _nodeId(std::move(nodeId)), _verbose(verbose) {
    _channel = grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials());
    _submitStub = market::SubmitOrder::NewStub(_channel);
    _cancelStub = market::CancelOrder::NewStub(_channel);
    _marketDataStub = market::MarketData::NewStub(_channel);
}

ExchangeClient::~ExchangeClient() {
    stop();
}

SubmitResult ExchangeClient::submitOrder(market::OrderSide side, market::OrderType type, double price, int64_t quantity) {
    market::SubmitOrderRequest request;
    request.set_node_id(_nodeId);
    market::Order* order = request.mutable_order();
    order->set_orderside(side);
    order->set_ordertype(type);
    order->set_price(price);
    order->set_quantity(quantity);

    market::SubmitOrderResponse response;
    grpc::ClientContext context;
    grpc::Status status = _submitStub->SubmitOrder(&context, request, &response);

    SubmitResult result;
    if (!status.ok()) {
        result.status = market::ORDER_STATUS_UNSPECIFIED;
        result.rejectionReason = status.error_message();
        return result;
    }

    result.status = response.order_status();
    result.orderId = response.order_id();
    result.quantity = response.quantity();
    result.avgPrice = response.avg_price();
    result.timestamp = response.timestamp();
    result.rejectionReason = response.rejection_reason();

    if (_verbose) {
        std::cout << "[" << _nodeId << "] SUBMIT " << (side == market::BID ? "BID" : "ASK")
                  << " " << (type == market::MARKET ? "MARKET" : "LIMIT")
                  << " qty=" << quantity << " price=" << price
                  << " -> " << statusName(result.status)
                  << " filled=" << result.quantity << " avg=" << result.avgPrice;
        if (!result.rejectionReason.empty()) {
            std::cout << " reason=" << result.rejectionReason;
        }
        std::cout << std::endl;
    }

    return result;
}

bool ExchangeClient::cancelOrder(const std::string& orderId) {
    market::CancelOrderRequest request;
    request.set_node_id(_nodeId);
    request.set_order_id(orderId);

    market::CancelOrderResponse response;
    grpc::ClientContext context;
    grpc::Status status = _cancelStub->CancelOrder(&context, request, &response);
    bool successful = status.ok() && response.successful();
    if (_verbose) {
        std::cout << "[" << _nodeId << "] CANCEL " << orderId << " -> " << (successful ? "OK" : "FAILED") << std::endl;
    }
    return successful;
}

void ExchangeClient::startMarketDataStream(MarketDataCallback callback) {
    _streaming = true;
    _streamContext = std::make_unique<grpc::ClientContext>();
    _streamThread = std::thread(&ExchangeClient::streamLoop, this, std::move(callback));
}

void ExchangeClient::streamLoop(MarketDataCallback callback) {
    market::SubscribeRequest request;
    request.set_node_id(_nodeId);

    auto reader = _marketDataStub->Subscribe(_streamContext.get(), request);
    market::MarketDataEvent event;
    while (_streaming && reader->Read(&event)) {
        callback(event);
    }
    reader->Finish();
}

void ExchangeClient::stop() {
    if (!_streaming) {
        return;
    }
    _streaming = false;
    if (_streamContext) {
        _streamContext->TryCancel();
    }
    if (_streamThread.joinable()) {
        _streamThread.join();
    }
}
