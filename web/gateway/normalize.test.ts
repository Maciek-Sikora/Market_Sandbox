import { describe, expect, it } from "vitest";
import { Normalizer } from "./normalize";

const batch = (over: any = {}) => ({
  timestamp: 1000,
  order: { op: "OP_SUBMIT", order_id: "ORD-1", node_id: "mm-1", side: "ASK", type: "LIMIT", price: 101, quantity: 10, status: "QUEUED", filled_quantity: 0 },
  trace: { seq: 1, t_rpc_in: 500, t_enqueue: 503, t_dequeue: 510, t_match_done: 520, t_promise_set: 524, t_publish: 530 },
  trades: [],
  top_of_book: { ask_price: 101, ask_quantity: 10 },
  book: { bids: [], asks: [{ price: 101, quantity: 10, orders: 1 }] },
  ...over,
});

describe("Normalizer", () => {
  it("maps enums, keeps absent top-of-book sides as null and makes trace times relative", () => {
    const n = new Normalizer();
    const out = n.normalize(batch(), 0);
    expect(out.op).toBe("submit");
    expect(out.order).toMatchObject({ side: "ask", type: "limit", status: "queued", node: "mm-1" });
    expect(out.tob).toEqual({ bid: null, bidQty: 0, ask: 101, askQty: 10 });
    expect(out.trace).toEqual({ enqueue: 3, dequeue: 10, matchDone: 20, promiseSet: 24, publish: 30 });
  });

  it("attributes fills to the resting order's owner and tracks what is left", () => {
    const n = new Normalizer();
    n.normalize(batch(), 0);
    const fill = (qty: number, id: string) =>
      n.normalize(
        batch({
          order: { op: "OP_SUBMIT", order_id: id, node_id: "mom-1", side: "BID", type: "MARKET", price: 0, quantity: qty, status: "FILLED", filled_quantity: qty, avg_price: 101 },
          trades: [{ trade_id: "TRD-" + id, price: 101, quantity: qty, aggressor_side: "BID", aggressor_order_id: id, resting_order_id: "ORD-1" }],
        }),
        100,
      );
    expect(fill(4, "ORD-2").trades[0]).toMatchObject({ restingNode: "mm-1", aggressorNode: "mom-1", qty: 4 });
    expect(fill(6, "ORD-3").trades[0].restingNode).toBe("mm-1");
    expect(fill(1, "ORD-4").trades[0].restingNode).toBe("?");
  });

  it("forgets cancelled orders", () => {
    const n = new Normalizer();
    n.normalize(batch(), 0);
    n.normalize(batch({ order: { op: "OP_CANCEL", order_id: "ORD-1", node_id: "mm-1", cancel_successful: true }, trades: [] }), 100);
    const out = n.normalize(batch({ trades: [{ trade_id: "T", price: 101, quantity: 1, aggressor_side: "BID", resting_order_id: "ORD-1" }] }), 200);
    expect(out.trades[0].restingNode).toBe("?");
  });

  it("coalesces the book to ~30 fps but flushes the withheld one", () => {
    const n = new Normalizer();
    expect(n.normalize(batch(), 1000).book).toBeDefined();
    expect(n.normalize(batch(), 1010).book).toBeUndefined();
    expect(n.flushBook()).not.toBeNull();
    expect(n.flushBook()).toBeNull();
    const withTrade = batch({ trades: [{ trade_id: "T", price: 101, quantity: 1, aggressor_side: "BID", resting_order_id: "x" }] });
    expect(n.normalize(withTrade, 1011).book).toBeDefined();
  });
});
