import { useEffect, useRef } from "react";
import { feed, type WireBatch } from "../feed";
import { bookAt } from "../derive";

export const W = 1200;
export const H = 440;

const C = {
  bg: "#0a1320",
  grid: "#122338",
  line: "#2a4460",
  text: "#c9dcef",
  dim: "#6f89a6",
  cyan: "#56d4ff",
  bid: "#2fd1a0",
  ask: "#ff7a6b",
  human: "#ffb938",
  queued: "#7fb2ff",
};

const BOT_X = 24;
const BOT_W = 104;
const BOT_H = 24;
const GRPC = { x: 270, y: 170, w: 120, h: 100 };
const QUEUE = { x: 450, y: 205, w: 240, h: 30, slots: 20 };
const ENGINE = { x: 770, y: 170, w: 120, h: 100 };
const BOOK = { x: 770, y: 318, w: 120, h: 92 };
const FAN = { x: 960, y: 185, w: 100, h: 70 };
const SUBS = [
  { x: 1110, y: 128, label: "bots' feeds" },
  { x: 1110, y: 290, label: "this page" },
];

type Pt = [number, number];
const lerp = (a: Pt, b: Pt, t: number): Pt => [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t];
const clamp01 = (v: number) => Math.min(1, Math.max(0, v));
const ease = (t: number) => t * t * (3 - 2 * t);

function polyAt(pts: Pt[], t: number): Pt {
  const lens: number[] = [];
  let total = 0;
  for (let i = 1; i < pts.length; i++) {
    const l = Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
    lens.push(l);
    total += l;
  }
  let d = clamp01(t) * total;
  for (let i = 0; i < lens.length; i++) {
    if (d <= lens[i] || i === lens.length - 1) return lerp(pts[i], pts[i + 1], lens[i] ? d / lens[i] : 0);
    d -= lens[i];
  }
  return pts[pts.length - 1];
}

interface Packet {
  f: WireBatch;
  start: number;
  dur: number;
  spot: boolean;
  s: [number, number, number, number];
}

export function boundaries(f: WireBatch): Packet["s"] {
  const d1 = Math.max(f.trace.enqueue, 1);
  const d2 = Math.max(f.trace.dequeue - f.trace.enqueue, 1);
  const d3 = Math.max(f.trace.matchDone - f.trace.dequeue, 1);
  const tot = d1 + d2 + d3;
  const s0 = 0.12;
  const s1 = s0 + 0.08 + 0.26 * (d1 / tot);
  const s2 = s1 + 0.08 + 0.26 * (d2 / tot);
  const s3 = s2 + 0.08 + 0.26 * (d3 / tot);
  return [s0, s1, s2, s3];
}

export const STAGES = ["gRPC handler", "Queue", "Matching", "Reply and publish"] as const;
export function stageAt(s: Packet["s"], p: number) {
  if (p < s[0]) return -1;
  if (p < s[1]) return 0;
  if (p < s[2]) return 1;
  if (p < s[3]) return 2;
  return 3;
}

export interface SpotState {
  frame: WireBatch;
  p: number;
}

interface Props {
  selected: WireBatch | null;
  onSelect: (b: WireBatch | null) => void;
  spotlightToken: number;
  onSpot: (s: SpotState | null) => void;
}

