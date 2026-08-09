#include <future>
#include <memory>

#include <grpcpp/grpcpp.h>
#include "generated-proto/trading.pb.h"
#include "generated-proto/trading.grpc.pb.h"

#include "exchange/OrderProcessor/repo/mpscqueue.h"
#include "exchange/OrderProcessor/dto/orderDTO.h"
#include "exchange/OrderProcessor/dto/orderStatusDTO.h"
#include "exchange/OrderProcessor/engine/queueMessage.h"
#include "exchange/OrderProcessor/engine/matchingEngine.h"
#include "exchange/OrderProcessor/service/orderIdGenerator.h"

class ExchangeServiceImpl final : public market::SubmitOrder::Service, public market::CancelOrder::Service {
public:
    explicit ExchangeServiceImpl(MPSCQueue<QueueMessage>& queue) : _queue(queue) {}

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

private:
    MPSCQueue<QueueMessage>& _queue;
};

int main() {
    MPSCQueue<QueueMessage> queue;
    MatchingEngine engine(queue);
    engine.start();

    ExchangeServiceImpl service(queue);

    std::string serverAddress("0.0.0.0:50051");
    grpc::ServerBuilder builder;
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(static_cast<market::SubmitOrder::Service*>(&service));
    builder.RegisterService(static_cast<market::CancelOrder::Service*>(&service));

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    server->Wait();

    engine.stop();
    return 0;
}
