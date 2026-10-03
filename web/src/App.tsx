import { useCallback, useMemo, useState } from "react";
import { feed, useFeed, type WireBatch } from "./feed";
import { computeStats } from "./derive";
import { Scope, type SpotState } from "./components/Scope";
import { Transport } from "./components/Transport";
import { Inspector } from "./components/Inspector";
import { Ladder, PriceChart, Tape } from "./components/Workspace";
import { Terminal, type Prefill } from "./components/Terminal";
import { Bots } from "./components/Bots";
import { Performance } from "./components/Performance";

const REPO = "https://github.com/Maciek-Sikora/Market_Sandbox";

export default function App() {
  const f = useFeed();
  const [selected, setSelected] = useState<WireBatch | null>(null);
  const [spot, setSpot] = useState<SpotState | null>(null);
  const [token, setToken] = useState(0);
  const [prefill, setPrefill] = useState<Prefill | null>(null);

  const idx = f.indexAt(f.vt);
  const { stats, open } = useMemo(() => computeStats(f.frames, idx, f.roster), [f.version, idx, f.roster]);
  const onSpot = useCallback((s: SpotState | null) => setSpot(s), []);

  const spotlight = (b: WireBatch) => {
    setSelected(b);
    setToken((t) => t + 1);
  };

  return (
    <>
      <header className="top">
        <div className="wrap top-in">
          <span className="brand">Market Sandbox</span>
          <StatusPill />
          <a className="top-link" href={REPO}>
            Source on GitHub
          </a>
        </div>
      </header>

      <main>
        <section className="hero wrap">
          <h1>A matching engine in C++, one order at a time</h1>
          <p className="lede">
            Ten bot processes trade against an exchange built from scratch: a lock-free queue, a single-threaded matcher and gRPC at the door. This page is wired to it. Every dot below is a real order, timed by the engine's own clock.
          </p>
          {f.source === "replay" && (
            <p className="banner">
              The exchange isn't running right now, so you're watching a recording of a real session. Run <span className="num">npm run demo</span> in <span className="num">web/</span> to see it live and trade against it.
            </p>
          )}
        </section>

        <section className="wrap scope-wrap" aria-label="Order pipeline">
          <div className="scope">
            <Scope selected={selected} onSelect={setSelected} spotlightToken={token} onSpot={onSpot} />
          </div>
          <ul className="key small">
            <li><span className="sw bid" /> Buy</li>
            <li><span className="sw ask" /> Sell</li>
            <li><span className="sw you" /> Your orders</li>
            <li><span className="sw md" /> Market data</li>
            <li className="muted">Click a dot to inspect that order. The animation stretches microseconds so you can watch them.</li>
          </ul>
          <Transport onStep={() => feed.step()} />
          <Inspector selected={selected} spot={spot} onSelect={setSelected} onSpotlight={spotlight} />
        </section>

        <section className="wrap block" aria-labelledby="trade-h">
          <h2 id="trade-h">Trade against it</h2>
          <p className="sub">
            Place a real order. It goes through the same gRPC door as the ten bots, the C++ engine matches it against their resting orders, and the engine's own answer comes back. Your orders are amber everywhere on the page, including the diagram above. The book, trades and chart follow the timeline, so scrubbing it rewinds them too.
          </p>
          <div className="workspace">
            <div className="panel ladder-panel">
              <h3>Order book</h3>
              <Ladder open={open} onPick={(side, price) => setPrefill({ side, price, n: Date.now() })} />
            </div>
            <div className="mid">
              <div className="panel">
                <h3>Price</h3>
                <PriceChart open={open} />
              </div>
              <div className="panel">
                <h3>Trades</h3>
                <Tape />
              </div>
            </div>
            <div className="panel">
              <h3>Ticket</h3>
              <Terminal prefill={prefill} open={open} me={stats.get("human")} />
            </div>
          </div>
        </section>

        <section className="wrap block" aria-labelledby="bots-h">
          <h2 id="bots-h">The bots</h2>
          <p className="sub">Positions and profit are worked out from the trades in the stream, marked at the last trade price.</p>
          <Bots stats={stats} order={f.roster.map((r) => r.id)} />
        </section>

        <section className="wrap block" aria-labelledby="perf-h">
          <h2 id="perf-h">How fast it is</h2>
          <Performance />
        </section>
      </main>
    </>
  );
}

function StatusPill() {
  const f = useFeed();
  const live = f.source === "live";
  const text = f.source === "connecting" ? "Connecting…" : live ? "Live from the C++ exchange" : "Recorded session";
  return (
    <span className={"pill " + (live ? "live" : f.source === "replay" ? "rec" : "")} role="status">
      <span className="pulse" /> {text}
      {f.recording && <span className="rec-tag"> · recording</span>}
    </span>
  );
}