export function Scope({ selected, onSelect, spotlightToken, onSpot }: Props) {
  const canvas = useRef<HTMLCanvasElement>(null);
  const packets = useRef<Packet[]>([]);
  const botGlow = useRef(new Map<string, number>());
  const bookFlash = useRef(0);
  const selRef = useRef<WireBatch | null>(selected);
  const spotRef = useRef<Packet | null>(null);
  const lastSpot = useRef<string>("");
  const hits = useRef<{ f: WireBatch; x: number; y: number }[]>([]);
  selRef.current = selected;

  useEffect(() => {
    if (!selected || spotlightToken === 0) return;
    spotRef.current = { f: selected, start: performance.now(), dur: 9000, spot: true, s: boundaries(selected) };
  }, [spotlightToken]);

  useEffect(() => {
    const cv = canvas.current!;
    const ctx = cv.getContext("2d")!;
    const reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    let raf = 0;

    const resize = () => {
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      const w = cv.clientWidth;
      cv.width = Math.round(w * dpr);
      cv.height = Math.round(w * (H / W) * dpr);
    };
    const ro = new ResizeObserver(resize);
    ro.observe(cv);
    resize();

    const botSlots = () => {
      const ids = feed.roster.map((r) => r.id);
      const slots = new Map<string, number>();
      ids.forEach((id, i) => slots.set(id, i));
      slots.set("human", ids.length);
      return slots;
    };
    const botY = (slots: Map<string, number>, node: string) => {
      const n = slots.size || 1;
      const i = slots.get(node) ?? slots.get("human") ?? 0;
      return 36 + (i * (H - 78)) / Math.max(n - 1, 1);
    };

    const draw = (now: number) => {
      raf = requestAnimationFrame(draw);
      feed.tick(now);

      const fresh = feed.takeNew();
      const journey = 2600 / feed.speed;
      for (const fr of fresh) {
        if (fr.t !== "batch") continue;
        botGlow.current.set(fr.order.node, now);
        if (fr.trades.length) bookFlash.current = now;
        if (packets.current.length < 140) packets.current.push({ f: fr, start: now, dur: journey, spot: false, s: boundaries(fr) });
      }
      packets.current = packets.current.filter((p) => now - p.start < p.dur + 60);

      const dpr = cv.width / cv.clientWidth;
      const k = (cv.clientWidth / W) * dpr;
      ctx.setTransform(k, 0, 0, k, 0, 0);
      ctx.fillStyle = C.bg;
      ctx.fillRect(0, 0, W, H);

      ctx.strokeStyle = C.grid;
      ctx.lineWidth = 1;
      ctx.beginPath();
      for (let x = 0; x <= W; x += 40) { ctx.moveTo(x, 0); ctx.lineTo(x, H); }
      for (let y = 0; y <= H; y += 40) { ctx.moveTo(0, y); ctx.lineTo(W, y); }
      ctx.stroke();

      const slots = botSlots();
      const active = packets.current;
      const spot = spotRef.current;
      const sel = selRef.current;

      ctx.strokeStyle = C.line;
      ctx.lineWidth = 1.5;
      ctx.setLineDash([]);
      ctx.beginPath();
      for (const [id] of slots) {
        const y = botY(slots, id);
        ctx.moveTo(BOT_X + BOT_W, y);
        ctx.bezierCurveTo(200, y, 210, 220, GRPC.x, 220);
      }
      ctx.moveTo(GRPC.x + GRPC.w, 220); ctx.lineTo(QUEUE.x, 220);
      ctx.moveTo(QUEUE.x + QUEUE.w, 220); ctx.lineTo(ENGINE.x, 220);
      ctx.moveTo(ENGINE.x + ENGINE.w / 2, ENGINE.y + ENGINE.h); ctx.lineTo(BOOK.x + BOOK.w / 2, BOOK.y);
      ctx.moveTo(ENGINE.x + ENGINE.w, 220); ctx.lineTo(FAN.x, 220);
      for (const s of SUBS) { ctx.moveTo(FAN.x + FAN.w, 220); ctx.bezierCurveTo(1090, 220, 1090, s.y + 12, s.x, s.y + 12); }
      ctx.stroke();
      ctx.setLineDash([5, 5]);
      ctx.strokeStyle = "#35597a";
      ctx.beginPath();
      ctx.moveTo(ENGINE.x + 30, ENGINE.y + ENGINE.h);
      ctx.lineTo(ENGINE.x + 30, 300);
      ctx.lineTo(GRPC.x + 60, 300);
      ctx.lineTo(GRPC.x + 60, GRPC.y + GRPC.h);
      ctx.stroke();
      ctx.setLineDash([]);

      ctx.font = "500 12px 'Spline Sans Mono', monospace";
      ctx.textBaseline = "middle";
      for (const [id] of slots) {
        const y = botY(slots, id);
        const g = clamp01(1 - (now - (botGlow.current.get(id) ?? -1e9)) / 500);
        const isHuman = id === "human";
        ctx.fillStyle = `rgba(${isHuman ? "255,185,56" : "86,212,255"},${0.06 + 0.35 * g})`;
        ctx.strokeStyle = isHuman ? C.human : "#34607f";
        round(ctx, BOT_X, y - BOT_H / 2, BOT_W, BOT_H, 5);
        ctx.fill();
        ctx.stroke();
        ctx.fillStyle = isHuman ? C.human : C.text;
        ctx.fillText(isHuman ? "you" : id, BOT_X + 10, y + 0.5);
      }

      node(ctx, GRPC, "gRPC handlers", "one thread per call", 0);
      const inQueue = active.filter((p) => {
        const e = (now - p.start) / p.dur;
        return e >= p.s[1] && e < p.s[2];
      }).length;
      ctx.strokeStyle = C.line;
      ctx.fillStyle = "#0e1c2e";
      round(ctx, QUEUE.x - 8, QUEUE.y - 22, QUEUE.w + 16, QUEUE.h + 44, 8);
      ctx.fill();
      ctx.stroke();
      const sw = QUEUE.w / QUEUE.slots;
      for (let i = 0; i < QUEUE.slots; i++) {
        const filled = i >= QUEUE.slots - Math.min(inQueue, QUEUE.slots);
        ctx.fillStyle = filled ? C.queued : "#13273d";
        ctx.fillRect(QUEUE.x + i * sw + 1, QUEUE.y, sw - 2, QUEUE.h);
      }
      ctx.fillStyle = C.text;
      ctx.font = "600 13px 'Schibsted Grotesk', sans-serif";
      ctx.textAlign = "center";
      ctx.fillText("MPSC queue", QUEUE.x + QUEUE.w / 2, QUEUE.y - 36 + 12);
      ctx.fillStyle = C.dim;
      ctx.font = "400 11px 'Schibsted Grotesk', sans-serif";
      ctx.fillText("lock-free, many producers, one consumer", QUEUE.x + QUEUE.w / 2, QUEUE.y + QUEUE.h + 36);
      ctx.textAlign = "left";

      const engineBusy = active.some((p) => {
        const e = (now - p.start) / p.dur;
        return e >= p.s[2] && e < p.s[3];
      });
      node(ctx, ENGINE, "Matching engine", "one thread, busy-spins", engineBusy ? 1 : 0);

      const bk = bookAt(feed.frames, feed.indexAt(feed.vt));
      const flash = clamp01(1 - (now - bookFlash.current) / 420);
      ctx.strokeStyle = flash > 0 ? C.cyan : C.line;
      ctx.fillStyle = "#0e1c2e";
      round(ctx, BOOK.x, BOOK.y, BOOK.w, BOOK.h, 8);
      ctx.fill();
      ctx.stroke();
      if (bk) {
        const maxQ = Math.max(1, ...bk.bids.slice(0, 5).map((l) => l[1]), ...bk.asks.slice(0, 5).map((l) => l[1]));
        for (let i = 0; i < 5; i++) {
          const a = bk.asks[i];
          const b = bk.bids[i];
          if (a) { ctx.fillStyle = C.ask; ctx.globalAlpha = 0.85; ctx.fillRect(BOOK.x + 60, BOOK.y + 28 - i * 5, (a[1] / maxQ) * 52, 4); }
          if (b) { ctx.fillStyle = C.bid; ctx.globalAlpha = 0.85; ctx.fillRect(BOOK.x + 60, BOOK.y + 40 + i * 5, (b[1] / maxQ) * 52, 4); }
        }
        ctx.globalAlpha = 1;
      }
      ctx.fillStyle = C.text;
      ctx.font = "600 13px 'Schibsted Grotesk', sans-serif";
      ctx.fillText("Order book", BOOK.x + 10, BOOK.y + 80);
      ctx.fillStyle = C.dim;
      ctx.font = "400 11px 'Schibsted Grotesk', sans-serif";
      ctx.fillText("depth", BOOK.x + 10, BOOK.y + 14);

      node(ctx, FAN, "Market data", "queue per subscriber", 0);
      ctx.font = "500 12px 'Spline Sans Mono', monospace";
      for (const s of SUBS) {
        ctx.strokeStyle = "#34607f";
        ctx.fillStyle = "#0e1c2e";
        round(ctx, s.x - 20, s.y, 100, 24, 5);
        ctx.fill();
        ctx.stroke();
        ctx.fillStyle = C.text;
        ctx.fillText(s.label, s.x - 12, s.y + 12.5);
      }

      const drawPacket = (p: Packet, prog: number, big: boolean) => {
        const f = p.f;
        const color = f.order.node === "human" ? C.human : f.op === "cancel" ? "#9fb6cc" : f.order.side === "bid" ? C.bid : C.ask;
        const y0 = botY(slots, f.order.node);
        const B: Pt = [BOT_X + BOT_W, y0];
        const main: Pt[] = [B, [GRPC.x, 220], [GRPC.x + GRPC.w, 220], [QUEUE.x, 220], [QUEUE.x + QUEUE.w, 220], [ENGINE.x, 220], [ENGINE.x + ENGINE.w / 2, 220]];
        const [s0, s1, s2, s3] = p.s;
        let head: Pt | null = null;
        if (prog < s0) head = lerp(B, [GRPC.x, 220], ease(prog / s0));
        else if (prog < s1) head = lerp(main[1], main[3], (prog - s0) / (s1 - s0));
        else if (prog < s2) head = lerp(main[3], main[4], (prog - s1) / (s2 - s1));
        else if (prog < s3) head = lerp(main[4], main[6], (prog - s2) / (s3 - s2));
        const r = big ? 8 : 4.5;
        const glow = (pt: Pt, col: string, rr: number, a = 1) => {
          ctx.globalAlpha = 0.25 * a;
          ctx.fillStyle = col;
          ctx.beginPath(); ctx.arc(pt[0], pt[1], rr * 2.3, 0, 7); ctx.fill();
          ctx.globalAlpha = a;
          ctx.beginPath(); ctx.arc(pt[0], pt[1], rr, 0, 7); ctx.fill();
          ctx.globalAlpha = 1;
        };
        if (head) glow(head, color, r);
        if (prog >= s3) {
          const t = (prog - s3) / (1 - s3);
          const fade = t > 0.85 ? 1 - (t - 0.85) / 0.15 : 1;
          const rc = f.order.status === "rejected" ? C.ask : f.order.status === "filled" ? C.bid : f.order.status === "partial" ? C.human : f.op === "cancel" ? "#9fb6cc" : C.queued;
          const route: Pt[] = [[ENGINE.x + 30, ENGINE.y + ENGINE.h], [ENGINE.x + 30, 300], [GRPC.x + 60, 300], [GRPC.x + 60, GRPC.y + GRPC.h], B];
          glow(polyAt(route, ease(t)), rc, r * 0.85, fade);
          const t2 = clamp01(t / 0.8);
          if (t2 < 1 && (f.trades.length || f.tob)) {
            const base: Pt[] = [[ENGINE.x + ENGINE.w, 220], [FAN.x, 220], [FAN.x + FAN.w, 220]];
            if (t2 < 0.5) glow(polyAt(base, t2 / 0.5), C.cyan, r * 0.75);
            else for (const s of SUBS) glow(lerp([FAN.x + FAN.w, 220], [s.x, s.y + 12], ease((t2 - 0.5) / 0.5)), C.cyan, r * 0.7);
          }
          if (f.trades.length && t < 0.35) glow(lerp([ENGINE.x + ENGINE.w / 2, ENGINE.y + ENGINE.h], [BOOK.x + BOOK.w / 2, BOOK.y], t / 0.35), "#ffffff", r * 0.6);
        }
        if (sel && sel.seq === f.seq && sel.at === f.at) {
          const pt = head ?? polyAt([[ENGINE.x + 30, ENGINE.y + ENGINE.h], [ENGINE.x + 30, 300], [GRPC.x + 60, 300], [GRPC.x + 60, GRPC.y + GRPC.h], B], ease((prog - s3) / (1 - s3)));
          ctx.strokeStyle = "#fff";
          ctx.lineWidth = 1.5;
          ctx.beginPath(); ctx.arc(pt[0], pt[1], r + 6, 0, 7); ctx.stroke();
        }
        return head;
      };

      hits.current = [];
      for (const p of active) {
        const prog = (now - p.start) / p.dur;
        const h = drawPacket(p, clamp01(prog), false);
        if (h) hits.current.push({ f: p.f, x: h[0], y: h[1] });
      }
      if (spot) {
        const prog = (now - spot.start) / spot.dur;
        if (prog >= 1.02) {
          spotRef.current = null;
          onSpot(null);
          lastSpot.current = "";
        } else {
          drawPacket(spot, clamp01(prog), true);
          const st = stageAt(spot.s, prog);
          const key = `${spot.f.seq}:${st}`;
          if (key !== lastSpot.current) {
            lastSpot.current = key;
            onSpot({ frame: spot.f, p: prog });
          }
        }
      }

      if (reduce) ctx.globalAlpha = 1;
    };

    raf = requestAnimationFrame(draw);
    return () => {
      cancelAnimationFrame(raf);
      ro.disconnect();
    };
  }, [onSpot]);

  const onClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const r = e.currentTarget.getBoundingClientRect();
    const x = ((e.clientX - r.left) / r.width) * W;
    const y = ((e.clientY - r.top) / r.height) * H;
    let best: { f: WireBatch; d: number } | null = null;
    for (const h of hits.current) {
      const d = Math.hypot(h.x - x, h.y - y);
      if (d < 22 && (!best || d < best.d)) best = { f: h.f, d };
    }
    onSelect(best ? best.f : null);
  };

  return (
    <canvas
      ref={canvas}
      className="scope-canvas"
      onClick={onClick}
      role="img"
      aria-label="Animated diagram: orders travel from bot processes through gRPC handlers and a lock-free queue into a single-threaded matching engine, then to the order book and to market data subscribers."
    />
  );
}

function round(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

function node(ctx: CanvasRenderingContext2D, n: { x: number; y: number; w: number; h: number }, title: string, sub: string, busy: number) {
  ctx.fillStyle = busy ? "#12304a" : "#0e1c2e";
  ctx.strokeStyle = busy ? C.cyan : C.line;
  ctx.lineWidth = busy ? 2 : 1.5;
  round(ctx, n.x, n.y, n.w, n.h, 10);
  ctx.fill();
  ctx.stroke();
  ctx.lineWidth = 1.5;
  ctx.textAlign = "center";
  ctx.fillStyle = C.text;
  ctx.font = "600 14px 'Schibsted Grotesk', sans-serif";
  ctx.fillText(title, n.x + n.w / 2, n.y + n.h / 2 - 6);
  ctx.fillStyle = C.dim;
  ctx.font = "400 11px 'Schibsted Grotesk', sans-serif";
  ctx.fillText(sub, n.x + n.w / 2, n.y + n.h / 2 + 12);
  ctx.textAlign = "left";
}
