import type { Frame, RosterEntry, WireBatch, WireBook } from "./feed";

export const isBatch = (f: Frame): f is WireBatch => f.t === "batch";

export function bookAt(frames: Frame[], idx: number): WireBook | null {
  for (let i = Math.min(idx, frames.length - 1); i >= 0; i--) {
    const f = frames[i];
    if (f.t === "book") return f.book;
    if (f.book) return f.book;
  }
  return null;
}

export function lastBatchBefore(frames: Frame[], idx: number, pred: (b: WireBatch) => boolean = () => true): WireBatch | null {
  for (let i = Math.min(idx, frames.length - 1); i >= 0; i--) {
    const f = frames[i];
    if (isBatch(f) && pred(f)) return f;
  }
  return null;
}

export interface TradePoint {
  at: number;
  price: number;
  qty: number;
  aggressorSide: "bid" | "ask";
  aggressorNode: string;
  restingNode: string;
  id: string;
}

export function recentTrades(frames: Frame[], idx: number, max: number): TradePoint[] {
  const out: TradePoint[] = [];
  for (let i = Math.min(idx, frames.length - 1); i >= 0 && out.length < max; i--) {
    const f = frames[i];
    if (!isBatch(f)) continue;
    for (let j = f.trades.length - 1; j >= 0 && out.length < max; j--) {
      const t = f.trades[j];
      out.push({ at: f.at, price: t.price, qty: t.qty, aggressorSide: t.aggressorSide, aggressorNode: t.aggressorNode, restingNode: t.restingNode, id: t.id });
    }
  }
  return out;
}

export interface QuotePoint {
  at: number;
  bid: number | null;
  ask: number | null;
}

export function priceSeries(frames: Frame[], idx: number, windowMs: number) {
  const trades: TradePoint[] = [];
  const quotes: QuotePoint[] = [];
  if (idx < 0) return { trades, quotes, from: 0, to: 0 };
  const to = frames[idx].at;
  const from = to - windowMs;
  for (let i = idx; i >= 0; i--) {
    const f = frames[i];
    if (f.at < from) break;
    if (!isBatch(f)) continue;
    if (f.tob) quotes.push({ at: f.at, bid: f.tob.bid, ask: f.tob.ask });
    for (const t of f.trades) trades.push({ at: f.at, price: t.price, qty: t.qty, aggressorSide: t.aggressorSide, aggressorNode: t.aggressorNode, restingNode: t.restingNode, id: t.id });
  }
  trades.reverse();
  quotes.reverse();
  return { trades, quotes, from, to };
}

export interface BotStat {
  id: string;
  strategy: string;
  orders: number;
  cancels: number;
  fills: number;
  volume: number;
  position: number;
  cash: number;
  pnl: number;
}
export interface OpenOrder {
  id: string;
  side: "bid" | "ask";
  price: number;
  remaining: number;
}

export function computeStats(frames: Frame[], idx: number, roster: RosterEntry[]) {
  const stats = new Map<string, BotStat>();
  const get = (id: string): BotStat => {
    let s = stats.get(id);
    if (!s) {
      s = { id, strategy: roster.find((r) => r.id === id)?.strategy ?? (id === "human" ? "you" : id), orders: 0, cancels: 0, fills: 0, volume: 0, position: 0, cash: 0, pnl: 0 };
      stats.set(id, s);
    }
    return s;
  };
  for (const r of roster) get(r.id);

  const open = new Map<string, OpenOrder>();
  let lastPrice = 0;
  for (let i = 0; i <= idx && i < frames.length; i++) {
    const f = frames[i];
    if (!isBatch(f)) continue;
    const o = f.order;
    const me = get(o.node);
    if (f.op === "submit") me.orders++;
    else if (o.cancelOk) me.cancels++;

    for (const t of f.trades) {
      lastPrice = t.price;
      const dir = t.aggressorSide === "bid" ? 1 : -1;
      const agg = get(t.aggressorNode);
      agg.fills++;
      agg.volume += t.qty;
      agg.position += dir * t.qty;
      agg.cash -= dir * t.qty * t.price;
      const rest = get(t.restingNode);
      rest.fills++;
      rest.volume += t.qty;
      rest.position -= dir * t.qty;
      rest.cash += dir * t.qty * t.price;
      const oo = open.get(t.resting);
      if (oo) {
        oo.remaining -= t.qty;
        if (oo.remaining <= 0) open.delete(t.resting);
      }
    }
    if (o.node === "human") {
      if (f.op === "submit" && (o.status === "queued" || o.status === "partial") && o.side) {
        open.set(o.id, { id: o.id, side: o.side, price: o.price, remaining: o.qty - o.filled });
      } else if (f.op === "cancel" && o.cancelOk) open.delete(o.id);
    }
  }
  for (const s of stats.values()) s.pnl = s.cash + s.position * lastPrice;
  return { stats, open: [...open.values()], lastPrice };
}

export function percentile(sorted: number[], p: number) {
  if (!sorted.length) return 0;
  return sorted[Math.min(sorted.length - 1, Math.floor((p / 100) * sorted.length))];
}

export const fmtPrice = (p: number) => p.toFixed(2);
export const fmtSigned = (v: number, d = 2) => (v > 0 ? "+" : v < 0 ? "−" : "") + Math.abs(v).toFixed(d);
export const fmtUs = (us: number) => (us >= 1000 ? `${(us / 1000).toFixed(2)} ms` : `${us.toFixed(0)} µs`);
