#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "exchange/OrderProcessor/engine/marketDataPublisher.h"

class CapnpFeedServer {
public:
    CapnpFeedServer(MarketDataPublisher& publisher, uint16_t port);
    ~CapnpFeedServer();

    CapnpFeedServer(const CapnpFeedServer&) = delete;
    CapnpFeedServer& operator=(const CapnpFeedServer&) = delete;

    void start();
    void stop();

private:
    MarketDataPublisher& _publisher;
    uint16_t _port;
    std::atomic<bool> _running;
    uintptr_t _listenSocket;
    std::thread _acceptThread;
    std::mutex _clientsMutex;
    std::vector<std::thread> _clientThreads;

    void acceptLoop();
    void serveClient(uintptr_t clientSocket);
};
