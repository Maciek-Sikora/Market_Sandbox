export interface WireTrade {
  id: string;
  price: number;
  qty: number;
  aggressorSide: "bid" | "ask";
  aggressor: string;
  aggressorNode: string;
  resting: string;
  restingNode: string;
}

export interface WireBook {
  bids: [number, number, number][];
  asks: [number, number, number][];
}

export interface WireBatch {
  t: "batch";
  at: number;
  seq: number;
  op: "submit" | "cancel";
  order: {
    id: string;
    node: string;
    side: "bid" | "ask" | "";
    type: "market" | "limit" | "";
    price: number;
    qty: number;
    status: "filled" | "partial" | "queued" | "rejected" | "";
    filled: number;
    avg: number;
    reason: string;
    cancelOk: boolean;
  };
  trace: { enqueue: number; dequeue: number; matchDone: number; promiseSet: number; publish: number };
  trades: WireTrade[];
  tob: { bid: number | null; bidQty: number; ask: number | null; askQty: number } | null;
  book?: WireBook;
}

const SIDE: Record<string, "bid" | "ask"> = { BID: "bid", ASK: "ask" };
const TYPE: Record<string, "market" | "limit"> = { MARKET: "market", LIMIT: "limit" };
const STATUS: Record<string, WireBatch["order"]["status"]> = {
  FILLED: "filled",
  PARTIAL: "partial",
  QUEUED: "queued",
  REJECTED: "rejected",
};

const num = (v: unknown): number => (v === undefined || v === null ? 0 : Number(v));

interface Resting {
  node: string;
  remaining: number;
}

export class Normalizer {
  private resting = new Map<string, Resting>();
  private lastBookAt = 0;
  private pendingBook: WireBook | null = null;

  flushBook(): WireBook | null {
    const b = this.pendingBook;
    this.pendingBook = null;
    return b;
  }

  normalize(raw: any, now: number = Date.now()): WireBatch {
    const o = raw.order ?? {};
    const tr = raw.trace ?? {};
    const isCancel = o.op === "OP_CANCEL";
    const orderId: string = o.order_id ?? "";
    const node: string = o.node_id ?? "";
    const status = STATUS[o.status as string] ?? "";
    const qty = num(o.quantity);
    const filled = num(o.filled_quantity);
    const cancelOk = !!o.cancel_successful;

    const trades: WireTrade[] = (raw.trades ?? []).map((t: any): WireTrade => {
      const restingId: string = t.resting_order_id ?? "";
      const entry = this.resting.get(restingId);
      const restingNode = entry?.node ?? "?";
      if (entry) {
        entry.remaining -= num(t.quantity);
        if (entry.remaining <= 0) this.resting.delete(restingId);
      }
      return {
        id: t.trade_id ?? "",
        price: num(t.price),
        qty: num(t.quantity),
        aggressorSide: SIDE[t.aggressor_side as string] ?? "bid",
        aggressor: t.aggressor_order_id ?? "",
        aggressorNode: node,
        resting: restingId,
        restingNode,
      };
    });

    if (!isCancel && (status === "queued" || status === "partial")) {
      this.resting.set(orderId, { node, remaining: qty - filled });
      if (this.resting.size > 50_000) {
        const first = this.resting.keys().next().value;
        if (first !== undefined) this.resting.delete(first);
      }
    }
    if (isCancel && cancelOk) this.resting.delete(orderId);

    const rpcIn = num(tr.t_rpc_in);
    const rel = (v: unknown) => Math.max(0, num(v) - rpcIn);

    const t = raw.top_of_book;
    const tob = t
      ? {
          bid: t.bid_price === undefined ? null : num(t.bid_price),
          bidQty: num(t.bid_quantity),
          ask: t.ask_price === undefined ? null : num(t.ask_price),
          askQty: num(t.ask_quantity),
        }
      : null;

    const batch: WireBatch = {
      t: "batch",
      at: num(raw.timestamp),
      seq: num(tr.seq),
      op: isCancel ? "cancel" : "submit",
      order: {
        id: orderId,
        node,
        side: SIDE[o.side as string] ?? "",
        type: TYPE[o.type as string] ?? "",
        price: num(o.price),
        qty,
        status,
        filled,
        avg: num(o.avg_price),
        reason: o.rejection_reason ?? "",
        cancelOk,
      },
      trace: {
        enqueue: rel(tr.t_enqueue),
        dequeue: rel(tr.t_dequeue),
        matchDone: rel(tr.t_match_done),
        promiseSet: rel(tr.t_promise_set),
        publish: rel(tr.t_publish),
      },
      trades,
      tob,
    };

    if (raw.book) {
      const lv = (l: any): [number, number, number] => [num(l.price), num(l.quantity), num(l.orders)];
      const book: WireBook = { bids: (raw.book.bids ?? []).map(lv), asks: (raw.book.asks ?? []).map(lv) };
      if (trades.length > 0 || now - this.lastBookAt >= 33) {
        batch.book = book;
        this.lastBookAt = now;
        this.pendingBook = null;
      } else {
        this.pendingBook = book;
      }
    }
    return batch;
  }
}
