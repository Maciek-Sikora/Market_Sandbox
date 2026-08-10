cmake_minimum_required(VERSION 3.20)
project(MarketSandbox CXX)

list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
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
    exchange/OrderProcessor/engine/orderBook.cpp
    exchange/OrderProcessor/engine/matchingEngine.cpp
    exchange/OrderProcessor/engine/marketDataPublisher.cpp
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
    Threads::Threads
)

add_executable(node
    nodes/common/nodeMain.cpp
    nodes/common/exchangeClient.cpp
    nodes/strategies/marketMakerStrategy.cpp
    nodes/strategies/momentumStrategy.cpp
    nodes/strategies/meanReversionStrategy.cpp
    nodes/strategies/noiseTraderStrategy.cpp
    nodes/strategies/replayStrategy.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_include_directories(node PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${PROTO_OUT_DIR}
)

target_link_libraries(node PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)

option(BUILD_MPSCQUEUE_SMOKETEST "Build standalone MPSCQueue smoke test" OFF)
if(BUILD_MPSCQUEUE_SMOKETEST)
    add_executable(mpscqueue_smoketest exchange/OrderProcessor/repo/tests/mpscqueue_smoketest.cpp)
    target_include_directories(mpscqueue_smoketest PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(mpscqueue_smoketest PRIVATE Threads::Threads)
endif()

option(BUILD_BENCHMARKS "Build standalone benchmark executables" OFF)
if(BUILD_BENCHMARKS)
    add_executable(mpscqueue_benchmark exchange/OrderProcessor/repo/tests/mpscqueue_benchmark.cpp)
    target_include_directories(mpscqueue_benchmark PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(mpscqueue_benchmark PRIVATE Threads::Threads)

    add_executable(exchange_benchmark
        nodes/bench/benchMain.cpp
        nodes/common/exchangeClient.cpp
        ${PROTO_SRCS}
        ${GRPC_SRCS}
    )
    target_include_directories(exchange_benchmark PRIVATE ${CMAKE_SOURCE_DIR} ${PROTO_OUT_DIR})
    target_link_libraries(exchange_benchmark PRIVATE gRPC::grpc++ protobuf::libprotobuf Threads::Threads)
endif()
