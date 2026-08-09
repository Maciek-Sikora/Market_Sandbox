#pragma once
#include <atomic>
#include <thread>
#include "exchange/OrderProcessor/repo/mpscqueue.h"
#include "exchange/OrderProcessor/engine/queueMessage.h"
#include "exchange/OrderProcessor/engine/orderBook.h"

class MatchingEngine {
public:
    explicit MatchingEngine(MPSCQueue<QueueMessage>& queue) : _queue(queue), _running(false) {}

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
    OrderBook _book;
    std::thread _worker;
    std::atomic<bool> _running;

    void run();
    void process(QueueMessage& msg);
    static uint64_t nowMillis();
};
