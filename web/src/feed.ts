import { useSyncExternalStore } from "react";
import type { WireBatch, WireBook } from "../gateway/normalize";

export type { WireBatch, WireBook };
export interface BookFrame {
  t: "book";
  at: number;
  book: WireBook;
}
export type Frame = WireBatch | BookFrame;
export interface RosterEntry {
  id: string;
  strategy: string;
}
export type Source = "connecting" | "live" | "replay";
export type ClockMode = "follow" | "play" | "pause";

const MAX_FRAMES = 20_000;
const TRIM = 2_000;
const GATEWAY_URL = (import.meta.env.VITE_GATEWAY as string | undefined) ?? `ws://${location.hostname || "localhost"}:8787`;
const RECORDING_URL = `${import.meta.env.BASE_URL}recordings/demo-session.ndjson`;

export interface OrderResult {
  ok: boolean;
  orderId?: string;
  status?: string;
  filled?: number;
  avg?: number;
  reason?: string;
}

export class Feed {
  frames: Frame[] = [];
  roster: RosterEntry[] = [];
  source: Source = "connecting";
  exchangeUp = false;
  recording: string | null = null;
  clockMode: ClockMode = "follow";
  speed = 1;
  vt = 0;
  version = 0;

  private cursor = 0;
  private forced: Frame[] = [];
  private anchor = { wall: 0, vt: 0 };
  private listeners = new Set<() => void>();
  private dirty = false;
  private lastNotify = 0;
  private ws: WebSocket | null = null;
  private reqId = 0;
  private pending = new Map<number, (r: OrderResult) => void>();
  private retry: number | undefined;

  subscribe = (fn: () => void) => {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  };
  getSnapshot = () => this.version;
  private bump() {
    this.version++;
    this.lastNotify = performance.now();
    this.dirty = false;
    for (const l of this.listeners) l();
  }

  get firstAt() {
    return this.frames.length ? this.frames[0].at : 0;
  }
  get lastAt() {
    return this.frames.length ? this.frames[this.frames.length - 1].at : 0;
  }
  indexAt(vt: number): number {
    let lo = 0;
    let hi = this.frames.length - 1;
    let ans = -1;
    while (lo <= hi) {
      const mid = (lo + hi) >> 1;
      if (this.frames[mid].at <= vt) {
        ans = mid;
        lo = mid + 1;
      } else hi = mid - 1;
    }
    return ans;
  }
  get atLive() {
    return this.source === "live" && this.clockMode === "follow";
  }

  private push(frames: Frame[]) {
    for (const f of frames) this.frames.push(f);
    if (this.frames.length > MAX_FRAMES + TRIM) {
      this.frames.splice(0, TRIM);
      this.cursor = Math.max(0, this.cursor - TRIM);
    }
    this.dirty = true;
  }

  private reset(source: Source) {
    this.frames = [];
    this.cursor = 0;
    this.source = source;
    this.dirty = true;
  }

  tick(now: number) {
    if (this.clockMode === "follow") {
      const last = this.lastAt;
      if (last !== this.vt) {
        this.vt = last;
        this.dirty = true;
      }
    } else if (this.clockMode === "play") {
      this.vt = this.anchor.vt + (now - this.anchor.wall) * this.speed;
      if (this.source === "replay" && this.frames.length && this.vt > this.lastAt + 1500) {
        this.vt = this.firstAt;
        this.cursor = 0;
        this.anchor = { wall: now, vt: this.vt };
      } else if (this.source === "live" && this.vt >= this.lastAt) {
        this.setFollow();
      }
      this.dirty = true;
    }
    if (this.dirty && now - this.lastNotify > 90) this.bump();
  }

  takeNew(): Frame[] {
    const out: Frame[] = this.forced;
    this.forced = [];
    if (this.clockMode === "pause") return out;
    const limit = this.clockMode === "follow" ? Infinity : this.vt;
    while (this.cursor < this.frames.length && this.frames[this.cursor].at <= limit) {
      out.push(this.frames[this.cursor++]);
    }
    return out;
  }

  private setFollow() {
    this.clockMode = "follow";
    this.speed = 1;
    this.vt = this.lastAt;
    this.cursor = this.frames.length;
    this.dirty = true;
  }

  goLive() {
    if (this.source === "live") this.setFollow();
    else {
      this.clockMode = "play";
      this.speed = 1;
      this.seek(this.firstAt);
      this.anchor = { wall: performance.now(), vt: this.vt };
    }
    this.bump();
  }

  togglePlay() {
    if (this.clockMode === "pause") {
      if (this.source === "live" && this.vt >= this.lastAt) this.setFollow();
      else {
        this.clockMode = "play";
        this.anchor = { wall: performance.now(), vt: this.vt };
      }
    } else {
      this.clockMode = "pause";
    }
    this.bump();
  }

