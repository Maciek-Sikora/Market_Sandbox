#include <grpcpp/grpcpp.h>
#include "generated-proto/trading.pb.h"
#include "generated-proto/trading.grpc.pb.h"


class ExchangeServiceImpl final : public market::SubmitOrder::Service {
    ::grpc::Status SubmitOrder(::grpc::ServerContext* context, const ::market::SubmitOrderRequest* request, ::market::SubmitOrderResponse* response) override {
        request->node_id();
        return grpc::Status::OK;
    }
};