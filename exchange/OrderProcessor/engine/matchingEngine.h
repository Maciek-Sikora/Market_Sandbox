#pragma once
#include <atomic>
#include <thread>
#include "exchange/OrderProcessor/repo/mpscqueue.h"
#include "exchange/OrderProcessor/engine/queueMessage.h"
#include "exchange/OrderProcessor/engine/orderBook.h"
#include "exchange/OrderProcessor/engine/marketDataPublisher.h"

class MatchingEngine {
public:
    MatchingEngine(MPSCQueue<QueueMessage>& queue, MarketDataPublisher& publisher)
        : _queue(queue), _publisher(publisher), _running(false) {}

    void start() {
        _running.store(true);
        _worker = std::thread(&MatchingEngine::run, this);
    }

    void stop() {
        _running.store(false);
        if (_worker.joinable()) {
            _worker.join();
        }
    }

    ~MatchingEngine() {
        if (_worker.joinable()) {
            stop();
        }
    }

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

private:
    MPSCQueue<QueueMessage>& _queue;
    MarketDataPublisher& _publisher;
    OrderBook _book;
    std::thread _worker;
    std::atomic<bool> _running;

    void run();
    void process(QueueMessage& msg);
    static uint64_t nowMillis();
};
