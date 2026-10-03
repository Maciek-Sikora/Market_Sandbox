import { useMemo } from "react";
import { feed } from "../feed";
import { fmtUs, isBatch, percentile } from "../derive";

const LATENCY = [
  { label: "p50", off: 150, on: 200 },
  { label: "p90", off: 214, on: 267 },
  { label: "p99", off: 295, on: 352 },
];
const QUEUE = [
  { label: "1 producer", v: 110.2 },
  { label: "4 producers", v: 19.6 },
];
const GRPC = [
  { label: "1 caller", off: 6110, on: 4820 },
  { label: "8 callers", off: 14800, on: 13590 },
];

function Bars({ rows, unit, max }: { rows: { label: string; a: number; b?: number }[]; unit: string; max: number }) {
  return (
    <div className="bars">
      {rows.map((r) => (
        <div className="bar-row" key={r.label}>
          <span className="bar-label">{r.label}</span>
          <span className="bar-tracks">
            <span className="bar a" style={{ width: `${(r.a / max) * 100}%` }} />
            {r.b !== undefined && <span className="bar b" style={{ width: `${(r.b / max) * 100}%` }} />}
          </span>
          <span className="num bar-val">
            {r.a.toLocaleString()}
            {r.b !== undefined && <> / {r.b.toLocaleString()}</>} {unit}
          </span>
        </div>
      ))}
    </div>
  );
}

export function Performance() {
  const live = useMemo(() => {
    const totals: number[] = [];
    const waits: number[] = [];
    for (let i = feed.frames.length - 1; i >= 0 && totals.length < 3000; i--) {
      const f = feed.frames[i];
      if (!isBatch(f)) continue;
      totals.push(f.trace.promiseSet);
      waits.push(f.trace.dequeue - f.trace.enqueue);
    }
    totals.sort((a, b) => a - b);
    waits.sort((a, b) => a - b);
    return { n: totals.length, p50: percentile(totals, 50), p99: percentile(totals, 99), w50: percentile(waits, 50), w99: percentile(waits, 99) };
  }, [Math.floor(feed.version / 20)]);

  return (
    <div className="perf">
      <section className="perf-live">
        <h3>From this session</h3>
        <p className="muted">Measured by the engine on the last {live.n.toLocaleString()} orders it handled, from the gRPC call arriving to the reply being released.</p>
        <dl className="kpis">
          <div>
            <dt>Median</dt>
            <dd className="num">{live.n ? fmtUs(live.p50) : "–"}</dd>
          </div>
          <div>
            <dt>99th percentile</dt>
            <dd className="num">{live.n ? fmtUs(live.p99) : "–"}</dd>
          </div>
          <div>
            <dt>Median wait in queue</dt>
            <dd className="num">{live.n ? fmtUs(live.w50) : "–"}</dd>
          </div>
          <div>
            <dt>99th percentile wait</dt>
            <dd className="num">{live.n ? fmtUs(live.w99) : "–"}</dd>
          </div>
        </dl>
      </section>

      <section>
        <h3>Round trip for one order, over gRPC</h3>
        <p className="muted">Microseconds, client side, 3,000 sequential calls. Left value: nobody watching. Right value: with a telemetry subscriber attached, which is what feeds this page.</p>
        <Bars rows={LATENCY.map((r) => ({ label: r.label, a: r.off, b: r.on }))} unit="µs" max={400} />
        <Legend a="Not watched" b="Watched" />
      </section>

      <section>
        <h3>Orders per second through gRPC</h3>
        <Bars rows={GRPC.map((r) => ({ label: r.label, a: r.off, b: r.on }))} unit="/s" max={16000} />
        <Legend a="Not watched" b="Watched" />
      </section>

      <section>
        <h3>The queue on its own</h3>
        <p className="muted">Millions of enqueue and dequeue operations per second, one consumer. Many producers contend for the same tail, so the rate drops as they are added.</p>
        <Bars rows={QUEUE.map((r) => ({ label: r.label, a: r.v }))} unit="M/s" max={120} />
      </section>
    </div>
  );
}

function Legend({ a, b }: { a: string; b: string }) {
  return (
    <p className="legend small">
      <span className="sw a" /> {a} <span className="sw b" /> {b}
    </p>
  );
}
