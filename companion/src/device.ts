// One knob, as the UI sees it: connection, the latest settings / state / SYS INFO, the app
// profiles with their icons, and a short history for the charts. Views subscribe and redraw.

import { Cmd, Tag, decode, encode, ICON_BYTES, type Hello, type Profile, type SetId, type Settings, type State, type SysA, type SysB } from "./proto";
import type { Transport } from "./transport";

export type Status = "searching" | "needs-permission" | "connected" | "unsupported";

export interface ProfileEntry extends Profile {
  icon: ImageData | null; // 48x48, decoded from the device's RGB565
}

export const HISTORY_SECONDS = 60;
export interface History {
  totalMa: number[];
  chipC: number[];
  load0: number[];
  load1: number[];
}

const STREAM_HZ = 30;
const SEARCH_MS = 1000;

export class Device {
  status: Status = "searching";
  hello: Hello | null = null;
  settings: Settings | null = null;
  state: State | null = null;
  sysA: SysA | null = null;
  sysB: SysB | null = null;
  profiles: ProfileEntry[] = [];
  history: History = { totalMa: [], chipC: [], load0: [], load1: [] };
  error: string | null = null;

  private listeners = new Set<() => void>();
  private iconBuf = new Map<number, Uint8Array>();
  private searchTimer: number | undefined;

  constructor(private transport: Transport | null) {
    if (!transport) {
      this.status = "unsupported";
      return;
    }
    transport.onReport = (r) => this.onReport(r);
    transport.onClosed = () => this.onClosed();
    this.search();
  }

  get kind() {
    return this.transport?.kind ?? null;
  }

  subscribe(fn: () => void): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  private changed() {
    for (const fn of this.listeners) fn();
  }

  // --- connection ---

  private search() {
    window.clearTimeout(this.searchTimer);
    if (!this.transport || this.status === "connected") return;
    this.transport
      .tryConnect()
      .then((ok) => {
        if (ok) return this.start();
        if (this.transport!.needsGesture && this.status !== "needs-permission") {
          this.status = "needs-permission";
          this.changed();
        }
        this.searchTimer = window.setTimeout(() => this.search(), SEARCH_MS);
      })
      .catch((e) => {
        this.error = String(e);
        this.searchTimer = window.setTimeout(() => this.search(), SEARCH_MS);
      });
  }

  // WebHID: the device picker (from a click).
  async requestAccess() {
    if (this.transport?.request && (await this.transport.request())) await this.start();
  }

  private async start() {
    this.status = "connected";
    this.error = null;
    this.profiles = [];
    this.iconBuf.clear();
    this.changed();
    await this.send(encode.hello());
    await this.send(encode.getSettings());
    await this.send(encode.stream(STREAM_HZ));
  }

  private onClosed() {
    this.status = "searching";
    this.hello = this.settings = this.state = this.sysA = this.sysB = null;
    this.changed();
    this.search();
  }

  private async send(r: Uint8Array) {
    try {
      await this.transport?.send(r);
    } catch (e) {
      this.error = String(e);
      this.changed();
    }
  }

  // --- commands ---

  set(id: SetId, value: number) {
    return this.send(encode.set(id, value));
  }
  save() {
    return this.send(encode.save());
  }
  revert() {
    return this.send(encode.revert());
  }
  resetPeaks() {
    return this.send(encode.resetPeaks());
  }

  // --- replies ---

  private onReport(r: Uint8Array) {
    const m = decode(r);
    switch (m.tag) {
      case Tag.HELLO:
        if ("hello" in m) {
          this.hello = m.hello;
          // Profiles one by one; each reply asks for the next (and its icon).
          if (m.hello.profileCount > 0) this.send(encode.profile(0));
        }
        break;
      case Tag.SETTINGS:
        if ("settings" in m) this.settings = m.settings;
        break;
      case Tag.PROFILE:
        if ("profile" in m) this.onProfile(m.profile);
        return; // changed() once its icon has arrived
      case Tag.PROFILE_ICON:
        if ("chunk" in m) this.onIconChunk(m.chunk.index, m.chunk.offset, m.chunk.bytes);
        return;
      case Tag.STATE:
        if ("state" in m) this.state = m.state;
        break;
      case Tag.SYS_A:
        if ("sys" in m) {
          this.sysA = m.sys as SysA;
          this.push("totalMa", this.sysA.totalMa);
          this.push("chipC", this.sysA.chipC);
        }
        break;
      case Tag.SYS_B:
        if ("sys" in m) {
          this.sysB = m.sys as SysB;
          this.push("load0", this.sysB.load[0]);
          this.push("load1", this.sysB.load[1]);
        }
        break;
      case Tag.ERROR:
        if ("cmd" in m) this.error = `device refused command 0x${m.cmd.toString(16)} (${m.code})`;
        break;
      default:
        return;
    }
    this.changed();
  }

  // SYS arrives twice a second: HISTORY_SECONDS worth of samples.
  private push(key: keyof History, v: number) {
    const a = this.history[key];
    a.push(v);
    if (a.length > HISTORY_SECONDS * 2) a.shift();
  }

  private onProfile(p: Profile) {
    this.profiles[p.index] = { ...p, icon: null };
    if (p.hasIcon) {
      this.iconBuf.set(p.index, new Uint8Array(ICON_BYTES));
      this.send(encode.profileIcon(p.index, 0));
    } else {
      this.nextProfile(p.index);
    }
    this.changed();
  }

  private onIconChunk(index: number, offset: number, bytes: Uint8Array) {
    const buf = this.iconBuf.get(index);
    if (!buf) return;
    buf.set(bytes, offset);
    const next = offset + bytes.length;
    if (next < ICON_BYTES && bytes.length > 0) {
      this.send(encode.profileIcon(index, next));
      return;
    }
    if (this.profiles[index]) this.profiles[index].icon = rgb565ToImage(buf, 48, 48);
    this.iconBuf.delete(index);
    this.nextProfile(index);
    this.changed();
  }

  private nextProfile(index: number) {
    if (this.hello && index + 1 < this.hello.profileCount) this.send(encode.profile(index + 1));
  }
}

// 48x48 RGB565, big-endian (what the firmware draws with swap565_t).
function rgb565ToImage(b: Uint8Array, w: number, h: number): ImageData {
  const img = new ImageData(w, h);
  for (let i = 0; i < w * h; i++) {
    const v = (b[i * 2] << 8) | b[i * 2 + 1];
    const r = (v >> 11) & 0x1f, g = (v >> 5) & 0x3f, bl = v & 0x1f;
    img.data[i * 4] = (r << 3) | (r >> 2);
    img.data[i * 4 + 1] = (g << 2) | (g >> 4);
    img.data[i * 4 + 2] = (bl << 3) | (bl >> 2);
    img.data[i * 4 + 3] = 255;
  }
  return img;
}

export { Cmd };