  setSpeed(s: number) {
    this.speed = s;
    if (this.clockMode === "follow") {
      if (s === 1) return;
      this.clockMode = "play";
    }
    if (this.clockMode === "play") this.anchor = { wall: performance.now(), vt: this.vt };
    this.bump();
  }

  seek(vt: number) {
    this.vt = Math.min(Math.max(vt, this.firstAt), this.lastAt);
    this.cursor = this.indexAt(this.vt) + 1;
    if (this.clockMode === "follow") this.clockMode = "pause";
    if (this.clockMode === "play") this.anchor = { wall: performance.now(), vt: this.vt };
    this.bump();
  }

  step(): Frame[] {
    this.clockMode = "pause";
    while (this.cursor < this.frames.length) {
      const f = this.frames[this.cursor++];
      this.vt = f.at;
      if (f.t === "batch") {
        this.forced.push(f);
        this.bump();
        return [f];
      }
    }
    this.bump();
    return [];
  }

  start() {
    this.connect();
    window.setTimeout(() => {
      if (this.source === "connecting") void this.useReplay();
    }, 2500);
  }

  private connect() {
    let ws: WebSocket;
    try {
      ws = new WebSocket(GATEWAY_URL);
    } catch {
      this.scheduleRetry();
      return;
    }
    this.ws = ws;
    ws.onmessage = (ev) => this.onMessage(JSON.parse(ev.data as string));
    ws.onclose = () => {
      this.ws = null;
      this.exchangeUp = false;
      if (this.source === "live") {
        this.source = "replay";
        if (this.frames.length === 0) void this.useReplay();
        else if (this.clockMode === "follow") this.clockMode = "pause";
      }
      this.bump();
      this.scheduleRetry();
    };
    ws.onerror = () => ws.close();
  }

  private scheduleRetry() {
    window.clearTimeout(this.retry);
    this.retry = window.setTimeout(() => this.connect(), 4000);
  }

  private onMessage(m: any) {
    switch (m.t) {
      case "hello":
        this.roster = m.roster;
        break;
      case "status":
        this.recording = m.recording;
        this.exchangeUp = m.exchange === "connected";
        if (this.exchangeUp && this.source !== "live") this.switchToLive();
        else if (!this.exchangeUp && this.source === "live" && this.frames.length === 0) void this.useReplay();
        break;
      case "history":
        if (this.source === "live" && this.frames.length === 0) {
          this.push(m.batches);
          this.setFollow();
        }
        break;
      case "batch":
        if (this.source === "live") this.push([m]);
        break;
      case "book":
        if (this.source === "live") this.push([m]);
        break;
      case "result": {
        const cb = this.pending.get(m.reqId);
        if (cb) {
          this.pending.delete(m.reqId);
          cb(m);
        }
        break;
      }
    }
    this.dirty = true;
  }

  private switchToLive() {
    this.reset("live");
    this.clockMode = "follow";
    this.speed = 1;
    this.vt = 0;
    this.bump();
  }

  async useReplay() {
    if (this.source === "live") return;
    try {
      const res = await fetch(RECORDING_URL);
      if (!res.ok) throw new Error(String(res.status));
      const lines = (await res.text()).split("\n").filter(Boolean).map((l) => JSON.parse(l));
      const frames: Frame[] = [];
      for (const m of lines) {
        if (m.t === "hello") this.roster = m.roster;
        else if (m.t === "batch" || m.t === "book") frames.push(m);
      }
      if ((this.source as Source) === "live") return;
      this.reset("replay");
      this.push(frames);
      this.clockMode = "play";
      this.speed = 1;
      this.vt = this.firstAt;
      this.anchor = { wall: performance.now(), vt: this.vt };
    } catch {
      this.source = "replay";
    }
    this.bump();
  }

  canTrade() {
    return this.source === "live" && this.exchangeUp && this.ws?.readyState === WebSocket.OPEN;
  }

  private request(msg: Record<string, unknown>): Promise<OrderResult> {
    return new Promise((resolve) => {
      if (!this.canTrade()) return resolve({ ok: false, reason: "The exchange isn't running, so there's nothing to trade against." });
      const reqId = ++this.reqId;
      const timer = window.setTimeout(() => {
        this.pending.delete(reqId);
        resolve({ ok: false, reason: "No answer from the exchange." });
      }, 4000);
      this.pending.set(reqId, (r) => {
        window.clearTimeout(timer);
        resolve(r);
      });
      this.ws!.send(JSON.stringify({ ...msg, reqId }));
    });
  }

  placeOrder(side: "bid" | "ask", type: "market" | "limit", price: number, qty: number) {
    return this.request({ t: "order", side, type, price, qty });
  }
  cancelOrder(orderId: string) {
    return this.request({ t: "cancel", orderId });
  }
  setRecording(on: boolean) {
    if (this.canTrade()) this.ws!.send(JSON.stringify({ t: "record", on }));
  }
}

export const feed = new Feed();

export function useFeed(): Feed {
  useSyncExternalStore(feed.subscribe, feed.getSnapshot);
  return feed;
}
