import { useMemo } from "react";
import { feed, type WireBatch } from "../feed";
import { fmtPrice, fmtUs, isBatch } from "../derive";
import { boundaries, type SpotState } from "./Scope";

const STAGE_COPY = [
  { name: "gRPC handler", color: "#4f7cff", text: "A gRPC thread decodes the request, assigns the order ID and pushes it onto the queue." },
  { name: "Waiting in the queue", color: "#7fb2ff", text: "The engine thread hasn't reached it yet. Producers never block each other, so this is pure waiting." },
  { name: "Matching", color: "#2fb98f", text: "The engine walks the opposite side from the best price, oldest order first, and records each trade." },
  { name: "Reply", color: "#e0a030", text: "The result goes back through a promise, which wakes the gRPC thread that has been blocked on it." },
  { name: "Publishing", color: "#56d4ff", text: "Trades and the new top of book are copied into every subscriber's own queue." },
];

export function stageDurations(b: WireBatch) {
  const t = b.trace;
  return [t.enqueue, t.dequeue - t.enqueue, t.matchDone - t.dequeue, t.promiseSet - t.matchDone, t.publish - t.promiseSet].map((v) => Math.max(0, v));
}

export function describe(b: WireBatch) {
  const o = b.order;
  if (b.op === "cancel") return `${o.node === "human" ? "You" : o.node} cancel ${o.id}`;
  const who = o.node === "human" ? "You" : o.node;
  const px = o.type === "limit" ? ` at ${fmtPrice(o.price)}` : "";
  return `${who} ${o.side === "bid" ? "buy" : "sell"} ${o.qty} ${o.type}${px}`;
}

function outcome(b: WireBatch) {
  const o = b.order;
  if (b.op === "cancel") return o.cancelOk ? "Cancelled" : "Not found";
  if (o.status === "rejected") return o.reason || "Rejected";
  if (o.status === "filled") return `Filled ${o.filled} at avg ${fmtPrice(o.avg)}`;
  if (o.status === "partial") return `Partly filled ${o.filled}, ${o.qty - o.filled} resting`;
  return "Resting in the book";
}

function traceRow(s: SpotState) {
  const [s0, s1, s2, s3] = boundaries(s.frame);
  const p = s.p;
  if (p < s1 || p < s0) return 0;
  if (p < s2) return 1;
  if (p < s3) return 2;
  return p < s3 + (1 - s3) * 0.5 ? 3 : 4;
}

interface Props {
  selected: WireBatch | null;
  spot: SpotState | null;
  onSelect: (b: WireBatch) => void;
  onSpotlight: (b: WireBatch) => void;
}

export function Inspector({ selected, spot, onSelect, onSpotlight }: Props) {
  const idx = feed.indexAt(feed.vt);
  const recent = useMemo(() => {
    const out: WireBatch[] = [];
    for (let i = idx; i >= 0 && out.length < 9; i--) {
      const f = feed.frames[i];
      if (isBatch(f)) out.push(f);
    }
    return out;
  }, [feed.version, idx]);

  const cur = selected ?? recent[0] ?? null;
  const durs = cur ? stageDurations(cur) : [];
  const total = cur ? Math.max(cur.trace.publish, 1) : 1;
  const row = spot && cur && spot.frame.seq === cur.seq && spot.frame.at === cur.at ? traceRow(spot) : -1;

  return (
    <div className="inspector">
      <div className="recent" role="listbox" aria-label="Recent orders">
        {recent.length === 0 && <p className="muted">No orders yet. Start the bots or place one yourself.</p>}
        {recent.map((b) => (
          <button
            key={`${b.seq}-${b.at}`}
            role="option"
            aria-selected={cur === b}
            className={"recent-row" + (cur === b ? " on" : "") + (b.order.node === "human" ? " human" : "")}
            onClick={() => onSelect(b)}
          >
            <span className={"dot " + (b.op === "cancel" ? "cx" : b.order.side)} />
            <span className="num id">{b.order.id}</span>
            <span className="what">{describe(b)}</span>
          </button>
        ))}
      </div>

      <div className="trace">
        {cur && (
          <>
            <div className="trace-head">
              <div>
                <h3>{describe(cur)}</h3>
                <p className="muted">
                  {outcome(cur)}
                  {cur.trades.length > 0 && `, ${cur.trades.length} trade${cur.trades.length > 1 ? "s" : ""}`}
                </p>
              </div>
              <button className="btn accent" onClick={() => onSpotlight(cur)}>
                Follow this order in slow motion
              </button>
            </div>
            <div className="waterfall" aria-label="Time spent in each stage">
              {durs.map((d, i) => {
                const left = durs.slice(0, i).reduce((a, b) => a + b, 0);
                return (
                  <div key={i} className={"wf-row" + (row === i ? " now" : "")}>
                    <span className="wf-name">{STAGE_COPY[i].name}</span>
                    <span className="wf-track">
                      <span className="wf-bar" style={{ left: `${(left / total) * 100}%`, width: `${Math.max((d / total) * 100, 1.2)}%`, background: STAGE_COPY[i].color }} />
                    </span>
                    <span className="num wf-val">{fmtUs(d)}</span>
                  </div>
                );
              })}
            </div>
            <p className="stage-copy" aria-live="polite">
              {row >= 0 ? (
                <>
                  <strong>{STAGE_COPY[row].name}.</strong> {STAGE_COPY[row].text}
                </>
              ) : (
                <>
                  <strong>{fmtUs(cur.trace.promiseSet)} inside the server</strong> from the call arriving to the reply being released. These timings come from the engine's own clock; the diagram above stretches them so you can see them.
                </>
              )}
            </p>
          </>
        )}
      </div>
    </div>
  );
}
