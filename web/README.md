# Market Sandbox web

A frontend wired to the real C++ exchange. Nothing here re-implements the matching engine.

```
exchangeService.exe --Telemetry.Subscribe (gRPC)--> gateway (Node) --WebSocket--> browser
node.exe bots ------SubmitOrder / CancelOrder-----> exchangeService.exe
browser ticket --WebSocket--> gateway --SubmitOrder (node_id "human")--> exchangeService.exe
```

A browser can't speak gRPC or raw Cap'n Proto over TCP, so `gateway/` bridges them. It loads the repo's own `proto/*.proto` files.

## Run it

Build the C++ first (Release gives honest numbers):

```
cmake -S .. -B ../build-release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build ../build-release
```

Then, from this folder:

```
npm install
npm run demo        # gateway + exchangeService.exe + the ten bots from nodes/launch_nodes.ps1
npm run dev         # the site, http://localhost:5173
```

`npm run gateway` attaches to an exchange you started yourself. `npm run record` also writes a session to `public/recordings/`; copy the one you like to `demo-session.ndjson`.

## When the exchange isn't running

The site replays `public/recordings/demo-session.ndjson`, captured from the C++ engine, and says so in the header. The ticket is disabled because there's nothing to trade against. That is what a hosted copy (`npm run build`, then serve `dist/`) shows.

## What the C++ side adds

`proto/telemetry.proto` defines a `Telemetry.Subscribe` stream. Each message describes one operation the engine handled: the order, trades, top of book, the top ten levels of depth, and six steady-clock timestamps through the pipeline (gRPC in, enqueued, dequeued, matched, reply released, published). The engine does none of this work unless a subscriber is attached. The existing MarketData stream, the Cap'n Proto feed and the bots are untouched.
