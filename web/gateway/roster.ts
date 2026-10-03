export interface BotSpec {
  id: string;
  strategy: "market-maker" | "momentum" | "mean-reversion" | "noise" | "replay";
  flags: string[];
}

export const ROSTER: BotSpec[] = [
  { id: "mm-tight", strategy: "market-maker", flags: ["--mm-spread=0.30", "--mm-size=10"] },
  { id: "mm-wide", strategy: "market-maker", flags: ["--mm-spread=1.00", "--mm-size=15"] },
  { id: "mom-fast", strategy: "momentum", flags: ["--mom-window=10", "--mom-threshold-pct=0.0008"] },
  { id: "mom-slow", strategy: "momentum", flags: ["--mom-window=30", "--mom-threshold-pct=0.002"] },
  { id: "mr-1", strategy: "mean-reversion", flags: ["--mr-window=30", "--mr-deviation-pct=0.01"] },
  { id: "noise-1", strategy: "noise", flags: ["--noise-action-prob=0.3"] },
  { id: "noise-2", strategy: "noise", flags: ["--noise-action-prob=0.4", "--noise-size-max=8"] },
  { id: "noise-3", strategy: "noise", flags: ["--noise-action-prob=0.2"] },
  { id: "noise-4", strategy: "noise", flags: ["--noise-action-prob=0.35"] },
  { id: "replay-1", strategy: "replay", flags: ["--replay-speed=10"] },
];
