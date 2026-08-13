// Two ways to look at the Cap'n Proto feed.
//
//   --bench (default)  In-process. Encodes and decodes the same corpus of
//                      MarketEvents both ways and reports ns/op and bytes.
//                      No sockets, so no loopback noise in the numbers.
//   --live [host:port] Connects to the running exchange's capnp feed on
//                      50052 and prints what comes off it. Proves the
//                      transport actually works end to end.
//
// Winsock first: it must precede any windows.h that protobuf might pull in.
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include <capnp/message.h>
#include <capnp/serialize.h>

#include "exchange/OrderProcessor/engine/marketEvent.h"
#include "exchange/OrderProcessor/dto/marketEventDTO.h"
#include "exchange/OrderProcessor/dto/marketEventCapnp.h"
#include "generated-proto/marketdata.pb.h"
#include "generated-capnp/marketdata.capnp.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr size_t kScratchWords = 512;
constexpr size_t kCorpusSize = 100000;

struct Sink {
    double d = 0;
    uint64_t u = 0;
};
Sink g_sink;


std::vector<MarketEvent> buildCorpus(size_t n) {
    std::vector<MarketEvent> events;
    events.reserve(n);
    std::mt19937 rng(12345);

    for (size_t i = 0; i < n; ++i) {
        if (i % 2 == 0) {
            Trade t;
            t.tradeId = "TRD-" + std::to_string(100000 + i % 900000);
            t.price = 100.0 + static_cast<double>(rng() % 2000) * 0.01;
            t.quantity = static_cast<int64_t>(rng() % 50) + 1;
            t.aggressorSide = (rng() % 2) ? BID : ASK;
            t.aggressorOrderId = "ORD-" + std::to_string(100000 + rng() % 900000);
            t.restingOrderId = "ORD-" + std::to_string(100000 + rng() % 900000);
            t.timestamp = 1723550000000ULL + i;
            events.emplace_back(std::move(t));
        } else {
            TopOfBook tob;
            if (i % 11 != 0) {
                tob.bidPrice = 100.0 + static_cast<double>(rng() % 100) * 0.01;
                tob.bidQuantity = static_cast<int64_t>(rng() % 200) + 1;
            }
            if (i % 13 != 0) {
                tob.askPrice = 101.0 + static_cast<double>(rng() % 100) * 0.01;
                tob.askQuantity = static_cast<int64_t>(rng() % 200) + 1;
            }
            tob.timestamp = 1723550000000ULL + i;
            events.emplace_back(std::move(tob));
        }
    }
    return events;
}


void readAllProto(const ::market::MarketDataEvent& e, Sink& s) {
    if (e.has_trade()) {
        const auto& t = e.trade();
        s.u += t.trade_id().size() + (t.trade_id().empty() ? 0 : t.trade_id()[0]);
        s.d += t.price();
        s.u += static_cast<uint64_t>(t.quantity());
        s.u += static_cast<uint64_t>(t.aggressor_side());
        s.u += t.aggressor_order_id().size() +
               (t.aggressor_order_id().empty() ? 0 : t.aggressor_order_id()[0]);
        s.u += t.resting_order_id().size() +
               (t.resting_order_id().empty() ? 0 : t.resting_order_id()[0]);
        s.u += t.timestamp();
    } else if (e.has_top_of_book()) {
        const auto& b = e.top_of_book();
        if (b.has_bid_price()) s.d += b.bid_price();
        if (b.has_bid_quantity()) s.u += static_cast<uint64_t>(b.bid_quantity());
        if (b.has_ask_price()) s.d += b.ask_price();
        if (b.has_ask_quantity()) s.u += static_cast<uint64_t>(b.ask_quantity());
        s.u += b.timestamp();
    }
}

