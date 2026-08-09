cmake_minimum_required(VERSION 3.20)
project(MarketSandbox CXX)

list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(PROTO_OUT_DIR ${CMAKE_SOURCE_DIR}/generated-proto)
file(MAKE_DIRECTORY ${PROTO_OUT_DIR})

file(GLOB PROTO_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/proto/*.proto")

protobuf_generate(
    LANGUAGE cpp
    OUT_VAR PROTO_SRCS
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS ${CMAKE_SOURCE_DIR}/proto
    PROTOC_OUT_DIR ${PROTO_OUT_DIR}
)

protobuf_generate(
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
    OUT_VAR GRPC_SRCS
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS ${CMAKE_SOURCE_DIR}/proto
    PROTOC_OUT_DIR ${PROTO_OUT_DIR}
)

add_executable(exchangeService
    exchange/OrderProcessor/service/exchangeService.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_include_directories(exchangeService PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${PROTO_OUT_DIR}
)

target_link_libraries(exchangeService PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
)
