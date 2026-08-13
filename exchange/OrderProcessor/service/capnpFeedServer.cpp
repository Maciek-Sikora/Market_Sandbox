#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>

#include <capnp/message.h>

#include "exchange/OrderProcessor/service/capnpFeedServer.h"
#include "exchange/OrderProcessor/dto/marketEventCapnp.h"
#include "exchange/OrderProcessor/repo/mpscqueue.h"
#include "exchange/OrderProcessor/engine/marketEvent.h"
#include "generated-capnp/marketdata.capnp.h"

namespace {

constexpr size_t kScratchWords = 512;

bool sendAll(SOCKET sock, const void* data, size_t length) {
    const char* p = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < length) {
        int n = ::send(sock, p + sent, static_cast<int>(length - sent), 0);
        if (n == SOCKET_ERROR || n == 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

}

CapnpFeedServer::CapnpFeedServer(MarketDataPublisher& publisher, uint16_t port)
    : _publisher(publisher), _port(port), _running(false), _listenSocket(INVALID_SOCKET) {}

CapnpFeedServer::~CapnpFeedServer() {
    stop();
}

void CapnpFeedServer::start() {
    if (_running) {
        return;
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::printf("[capnp-feed] WSAStartup failed\n");
        return;
    }

    SOCKET listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::printf("[capnp-feed] socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    int reuse = 1;
    ::setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);

    if (::bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR ||
        ::listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        std::printf("[capnp-feed] bind/listen on port %u failed: %d\n", _port, WSAGetLastError());
        ::closesocket(listenSock);
        WSACleanup();
        return;
    }

    _listenSocket = static_cast<uintptr_t>(listenSock);
    _running = true;
    _acceptThread = std::thread(&CapnpFeedServer::acceptLoop, this);
    std::printf("[capnp-feed] listening on port %u\n", _port);
}

void CapnpFeedServer::stop() {
    if (!_running) {
        return;
    }
    _running = false;

    if (_listenSocket != INVALID_SOCKET) {
        ::closesocket(static_cast<SOCKET>(_listenSocket));
        _listenSocket = INVALID_SOCKET;
    }
    if (_acceptThread.joinable()) {
        _acceptThread.join();
    }

    std::vector<std::thread> clients;
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        clients.swap(_clientThreads);
    }
    for (auto& t : clients) {
        if (t.joinable()) {
            t.join();
        }
    }

    WSACleanup();
}

void CapnpFeedServer::acceptLoop() {
    while (_running) {
        SOCKET client = ::accept(static_cast<SOCKET>(_listenSocket), nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            break;
        }
        std::lock_guard<std::mutex> lock(_clientsMutex);
        _clientThreads.emplace_back(&CapnpFeedServer::serveClient, this,
                                    static_cast<uintptr_t>(client));
    }
}

void CapnpFeedServer::serveClient(uintptr_t clientSocket) {
    SOCKET sock = static_cast<SOCKET>(clientSocket);


    auto queue = std::make_shared<MPSCQueue<MarketEvent>>();
    auto subId = _publisher.subscribe(queue);
    std::printf("[capnp-feed] subscriber %llu connected\n",
                static_cast<unsigned long long>(subId));


    capnp::word scratch[kScratchWords];
    std::memset(scratch, 0, sizeof(scratch));

    MarketEvent event;
    bool alive = true;
    while (_running && alive) {
        if (!queue->dequeue(event)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        capnp::MallocMessageBuilder message(kj::arrayPtr(scratch, kScratchWords));
        MarketEventCapnp::write(event, message.initRoot<mktfeed::MarketDataEvent>());

        auto segments = message.getSegmentsForOutput();
        if (segments.size() != 1) {
            std::printf("[capnp-feed] event did not fit one segment, dropping subscriber\n");
            break;
        }

        auto words = segments[0];
        uint32_t byteLen = static_cast<uint32_t>(words.size() * sizeof(capnp::word));
        alive = sendAll(sock, &byteLen, sizeof(byteLen)) &&
                sendAll(sock, words.begin(), byteLen);
    }

    _publisher.unsubscribe(subId);
    ::closesocket(sock);
    std::printf("[capnp-feed] subscriber %llu disconnected\n",
                static_cast<unsigned long long>(subId));
}