void readAllCapnp(mktfeed::MarketDataEvent::Reader e, Sink& s) {
    if (e.isTrade()) {
        auto t = e.getTrade();
        auto id = t.getTradeId();
        s.u += id.size() + (id.size() ? static_cast<unsigned char>(id.begin()[0]) : 0);
        s.d += t.getPrice();
        s.u += static_cast<uint64_t>(t.getQuantity());
        s.u += static_cast<uint64_t>(t.getAggressorSide());
        auto ag = t.getAggressorOrderId();
        s.u += ag.size() + (ag.size() ? static_cast<unsigned char>(ag.begin()[0]) : 0);
        auto rs = t.getRestingOrderId();
        s.u += rs.size() + (rs.size() ? static_cast<unsigned char>(rs.begin()[0]) : 0);
        s.u += t.getTimestamp();
    } else if (e.isTopOfBook()) {
        auto b = e.getTopOfBook();
        if (b.getHasBid()) {
            s.d += b.getBidPrice();
            s.u += static_cast<uint64_t>(b.getBidQuantity());
        }
        if (b.getHasAsk()) {
            s.d += b.getAskPrice();
            s.u += static_cast<uint64_t>(b.getAskQuantity());
        }
        s.u += b.getTimestamp();
    }
}

template <typename F>
double bestNsPerOp(F&& f, size_t ops, int rounds = 5) {
    double best = 1e30;
    for (int r = 0; r < rounds; ++r) {
        auto t0 = Clock::now();
        f();
        auto t1 = Clock::now();
        best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() /
                                  static_cast<double>(ops));
    }
    return best;
}


