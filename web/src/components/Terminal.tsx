import { useEffect, useState } from "react";
import { feed, useFeed } from "../feed";
import { fmtPrice, fmtSigned, type BotStat, type OpenOrder } from "../derive";

export interface Prefill {
  side: "bid" | "ask";
  price: number;
  n: number;
}

interface Props {
  prefill: Prefill | null;
  open: OpenOrder[];
  me: BotStat | undefined;
}

export function Terminal({ prefill, open, me }: Props) {
  const f = useFeed();
  const [side, setSide] = useState<"bid" | "ask">("bid");
  const [type, setType] = useState<"limit" | "market">("limit");
  const [price, setPrice] = useState("");
  const [qty, setQty] = useState("5");
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState<{ ok: boolean; text: string } | null>(null);
  const can = f.canTrade();

  useEffect(() => {
    if (!prefill) return;
    setSide(prefill.side);
    setType("limit");
    setPrice(fmtPrice(prefill.price));
  }, [prefill]);

  const q = Math.floor(Number(qty));
  const p = Number(price);
  const valid = q > 0 && (type === "market" || p > 0);

  const submit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!valid || busy) return;
    setBusy(true);
    const r = await feed.placeOrder(side, type, type === "market" ? 0 : p, q);
    setBusy(false);
    if (!r.ok) setMsg({ ok: false, text: r.reason ?? "The order didn't go through." });
    else if (r.status === "rejected") setMsg({ ok: false, text: r.reason ?? "Rejected." });
    else if (r.status === "filled") setMsg({ ok: true, text: `Filled ${r.filled} at an average of ${fmtPrice(r.avg ?? 0)}.` });
    else if (r.status === "partial") setMsg({ ok: true, text: `Filled ${r.filled}. The rest is resting in the book.` });
    else setMsg({ ok: true, text: `Resting in the book as ${r.orderId}.` });
  };

  const cancel = async (id: string) => {
    const r = await feed.cancelOrder(id);
    setMsg(r.ok ? { ok: true, text: `Cancelled ${id}.` } : { ok: false, text: r.reason ?? "Couldn't cancel." });
  };

  return (
    <form className="ticket" onSubmit={submit} aria-label="Order ticket">
      {!can && (
        <p className="notice">
          {f.source === "replay"
            ? "You're watching a recording, so there's no exchange to trade against. Run the exchange and gateway locally and this ticket sends real orders."
            : "Connecting to the exchange…"}
        </p>
      )}
      <div className="seg" role="radiogroup" aria-label="Side">
        <button type="button" role="radio" aria-checked={side === "bid"} className={side === "bid" ? "on bid" : ""} onClick={() => setSide("bid")}>
          Buy
        </button>
        <button type="button" role="radio" aria-checked={side === "ask"} className={side === "ask" ? "on ask" : ""} onClick={() => setSide("ask")}>
          Sell
        </button>
      </div>
      <div className="seg" role="radiogroup" aria-label="Order type">
        <button type="button" role="radio" aria-checked={type === "limit"} className={type === "limit" ? "on" : ""} onClick={() => setType("limit")}>
          Limit
        </button>
        <button type="button" role="radio" aria-checked={type === "market"} className={type === "market" ? "on" : ""} onClick={() => setType("market")}>
          Market
        </button>
      </div>
      <label className="field">
        <span>Price</span>
        <input inputMode="decimal" className="num" value={type === "market" ? "" : price} placeholder={type === "market" ? "best available" : "0.00"} disabled={type === "market"} onChange={(e) => setPrice(e.target.value)} />
      </label>
      <label className="field">
        <span>Quantity</span>
        <input inputMode="numeric" className="num" value={qty} onChange={(e) => setQty(e.target.value)} />
      </label>
      <button className={"btn submit " + side} disabled={!can || !valid || busy}>
        {busy ? "Sending…" : `${side === "bid" ? "Buy" : "Sell"} ${q > 0 ? q : ""} ${type === "limit" && p > 0 ? `at ${fmtPrice(p)}` : type === "market" ? "at market" : ""}`.trim()}
      </button>
      <p className={"result" + (msg ? (msg.ok ? " ok" : " bad") : "")} role="status">
        {msg?.text ?? ""}
      </p>

      <div className="mine-block">
        <h4>Your resting orders</h4>
        <div className="open-wrap">
          {open.length === 0 ? (
            <p className="muted small">None. A limit order that doesn't cross the spread will rest here.</p>
          ) : (
            <ul className="open">
              {open.map((o) => (
                <li key={o.id}>
                  <span className={"num " + (o.side === "bid" ? "bid-t" : "ask-t")}>
                    {o.side === "bid" ? "Buy" : "Sell"} {o.remaining} at {fmtPrice(o.price)}
                  </span>
                  <button type="button" className="link" onClick={() => cancel(o.id)}>
                    Cancel
                  </button>
                </li>
              ))}
            </ul>
          )}
        </div>
        <dl className="pos-grid">
          <div>
            <dt>Position</dt>
            <dd className="num">{me ? me.position : 0}</dd>
          </div>
          <div>
            <dt>Profit and loss</dt>
            <dd className={"num " + ((me?.pnl ?? 0) > 0 ? "bid-t" : (me?.pnl ?? 0) < 0 ? "ask-t" : "")}>{fmtSigned(me?.pnl ?? 0)}</dd>
          </div>
        </dl>
      </div>
    </form>
  );
}
