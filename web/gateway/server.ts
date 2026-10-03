import * as grpc from "@grpc/grpc-js";
import * as protoLoader from "@grpc/proto-loader";
import { spawn, type ChildProcess } from "node:child_process";
import { createWriteStream, existsSync, mkdirSync, type WriteStream } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { WebSocketServer, WebSocket } from "ws";
import { Normalizer, type WireBatch } from "./normalize.js";
import { ROSTER } from "./roster.js";

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, "../..");
const args = new Map(
  process.argv.slice(2).map((a) => {
    const [k, v] = a.replace(/^--/, "").split("=");
    return [k, v ?? "true"] as const;
  }),
);
const EXCHANGE = args.get("exchange") ?? "localhost:50051";
const PORT = Number(args.get("port") ?? 8787);

const pkgDef = protoLoader.loadSync(
  ["order.proto", "trading.proto", "marketdata.proto", "telemetry.proto"].map((f) => path.join(repoRoot, "proto", f)),
  { keepCase: true, longs: Number, enums: String, defaults: false, oneofs: true, includeDirs: [path.join(repoRoot, "proto")] },
);
const market = (grpc.loadPackageDefinition(pkgDef) as any).market;
const creds = grpc.credentials.createInsecure();
const submitClient = new market.SubmitOrder(EXCHANGE, creds);
const cancelClient = new market.CancelOrder(EXCHANGE, creds);

const clients = new Set<WebSocket>();
const history: WireBatch[] = [];
const HISTORY_MAX = 600;
const normalizer = new Normalizer();
let exchangeUp = false;
let recorder: { stream: WriteStream; file: string; first: number | null; count: number } | null = null;

const send = (ws: WebSocket, msg: unknown) => ws.readyState === WebSocket.OPEN && ws.send(JSON.stringify(msg));
const broadcast = (msg: unknown) => {
  const s = JSON.stringify(msg);
  for (const ws of clients) if (ws.readyState === WebSocket.OPEN) ws.send(s);
};
const status = () => ({ t: "status", exchange: exchangeUp ? "connected" : "disconnected", recording: recorder?.file ?? null });

function record(msg: unknown, at: number) {
  if (!recorder) return;
  if (recorder.first === null) recorder.first = at;
  recorder.stream.write(JSON.stringify(msg) + "\n");
  recorder.count++;
}

function startRecording() {
  if (recorder) return;
  const dir = path.join(repoRoot, "web", "public", "recordings");
  mkdirSync(dir, { recursive: true });
  const file = `session-${new Date().toISOString().replace(/[:.]/g, "-")}.ndjson`;
  const stream = createWriteStream(path.join(dir, file));
  stream.write(JSON.stringify({ t: "hello", roster: ROSTER.map((b) => ({ id: b.id, strategy: b.strategy })) }) + "\n");
  recorder = { stream, file, first: null, count: 0 };
  console.log(`[gateway] recording -> web/public/recordings/${file}`);
  broadcast(status());
}

function stopRecording() {
  if (!recorder) return;
  console.log(`[gateway] recording stopped (${recorder.count} batches) -> ${recorder.file}`);
  recorder.stream.end();
  recorder = null;
  broadcast(status());
}

function subscribeTelemetry() {
  const client = new market.Telemetry(EXCHANGE, creds);
  const call = client.Subscribe({ node_id: "gateway" });
  let alive = false;
  const down = () => {
    if (alive || exchangeUp) {
      alive = false;
      exchangeUp = false;
      console.log("[gateway] exchange disconnected");
      broadcast(status());
    }
    client.close();
    setTimeout(subscribeTelemetry, 1000);
  };
  call.on("metadata", () => {
    alive = true;
    exchangeUp = true;
    console.log(`[gateway] connected to exchange ${EXCHANGE}`);
    broadcast(status());
  });
  call.on("data", (raw: unknown) => {
    const batch = normalizer.normalize(raw);
    history.push(batch);
    if (history.length > HISTORY_MAX) history.shift();
    broadcast(batch);
    record(batch, batch.at);
  });
  let ended = false;
  const once = () => {
    if (!ended) {
      ended = true;
      down();
    }
  };
  call.on("error", once);
  call.on("end", once);
}