int runBench() {
    std::printf("[capnp-bench] building corpus of %zu events...\n", kCorpusSize);
    auto events = buildCorpus(kCorpusSize);
    const size_t n = events.size();

    capnp::word scratch[kScratchWords];
    std::memset(scratch, 0, sizeof(scratch));

    std::vector<char> pbBuf(1024);
    double pbEncodeNs = bestNsPerOp([&] {
        for (const auto& ev : events) {
            ::market::MarketDataEvent proto = MarketEventDTO::marketEventToProto(ev);
            size_t size = proto.ByteSizeLong();
            proto.SerializeToArray(pbBuf.data(), static_cast<int>(size));
            g_sink.u += size;
        }
    }, n);

    double capnpEncodeNs = bestNsPerOp([&] {
        for (const auto& ev : events) {
            capnp::MallocMessageBuilder message(kj::arrayPtr(scratch, kScratchWords));
            MarketEventCapnp::write(ev, message.initRoot<mktfeed::MarketDataEvent>());
            auto words = message.getSegmentsForOutput()[0];
            g_sink.u += words.size();
        }
    }, n);

    std::vector<std::vector<char>> pbWire(n);
    std::vector<std::vector<capnp::word>> capnpWire(n);
    size_t pbBytes = 0, capnpBytes = 0;

    for (size_t i = 0; i < n; ++i) {
        ::market::MarketDataEvent proto = MarketEventDTO::marketEventToProto(events[i]);
        pbWire[i].resize(proto.ByteSizeLong());
        proto.SerializeToArray(pbWire[i].data(), static_cast<int>(pbWire[i].size()));
        pbBytes += pbWire[i].size();

        capnp::MallocMessageBuilder message(kj::arrayPtr(scratch, kScratchWords));
        MarketEventCapnp::write(events[i], message.initRoot<mktfeed::MarketDataEvent>());
        auto words = message.getSegmentsForOutput()[0];
        capnpWire[i].assign(words.begin(), words.end());
        capnpBytes += words.size() * sizeof(capnp::word);
    }


    ::market::MarketDataEvent reused;

    double pbDecode1Ns = bestNsPerOp([&] {
        for (size_t i = 0; i < n; ++i) {
            reused.ParseFromArray(pbWire[i].data(), static_cast<int>(pbWire[i].size()));
            if (reused.has_trade()) g_sink.d += reused.trade().price();
            else if (reused.has_top_of_book()) g_sink.u += reused.top_of_book().timestamp();
        }
    }, n);

    double pbDecodeAllNs = bestNsPerOp([&] {
        for (size_t i = 0; i < n; ++i) {
            reused.ParseFromArray(pbWire[i].data(), static_cast<int>(pbWire[i].size()));
            readAllProto(reused, g_sink);
        }
    }, n);

    double capnpDecode1Ns = bestNsPerOp([&] {
        for (size_t i = 0; i < n; ++i) {
            kj::ArrayPtr<const capnp::word> segs[1] = {
                kj::arrayPtr(static_cast<const capnp::word*>(capnpWire[i].data()),
                             capnpWire[i].size())
            };
            capnp::SegmentArrayMessageReader reader(kj::arrayPtr(segs, 1));
            auto root = reader.getRoot<mktfeed::MarketDataEvent>();
            if (root.isTrade()) g_sink.d += root.getTrade().getPrice();
            else if (root.isTopOfBook()) g_sink.u += root.getTopOfBook().getTimestamp();
        }
    }, n);

    double capnpDecodeAllNs = bestNsPerOp([&] {
        for (size_t i = 0; i < n; ++i) {
            kj::ArrayPtr<const capnp::word> segs[1] = {
                kj::arrayPtr(static_cast<const capnp::word*>(capnpWire[i].data()),
                             capnpWire[i].size())
            };
            capnp::SegmentArrayMessageReader reader(kj::arrayPtr(segs, 1));
            readAllCapnp(reader.getRoot<mktfeed::MarketDataEvent>(), g_sink);
        }
    }, n);

    std::printf("\n");
    std::printf("[capnp-bench] %zu events, best of 5 rounds, ns per event\n\n", n);
    std::printf("  %-10s %10s %14s %16s %12s\n",
                "format", "encode", "decode 1 fld", "decode all flds", "bytes/event");
    std::printf("  %-10s %10s %14s %16s %12s\n",
                "----------", "----------", "--------------", "----------------", "------------");
    std::printf("  %-10s %9.1f %13.1f %15.1f %11.1f\n", "protobuf", pbEncodeNs, pbDecode1Ns,
                pbDecodeAllNs, static_cast<double>(pbBytes) / static_cast<double>(n));
    std::printf("  %-10s %9.1f %13.1f %15.1f %11.1f\n", "capnp", capnpEncodeNs, capnpDecode1Ns,
                capnpDecodeAllNs, static_cast<double>(capnpBytes) / static_cast<double>(n));
    std::printf("\n");
    std::printf("  encode          %5.2fx   decode 1 fld    %5.2fx   decode all  %5.2fx\n",
                pbEncodeNs / capnpEncodeNs, pbDecode1Ns / capnpDecode1Ns,
                pbDecodeAllNs / capnpDecodeAllNs);
    std::printf("  wire size       %5.2fx  (capnp is larger: it pads to 8-byte words "
                "so fields sit at fixed offsets)\n",
                static_cast<double>(capnpBytes) / static_cast<double>(pbBytes));
    std::printf("\n");
    std::printf("  Both decode columns matter. Cap'n Proto does no work until a field is\n");
    std::printf("  read, so \"decode 1 field\" is close to free but flatters it; \"decode all\"\n");
    std::printf("  is the fair worst case. A strategy reading only trade price lives at the\n");
    std::printf("  left column, a full-book consumer at the right.\n");
    std::printf("\n[capnp-bench] sink %.0f / %llu (ignore, exists to defeat the optimizer)\n",
                g_sink.d, static_cast<unsigned long long>(g_sink.u));
    return 0;
}

// --------------------------------------------------------------- live mode --

bool recvAll(SOCKET sock, void* data, size_t length) {
    char* p = static_cast<char*>(data);
    size_t got = 0;
    while (got < length) {
        int n = ::recv(sock, p + got, static_cast<int>(length - got), 0);
        if (n <= 0) {
            return false;
        }
        got += static_cast<size_t>(n);
    }
    return true;
}

const char* sideName(mktfeed::OrderSide side) {
    switch (side) {
        case mktfeed::OrderSide::BID: return "BID";
        case mktfeed::OrderSide::ASK: return "ASK";
        default: return "?";
    }
}

