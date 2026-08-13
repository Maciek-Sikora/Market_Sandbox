#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "generated-proto/trading.pb.h"
#include "generated-proto/trading.grpc.pb.h"
#include "generated-proto/marketdata.pb.h"
#include "generated-proto/marketdata.grpc.pb.h"

#include "exchange/OrderProcessor/repo/mpscqueue.h"
#include "exchange/OrderProcessor/dto/orderDTO.h"
#include "exchange/OrderProcessor/dto/orderStatusDTO.h"
#include "exchange/OrderProcessor/dto/marketEventDTO.h"
#include "exchange/OrderProcessor/engine/queueMessage.h"
#include "exchange/OrderProcessor/engine/matchingEngine.h"
#include "exchange/OrderProcessor/engine/marketDataPublisher.h"
#include "exchange/OrderProcessor/service/orderIdGenerator.h"
#include "exchange/OrderProcessor/service/capnpFeedServer.h"

class ExchangeServiceImpl final
    : public market::SubmitOrder::Service,
      public market::CancelOrder::Service,
      public market::MarketData::Service {
public:
    ExchangeServiceImpl(MPSCQueue<QueueMessage>& queue, MarketDataPublisher& publisher)
        : _queue(queue), _publisher(publisher) {}

    ::grpc::Status SubmitOrder(::grpc::ServerContext* context, const ::market::SubmitOrderRequest* request, ::market::SubmitOrderResponse* response) override {
        Order order = OrderDTO::protoToOrder(request);
        order.setOrderId(OrderIdGenerator::next());
        order.setNodeId(request->node_id());

        QueueMessage msg;
        msg.payload = SubmitPayload{std::move(order)};
        msg.resultPromise = std::make_shared<std::promise<EngineResult>>();
        std::future<EngineResult> future = msg.resultPromise->get_future();

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
        QueueMessage msg;
        msg.payload = CancelPayload{request->order_id(), request->node_id()};
        msg.resultPromise = std::make_shared<std::promise<EngineResult>>();
        std::future<EngineResult> future = msg.resultPromise->get_future();

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

private:
    MPSCQueue<QueueMessage>& _queue;
    MarketDataPublisher& _publisher;
};

int main() {
    MPSCQueue<QueueMessage> queue;
    MarketDataPublisher publisher;
    MatchingEngine engine(queue, publisher);
    engine.start();

    ExchangeServiceImpl service(queue, publisher);

    CapnpFeedServer feedServer(publisher, 50052);
    feedServer.start();

    std::string serverAddress("0.0.0.0:50051");
    grpc::ServerBuilder builder;
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(static_cast<market::SubmitOrder::Service*>(&service));
    builder.RegisterService(static_cast<market::CancelOrder::Service*>(&service));
    builder.RegisterService(static_cast<market::MarketData::Service*>(&service));

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();

    feedServer.stop();
    engine.stop();
    return 0;
}
