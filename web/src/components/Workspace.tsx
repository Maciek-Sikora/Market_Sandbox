import { useMemo } from "react";
import { feed } from "../feed";
import { bookAt, fmtPrice, lastBatchBefore, priceSeries, recentTrades, type OpenOrder } from "../derive";

const ROWS = 8;

export function Ladder({ open, onPick }: { open: OpenOrder[]; onPick: (side: "bid" | "ask", price: number) => void }) {
  const idx = feed.indexAt(feed.vt);
  const book = bookAt(feed.frames, idx);
  const mine = new Set(open.map((o) => `${o.side}:${o.price}`));
  const asks = (book?.asks ?? []).slice(0, ROWS);
  const bids = (book?.bids ?? []).slice(0, ROWS);
  const max = Math.max(1, ...asks.map((l) => l[1]), ...bids.map((l) => l[1]));
  const bestBid = bids[0]?.[0];
  const bestAsk = asks[0]?.[0];

  const empty = (key: string) => (
    <div key={key} className="lad-row empty" aria-hidden="true">
      <span className="num lad-px">–</span>
    </div>
  );
  const row = (l: [number, number, number] | undefined, side: "bid" | "ask", slot: number) =>
    !l ? empty(`${side}-empty-${slot}`) : (
    <button key={`${side}${l[0]}`} className={`lad-row ${side}` + (mine.has(`${side}:${l[0]}`) ? " mine" : "")} onClick={() => onPick(side === "bid" ? "ask" : "bid", l[0])} title={`${side === "bid" ? "Sell" : "Buy"} at ${fmtPrice(l[0])}`}>
      <span className="lad-bar" style={{ width: `${(l[1] / max) * 100}%` }} />
      <span className="num lad-px">{fmtPrice(l[0])}</span>
      <span className="num lad-q">{l[1]}</span>
      <span className="num lad-n">{l[2] > 1 ? `${l[2]} orders` : ""}</span>
    </button>
    );

  return (
    <div className="ladder" aria-label="Order book depth">
      <div className="lad-head">
        <span>Price</span>
        <span>Size</span>
        <span />
      </div>
      <div className="lad-side">{Array.from({ length: ROWS }, (_, i) => row(asks[ROWS - 1 - i], "ask", i))}</div>
      <div className="lad-spread num">
        {bestBid !== undefined && bestAsk !== undefined ? `spread ${fmtPrice(bestAsk - bestBid)}` : book ? "one-sided book" : "empty book"}
      </div>
      <div className="lad-side">{Array.from({ length: ROWS }, (_, i) => row(bids[i], "bid", i))}</div>
      <p className="muted small">Click a level to prefill the ticket with the opposite side.</p>
    </div>
  );
}