int runLive(const std::string& host, uint16_t port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::printf("[capnp-live] WSAStartup failed\n");
        return 1;
    }

    SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::printf("[capnp-live] socket() failed\n");
        WSACleanup();
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        std::printf("[capnp-live] bad host '%s' (expects a dotted IPv4 address)\n", host.c_str());
        ::closesocket(sock);
        WSACleanup();
        return 1;
    }

    if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::printf("[capnp-live] could not connect to %s:%u - is exchangeService.exe running?\n",
                    host.c_str(), port);
        ::closesocket(sock);
        WSACleanup();
        return 1;
    }
    std::printf("[capnp-live] connected to %s:%u, waiting for events (Ctrl-C to stop)\n",
                host.c_str(), port);

    std::vector<capnp::word> buf(kScratchWords);  // word array: 8-byte aligned, as the reader needs
    uint64_t count = 0, bytes = 0;

    while (true) {
        uint32_t byteLen = 0;
        if (!recvAll(sock, &byteLen, sizeof(byteLen))) {
            break;
        }
        if (byteLen == 0 || byteLen % sizeof(capnp::word) != 0 ||
            byteLen > kScratchWords * sizeof(capnp::word)) {
            std::printf("[capnp-live] bad frame length %u, dropping connection\n", byteLen);
            break;
        }
        if (!recvAll(sock, buf.data(), byteLen)) {
            break;
        }

        ++count;
        bytes += byteLen + sizeof(byteLen);

        kj::ArrayPtr<const capnp::word> segs[1] = {
            kj::arrayPtr(static_cast<const capnp::word*>(buf.data()),
                         byteLen / sizeof(capnp::word))
        };
        capnp::SegmentArrayMessageReader reader(kj::arrayPtr(segs, 1));
        auto root = reader.getRoot<mktfeed::MarketDataEvent>();

        if (root.isTrade()) {
            auto t = root.getTrade();
            std::printf("[capnp] TRADE  px=%-8.2f qty=%-4lld %s  %s hit %s\n", t.getPrice(),
                        static_cast<long long>(t.getQuantity()), sideName(t.getAggressorSide()),
                        t.getAggressorOrderId().cStr(), t.getRestingOrderId().cStr());
        } else if (root.isTopOfBook()) {
            auto b = root.getTopOfBook();
            char bid[32] = "    --   ";
            char ask[32] = "    --   ";
            if (b.getHasBid()) {
                std::snprintf(bid, sizeof(bid), "%.2f x%lld", b.getBidPrice(),
                              static_cast<long long>(b.getBidQuantity()));
            }
            if (b.getHasAsk()) {
                std::snprintf(ask, sizeof(ask), "%.2f x%lld", b.getAskPrice(),
                              static_cast<long long>(b.getAskQuantity()));
            }
            std::printf("[capnp] TOB    bid=%-14s ask=%-14s\n", bid, ask);
        }

        if (count % 50 == 0) {
            std::printf("[capnp-live] --- %llu events, %llu bytes (%.1f bytes/event) ---\n",
                        static_cast<unsigned long long>(count),
                        static_cast<unsigned long long>(bytes),
                        static_cast<double>(bytes) / static_cast<double>(count));
        }
    }

    std::printf("[capnp-live] stream ended after %llu events, %llu bytes\n",
                static_cast<unsigned long long>(count), static_cast<unsigned long long>(bytes));
    ::closesocket(sock);
    WSACleanup();
    return 0;
}

}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    bool live = false;
    std::string host = "127.0.0.1";
    uint16_t port = 50052;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--live") {
            live = true;
        } else if (arg == "--bench") {
            live = false;
        } else if (arg.rfind("--", 0) != 0) {
            auto colon = arg.find(':');
            if (colon != std::string::npos) {
                host = arg.substr(0, colon);
                port = static_cast<uint16_t>(std::stoi(arg.substr(colon + 1)));
            } else {
                host = arg;
            }
        }
    }

    return live ? runLive(host, port) : runBench();
}