setInterval(() => {
  const book = normalizer.flushBook();
  if (book) {
    const msg = { t: "book", at: Date.now(), book };
    broadcast(msg);
    record(msg, Date.now());
  }
}, 50);

const grpcUnary = (client: any, method: string, req: unknown): Promise<any> =>
  new Promise((resolve, reject) => client[method](req, { deadline: Date.now() + 3000 }, (e: Error | null, r: any) => (e ? reject(e) : resolve(r))));

async function placeOrder(node: string, side: "bid" | "ask", type: "market" | "limit", price: number, qty: number) {
  return grpcUnary(submitClient, "SubmitOrder", {
    node_id: node,
    order: { orderSide: side === "bid" ? "BID" : "ASK", orderType: type === "market" ? "MARKET" : "LIMIT", price, quantity: qty },
  });
}

const wss = new WebSocketServer({ port: PORT });
wss.on("connection", (ws) => {
  clients.add(ws);
  send(ws, { t: "hello", roster: ROSTER.map((b) => ({ id: b.id, strategy: b.strategy })) });
  send(ws, status());
  send(ws, { t: "history", batches: history });
  ws.on("close", () => clients.delete(ws));
  ws.on("message", async (data) => {
    let m: any;
    try { m = JSON.parse(String(data)); } catch { return; }
    try {
      if (m.t === "order") {
        const r = await placeOrder("human", m.side, m.type, Number(m.price) || 0, Math.floor(Number(m.qty)));
        send(ws, { t: "result", reqId: m.reqId, ok: true, orderId: r.order_id, status: String(r.order_status ?? "").toLowerCase(), filled: r.quantity ?? 0, avg: r.avg_price ?? 0, reason: r.rejection_reason ?? "" });
      } else if (m.t === "cancel") {
        const r = await grpcUnary(cancelClient, "CancelOrder", { node_id: "human", order_id: String(m.orderId) });
        send(ws, { t: "result", reqId: m.reqId, ok: !!r.successful, orderId: m.orderId, reason: r.successful ? "" : "order not found" });
      } else if (m.t === "record") {
        m.on ? startRecording() : stopRecording();
      }
    } catch (e) {
      send(ws, { t: "result", reqId: m.reqId, ok: false, reason: exchangeUp ? (e as Error).message : "exchange is not running" });
    }
  });
});
console.log(`[gateway] ws://localhost:${PORT}  (exchange ${EXCHANGE})`);

const children: ChildProcess[] = [];
function startDemo() {
  const buildDir = ["build-release", "build"].map((d) => path.join(repoRoot, d)).find((d) => existsSync(path.join(d, "exchangeService.exe")));
  if (!buildDir) {
    console.log("[gateway] no exchangeService.exe found in build-release/ or build/ - build the C++ project first");
    return;
  }
  const env = { ...process.env, PATH: `C:\\msys64\\ucrt64\\bin;${process.env.PATH}` };
  const run = (exe: string, a: string[], tag: string) => {
    const c = spawn(exe, a, { env, windowsHide: true, stdio: ["ignore", "ignore", "ignore"] });
    c.on("exit", (code) => console.log(`[gateway] ${tag} exited (${code})`));
    children.push(c);
    return c;
  };
  console.log(`[gateway] starting ${buildDir}\\exchangeService.exe + ${ROSTER.length} bots`);
  run(path.join(buildDir, "exchangeService.exe"), [], "exchange");
  setTimeout(() => {
    const replayFile = path.join(repoRoot, "nodes", "strategies", "replay_data", "sample_series.csv");
    for (const b of ROSTER) {
      const a = [`--strategy=${b.strategy}`, `--node-id=${b.id}`, `--server=${EXCHANGE}`, ...b.flags];
      if (b.strategy === "replay") a.push(`--replay-file=${replayFile}`);
      run(path.join(buildDir, "node.exe"), a, b.id);
    }
  }, 1500);
}
function shutdown() {
  stopRecording();
  for (const c of children) c.kill();
  process.exit(0);
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);

if (args.has("demo")) startDemo();
if (args.has("record")) startRecording();
subscribeTelemetry();
