#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "generated-proto/trading.pb.h"
#include "generated-proto/trading.grpc.pb.h"
#include "generated-proto/marketdata.pb.h"
#include "generated-proto/marketdata.grpc.pb.h"
#include "generated-proto/telemetry.pb.h"
#include "generated-proto/telemetry.grpc.pb.h"

#include "exchange/OrderProcessor/repo/mpscqueue.h"
#include "exchange/OrderProcessor/dto/orderDTO.h"
#include "exchange/OrderProcessor/dto/orderStatusDTO.h"
#include "exchange/OrderProcessor/dto/marketEventDTO.h"
#include "exchange/OrderProcessor/engine/queueMessage.h"
#include "exchange/OrderProcessor/engine/matchingEngine.h"
#include "exchange/OrderProcessor/engine/marketDataPublisher.h"
#include "exchange/OrderProcessor/engine/telemetry.h"
#include "exchange/OrderProcessor/dto/orderSideDTO.h"
#include "exchange/OrderProcessor/service/orderIdGenerator.h"
#include "exchange/OrderProcessor/service/capnpFeedServer.h"

class ExchangeServiceImpl final
    : public market::SubmitOrder::Service,
      public market::CancelOrder::Service,
      public market::MarketData::Service,
      public market::Telemetry::Service {
public:
    ExchangeServiceImpl(MPSCQueue<QueueMessage>& queue, MarketDataPublisher& publisher, TelemetryPublisher& telemetry)
        : _queue(queue), _publisher(publisher), _telemetry(telemetry) {}

    ::grpc::Status SubmitOrder(::grpc::ServerContext* context, const ::market::SubmitOrderRequest* request, ::market::SubmitOrderResponse* response) override {
        const int64_t tRpcIn = telemetryNowMicros();
        Order order = OrderDTO::protoToOrder(request);
        order.setOrderId(OrderIdGenerator::next());
        order.setNodeId(request->node_id());

        QueueMessage msg;
        msg.payload = SubmitPayload{std::move(order)};
        msg.resultPromise = std::make_shared<std::promise<EngineResult>>();
        std::future<EngineResult> future = msg.resultPromise->get_future();

        msg.tRpcIn = tRpcIn;
        msg.tEnqueue = telemetryNowMicros();
        _queue.enqueue(msg);
        EngineResult result = future.get();

        response->set_order_status(OrderStatusDTO::orderStatusToProto(result.status));
        response->set_order_id(result.orderId);
        response->set_quantity(result.filledQuantity);
        response->set_avg_price(result.avgPrice);
        response->set_timestamp(result.timestamp);
        response->set_rejection_reason(result.rejectionReason);
        return grpc::Status::OK;
    }

    ::grpc::Status CancelOrder(::grpc::ServerContext* context, const ::market::CancelOrderRequest* request, ::market::CancelOrderResponse* response) override {
        const int64_t tRpcIn = telemetryNowMicros();
        QueueMessage msg;
        msg.payload = CancelPayload{request->order_id(), request->node_id()};
        msg.resultPromise = std::make_shared<std::promise<EngineResult>>();
        std::future<EngineResult> future = msg.resultPromise->get_future();

        msg.tRpcIn = tRpcIn;
        msg.tEnqueue = telemetryNowMicros();
        _queue.enqueue(msg);
        EngineResult result = future.get();

        response->set_successful(result.cancelSuccessful);
        return grpc::Status::OK;
    }

    ::grpc::Status Subscribe(::grpc::ServerContext* context, const ::market::SubscribeRequest* request,
                              ::grpc::ServerWriter<::market::MarketDataEvent>* writer) override {
        auto queue = std::make_shared<MPSCQueue<MarketEvent>>();
        auto subId = _publisher.subscribe(queue);

        MarketEvent event;
        while (!context->IsCancelled()) {
            if (queue->dequeue(event)) {
                if (!writer->Write(MarketEventDTO::marketEventToProto(event))) {
                    break;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        _publisher.unsubscribe(subId);
        return grpc::Status::OK;
    }

    ::grpc::Status Subscribe(::grpc::ServerContext* context, const ::market::SubscribeRequest* request,
                              ::grpc::ServerWriter<::market::TelemetryBatch>* writer) override {
        auto queue = std::make_shared<TelemetryQueue>();
        auto subId = _telemetry.subscribe(queue);

        std::shared_ptr<const TelemetryRecord> record;
        while (!context->IsCancelled()) {
            if (queue->dequeue(record)) {
                if (!writer->Write(toProto(*record))) {
                    break;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        _telemetry.unsubscribe(subId);
        return grpc::Status::OK;
    }

private:
    static void fillLevels(const std::vector<DepthLevel>& in, ::google::protobuf::RepeatedPtrField<::market::BookLevel>* out) {
        for (const auto& l : in) {
            auto* p = out->Add();
            p->set_price(l.price);
            p->set_quantity(l.quantity);
            p->set_orders(l.orders);
        }
    }

    static ::market::TelemetryBatch toProto(const TelemetryRecord& r) {
        ::market::TelemetryBatch b;
        b.set_timestamp(r.timestamp);

        auto* o = b.mutable_order();
        o->set_op(r.isCancel ? ::market::OP_CANCEL : ::market::OP_SUBMIT);
        o->set_order_id(r.orderId);
        o->set_node_id(r.nodeId);
        o->set_side(OrderSideDTO::orderSideToProto(r.side));
        o->set_type(static_cast<::market::OrderType>(static_cast<int>(r.type)));
        o->set_price(r.price);
        o->set_quantity(r.quantity);
        o->set_status(OrderStatusDTO::orderStatusToProto(r.result.status));
        o->set_filled_quantity(r.result.filledQuantity);
        o->set_avg_price(r.result.avgPrice);
        o->set_rejection_reason(r.result.rejectionReason);
        o->set_cancel_successful(r.result.cancelSuccessful);

        auto* t = b.mutable_trace();
        t->set_seq(r.seq);
        t->set_t_rpc_in(r.tRpcIn);
        t->set_t_enqueue(r.tEnqueue);
        t->set_t_dequeue(r.tDequeue);
        t->set_t_match_done(r.tMatchDone);
        t->set_t_promise_set(r.tPromiseSet);
        t->set_t_publish(r.tPublish);

        for (const auto& e : r.events) {
            ::market::MarketDataEvent proto = MarketEventDTO::marketEventToProto(e);
            if (proto.has_trade()) {
                *b.add_trades() = proto.trade();
            } else if (proto.has_top_of_book()) {
                *b.mutable_top_of_book() = proto.top_of_book();
            }
        }

        fillLevels(r.depth.bids, b.mutable_book()->mutable_bids());
        fillLevels(r.depth.asks, b.mutable_book()->mutable_asks());
        return b;
    }

    MPSCQueue<QueueMessage>& _queue;
    MarketDataPublisher& _publisher;
    TelemetryPublisher& _telemetry;
};

int main() {
    MPSCQueue<QueueMessage> queue;
    MarketDataPublisher publisher;
    TelemetryPublisher telemetry;
    MatchingEngine engine(queue, publisher, telemetry);
    engine.start();

    ExchangeServiceImpl service(queue, publisher, telemetry);

    CapnpFeedServer feedServer(publisher, 50052);
    feedServer.start();

    std::string serverAddress("0.0.0.0:50051");
    grpc::ServerBuilder builder;
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(static_cast<market::SubmitOrder::Service*>(&service));
    builder.RegisterService(static_cast<market::CancelOrder::Service*>(&service));
    builder.RegisterService(static_cast<market::MarketData::Service*>(&service));
    builder.RegisterService(static_cast<market::Telemetry::Service*>(&service));

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();

    feedServer.stop();
    engine.stop();
    return 0;
}