export function PriceChart({ open }: { open: OpenOrder[] }) {
  const idx = feed.indexAt(feed.vt);
  const s = useMemo(() => priceSeries(feed.frames, idx, 90_000), [feed.version, idx]);
  const W = 640;
  const H = 230;
  const pad = { l: 8, r: 52, t: 10, b: 20 };

  if (s.trades.length < 2) {
    return <div className="chart empty muted">Waiting for trades…</div>;
  }
  const prices = [...s.trades.map((t) => t.price), ...s.quotes.flatMap((q) => [q.bid, q.ask]).filter((v): v is number => v !== null)];
  let lo = Math.min(...prices);
  let hi = Math.max(...prices);
  const m = Math.max((hi - lo) * 0.12, 0.2);
  lo -= m;
  hi += m;
  const x = (t: number) => pad.l + ((t - s.from) / (s.to - s.from || 1)) * (W - pad.l - pad.r);
  const y = (p: number) => pad.t + (1 - (p - lo) / (hi - lo)) * (H - pad.t - pad.b);

  const line = (pts: { at: number; v: number | null }[]) => {
    let d = "";
    let prev: number | null = null;
    for (const p of pts) {
      if (p.v === null) { prev = null; continue; }
      d += prev === null ? `M${x(p.at)},${y(p.v)}` : `L${x(p.at)},${y(prev)}L${x(p.at)},${y(p.v)}`;
      prev = p.v;
    }
    return d;
  };
  const bidPath = line(s.quotes.map((q) => ({ at: q.at, v: q.bid })));
  const askPath = line(s.quotes.map((q) => ({ at: q.at, v: q.ask })));
  const tradePath = line(s.trades.map((t) => ({ at: t.at, v: t.price })));
  const ticks = [0, 0.25, 0.5, 0.75, 1].map((f) => lo + (hi - lo) * f);

  return (
    <svg className="chart" viewBox={`0 0 ${W} ${H}`} role="img" aria-label={`Price over the last 90 seconds, from ${fmtPrice(Math.min(...prices))} to ${fmtPrice(Math.max(...prices))}`}>
      {ticks.map((p) => (
        <g key={p}>
          <line x1={pad.l} x2={W - pad.r} y1={y(p)} y2={y(p)} className="grid" />
          <text x={W - pad.r + 6} y={y(p) + 4} className="axis num">{fmtPrice(p)}</text>
        </g>
      ))}
      <path d={bidPath} className="q-bid" />
      <path d={askPath} className="q-ask" />
      <path d={tradePath} className="px-line" />
      {s.trades.map((t, i) => (
        <circle key={i} cx={x(t.at)} cy={y(t.price)} r={t.aggressorNode === "human" || t.restingNode === "human" ? 5 : 2.4} className={t.aggressorNode === "human" || t.restingNode === "human" ? "dot-human" : t.aggressorSide === "bid" ? "dot-bid" : "dot-ask"} />
      ))}
      {open.map((o) => (
        <g key={o.id}>
          <line x1={pad.l} x2={W - pad.r} y1={y(o.price)} y2={y(o.price)} className="my-order" />
          <text x={pad.l + 4} y={y(o.price) - 4} className="axis my">{o.side === "bid" ? "your bid" : "your ask"}</text>
        </g>
      ))}
      <text x={pad.l} y={H - 4} className="axis">90 s ago</text>
      <text x={W - pad.r} y={H - 4} textAnchor="end" className="axis">now</text>
    </svg>
  );
}

const TAPE_ROWS = 12;

export function Tape() {
  const idx = feed.indexAt(feed.vt);
  const trades = useMemo(() => recentTrades(feed.frames, idx, TAPE_ROWS), [feed.version, idx]);
  const last = lastBatchBefore(feed.frames, idx, (b) => b.tob !== null);
  return (
    <div className="tape" aria-label="Trade tape">
      <p className="tob num">
        {last?.tob ? (
          <>
            <span className="bid-t">{last.tob.bid !== null ? `${fmtPrice(last.tob.bid)} × ${last.tob.bidQty}` : "no bids"}</span>
            <span className="sep">/</span>
            <span className="ask-t">{last.tob.ask !== null ? `${fmtPrice(last.tob.ask)} × ${last.tob.askQty}` : "no asks"}</span>
          </>
        ) : (
          <span className="muted">No quotes yet</span>
        )}
      </p>
      <ul>
        {Array.from({ length: TAPE_ROWS }, (_, i) => {
          const t = trades[i];
          if (!t) {
            return (
              <li key={`empty-${i}`} className={i === 0 && trades.length === 0 ? "muted" : "empty"}>
                {i === 0 && trades.length === 0 ? "No trades yet." : ""}
              </li>
            );
          }
          return (
            <li key={t.id} className={t.aggressorNode === "human" || t.restingNode === "human" ? "mine" : ""}>
              <span className={"num px " + (t.aggressorSide === "bid" ? "bid-t" : "ask-t")}>{fmtPrice(t.price)}</span>
              <span className="num q">{t.qty}</span>
              <span className="who">
                {t.aggressorNode} {t.aggressorSide === "bid" ? "bought from" : "sold to"} {t.restingNode}
              </span>
            </li>
          );
        })}
      </ul>
    </div>
  );
}
