#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include "nodes/common/exchangeClient.h"
#include "nodes/common/strategy.h"
#include "nodes/common/params.h"
#include "nodes/strategies/marketMakerStrategy.h"
#include "nodes/strategies/momentumStrategy.h"
#include "nodes/strategies/meanReversionStrategy.h"
#include "nodes/strategies/noiseTraderStrategy.h"
#include "nodes/strategies/replayStrategy.h"

ParamMap parseArgs(int argc, char** argv) {
    ParamMap args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--", 0) != 0) {
            continue;
        }
        arg = arg.substr(2);
        auto eq = arg.find('=');
        if (eq != std::string::npos) {
            args[arg.substr(0, eq)] = arg.substr(eq + 1);
        } else {
            args[arg] = "true";
        }
    }
    return args;
}

std::unique_ptr<Strategy> createStrategy(const std::string& name, ExchangeClient& client, const ParamMap& params) {
    if (name == "market-maker") return std::make_unique<MarketMakerStrategy>(client, params);
    if (name == "momentum") return std::make_unique<MomentumStrategy>(client, params);
    if (name == "mean-reversion") return std::make_unique<MeanReversionStrategy>(client, params);
    if (name == "noise") return std::make_unique<NoiseTraderStrategy>(client, params);
    if (name == "replay") return std::make_unique<ReplayStrategy>(client, params);
    return nullptr;
}

int main(int argc, char** argv) {
    ParamMap params = parseArgs(argc, argv);
    std::string server = getParam(params, "server", "localhost:50051");
    std::string nodeId = getParam(params, "node-id", "node-" + std::to_string(std::random_device{}()));
    std::string strategyName = getParam(params, "strategy", "");

    ExchangeClient client(server, nodeId);
    std::unique_ptr<Strategy> strategy = createStrategy(strategyName, client, params);
    if (!strategy) {
        std::cerr << "Unknown --strategy=" << strategyName << std::endl;
        return 1;
    }

    client.startMarketDataStream([&strategy](const market::MarketDataEvent& event) {
        strategy->onMarketData(event);
    });

    std::cout << "[" << nodeId << "] running strategy=" << strategyName << " against " << server << std::endl;

    while (true) {
        strategy->onTick();
        std::this_thread::sleep_for(std::chrono::milliseconds(strategy->tickIntervalMs()));
    }

    return 0;
}
