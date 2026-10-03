import { useFeed } from "../feed";

const SPEEDS = [0.25, 0.5, 1, 2, 4];

function clock(ms: number) {
  const s = Math.max(0, Math.round(ms / 1000));
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`;
}

export function Transport({ onStep }: { onStep: () => void }) {
  const f = useFeed();
  const span = Math.max(f.lastAt - f.firstAt, 1);
  const paused = f.clockMode === "pause";
  const behind = f.source === "live" ? Math.max(0, f.lastAt - f.vt) : 0;

  return (
    <div className="transport" role="group" aria-label="Playback controls">
      <button className="btn icon" onClick={() => f.togglePlay()} aria-label={paused ? "Play" : "Pause"}>
        {paused ? "▶" : "❚❚"}
      </button>
      <button className="btn" onClick={onStep} disabled={!paused} title="Advance one order">
        Step
      </button>
      <div className="speeds" role="radiogroup" aria-label="Playback speed">
        {SPEEDS.map((s) => (
          <button key={s} role="radio" aria-checked={f.speed === s} className={"chip" + (f.speed === s ? " on" : "")} onClick={() => f.setSpeed(s)}>
            {s}×
          </button>
        ))}
      </div>
      <input
        className="scrub"
        type="range"
        min={f.firstAt}
        max={f.firstAt + span}
        step={1}
        value={f.vt || f.firstAt}
        onChange={(e) => f.seek(Number(e.target.value))}
        aria-label="Position in the recorded window"
        disabled={f.frames.length < 2}
      />
      <span className="pos num">
        {f.frames.length < 2 ? "waiting for orders" : f.atLive ? "live" : `${clock(f.vt - f.firstAt)} / ${clock(span)}`}
      </span>
      {f.source === "live" ? (
        <button className={"btn" + (f.atLive ? " ghost" : " accent")} onClick={() => f.goLive()} disabled={f.atLive}>
          {f.atLive ? "Live" : `Back to live${behind > 1500 ? ` (${clock(behind)} behind)` : ""}`}
        </button>
      ) : (
        <button className="btn" onClick={() => f.goLive()}>
          Restart recording
        </button>
      )}
    </div>
  );
}
