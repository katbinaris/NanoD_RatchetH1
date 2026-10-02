// One knob, as the UI sees it: connection, the latest settings / state / SYS INFO, the app
// profiles with their icons, and a short history for the charts. Views subscribe and redraw.

import { Cmd, EXT_NET_VERSION, ExtCmd, ExtStatus, ExtTag, NetOp, LED_COUNT, Res, RES_TEXT, TEXT_CHUNK, Tag, crc32, decode, encode, ICON_BYTES, UploadFlag, type Hello, type Lights, type Net, type Prefs, type Profile, type Result, type SetId, type Settings, type State, type SysA, type SysB } from "./proto";
import { rgb565ToImage, tidy, type ProfileJson } from "./profile";
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
const TRANSFER_MS = 8000;
const PREFS_MS = 1000; // LIGHTS and the idle word can change on the knob too (its menu)
const LIGHTS_RETRY_MS = 60; // the knob takes one LIGHTS at a time

export class DeviceError extends Error {}

export class Device {
  status: Status = "searching";
  hello: Hello | null = null;
  settings: Settings | null = null;
  state: State | null = null;
  sysA: SysA | null = null;
  sysB: SysB | null = null;
  profiles: ProfileEntry[] = [];
  history: History = { totalMa: [], chipC: [], load0: [], load1: [] };
  // What the LEDs show (RGB, as sent to the strips), and when that last arrived -- firmware
  // before the LED stream never sends it, and the view makes it up instead.
  leds = new Uint8Array(LED_COUNT * 3);
  ledsAt = 0;
  // ext_proto.h: its version (0 = firmware without the extensions, null = not known yet), and
  // LIGHTS + the idle word.
  ext: number | null = null;
  prefs: Prefs | null = null;
  net: Net | null = null; // WiFi, from extensions v4
  error: string | null = null;

  private listeners = new Set<() => void>();
  private iconBuf = new Map<number, Uint8Array>();
  // On a list reload, icons are fetched again only for these (all when null).
  private staleIcons: globalThis.Set<number> | null = null;
  private searchTimer: number | undefined;
  // Profile transfers run one at a time; each waits for its own reply.
  private chain: Promise<unknown> = Promise.resolve();
  private download: { index: number; buf: Uint8Array | null; crc: number; got: number; done: (t: string) => void; fail: (e: Error) => void } | null = null;
  private waitResult: { cmd: number; done: (r: Result) => void } | null = null;
  private prefsTimer: number | undefined;
  private lightsSent: { l: Lights; save: boolean; retried: boolean } | null = null;

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
    this.staleIcons = null;
    this.iconBuf.clear();
    this.changed();
    await this.send(encode.hello());
    await this.send(encode.getSettings());
    await this.send(encode.stream(STREAM_HZ));
    await this.send(encode.extHello());
  }

  private onClosed() {
    this.status = "searching";
    this.hello = this.settings = this.state = this.sysA = this.sysB = null;
    this.ext = this.prefs = this.net = null;
    window.clearInterval(this.prefsTimer);
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
  // SAVE and REVERT cover LIGHTS too (menu_remote_save / _revert).
  async save() {
    await this.send(encode.save());
    if (this.ext) await this.send(encode.extPrefs());
  }
  async revert() {
    await this.send(encode.revert());
    if (this.ext) await this.send(encode.extPrefs());
  }
  setIdleText(text: string) {
    return this.send(encode.idleText(text));
  }
  // WiFi: a new network (`ssid` + `password`, "" = open), or on / off with the stored one.
  async setWifi(on: boolean, ssid?: string, password?: string) {
    if (ssid !== undefined) {
      const pw = new Uint8Array(64);
      pw.set(new TextEncoder().encode(password ?? "").subarray(0, 63));
      await this.send(encode.net(NetOp.SSID, new TextEncoder().encode(ssid)));
      await this.send(encode.net(NetOp.PASS_A, pw.subarray(0, 32)));
      await this.send(encode.net(NetOp.PASS_B, pw.subarray(32, 64)));
    }
    const r = encode.net(NetOp.APPLY);
    r[2] = on ? 1 : 0;
    await this.send(r);
  }
  setLights(l: Lights, save = false) {
    this.lightsSent = { l: { ...l }, save, retried: false };
    return this.send(encode.lights(l, save));
  }
  // The shown haptic profile back to its factory feel and values (live, not saved).
  resetHaptic() {
    return this.send(encode.hapticReset());
  }
  resetPeaks() {
    return this.send(encode.resetPeaks());
  }

  // --- profiles (JSON, profile.ts) ---

  private serial<T>(job: () => Promise<T>): Promise<T> {
    const run = this.chain.then(job, job);
    this.chain = run.catch(() => undefined);
    return run;
  }

  private timeout<T>(p: Promise<T>, what: string): Promise<T> {
    return new Promise<T>((resolve, reject) => {
      const t = window.setTimeout(() => {
        this.download = null;
        this.waitResult = null;
        reject(new DeviceError(`${what}: NO ANSWER FROM THE KNOB`));
      }, TRANSFER_MS);
      p.then(
        (v) => (window.clearTimeout(t), resolve(v)),
        (e) => (window.clearTimeout(t), reject(e)),
      );
    });
  }

  // The whole profile at `index`, as the device has it right now (live edit included).
  readProfile(index: number): Promise<ProfileJson> {
    return this.serial(() =>
      this.timeout(
        new Promise<string>((done, fail) => {
          this.download = { index, buf: null, crc: 0, got: 0, done, fail };
          this.send(encode.profileRead(index));
        }),
        "READING THE PROFILE",
      ).then((text) => JSON.parse(text) as ProfileJson),
    );
  }

  private result(cmd: number, send: () => Promise<void>, what: string): Promise<Result> {
    return this.timeout(
      new Promise<Result>((done) => {
        this.waitResult = { cmd, done };
        void send();
      }),
      what,
    ).then((r) => {
      if (r.res !== Res.OK) throw new DeviceError(r.why ? `${RES_TEXT[r.res] ?? "FAILED"}: ${r.why.toUpperCase()}` : (RES_TEXT[r.res] ?? "FAILED"));
      this.reloadProfiles(r.removed ? undefined : r.index);
      return r;
    });
  }

  // Live on the knob right away; `save` stores it too.
  uploadProfile(p: ProfileJson, save: boolean): Promise<Result> {
    return this.serial(() => {
      const bytes = new TextEncoder().encode(JSON.stringify(tidy(p)));
      return this.result(
        Cmd.UPLOAD_END,
        async () => {
          await this.send(encode.uploadBegin(bytes.length, crc32(bytes), save ? UploadFlag.SAVE : 0));
          for (let off = 0; off < bytes.length; off += TEXT_CHUNK) await this.send(encode.uploadData(off, bytes.subarray(off, off + TEXT_CHUNK)));
          await this.send(encode.uploadEnd());
        },
        "SENDING THE PROFILE",
      );
    });
  }

  profileOp(index: number, op: number): Promise<Result> {
    return this.serial(() => this.result(Cmd.PROFILE_OP, () => this.send(encode.profileOp(index, op)), "PROFILE"));
  }

  // The list again (names, flags), after a change -- icons only for `changed` (all if not given:
  // a removal moves the ones after it).
  reloadProfiles(changed?: number) {
    this.iconBuf.clear();
    this.staleIcons = changed === undefined ? null : new globalThis.Set([changed]);
    return this.send(encode.hello());
  }

  // --- replies ---

  private onReport(r: Uint8Array) {
    const m = decode(r);
    switch (m.tag) {
      case Tag.HELLO:
        if ("hello" in m) {
          this.hello = m.hello;
          this.profiles.length = Math.min(this.profiles.length, m.hello.profileCount);
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
      case Tag.LEDS:
        if ("rgb" in m && m.first * 3 + m.rgb.length <= this.leds.length) {
          this.leds.set(m.rgb, m.first * 3);
          this.ledsAt = performance.now();
        }
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
      case Tag.PROFILE_BEGIN:
        if ("length" in m && this.download && m.index === this.download.index) {
          this.download.buf = new Uint8Array(m.length);
          this.download.crc = m.crc;
          this.download.got = 0;
          if (m.length === 0) this.finishDownload();
        }
        return;
      case Tag.PROFILE_DATA:
        if ("offset" in m && this.download?.buf) {
          const d = this.download, buf = d.buf!;
          if (m.offset !== d.got) {
            this.download = null;
            d.fail(new DeviceError("PROFILE ARRIVED OUT OF ORDER"));
            return;
          }
          const n = Math.min(TEXT_CHUNK, buf.length - m.offset);
          buf.set(m.bytes.subarray(0, n), m.offset);
          d.got += n;
          if (d.got >= buf.length) this.finishDownload();
        }
        return;
      case Tag.RESULT:
        if ("result" in m && this.waitResult && this.waitResult.cmd === m.result.cmd) {
          const w = this.waitResult;
          this.waitResult = null;
          w.done(m.result);
        }
        return;
      case ExtTag.HELLO:
        if ("ext" in m) {
          this.ext = m.ext;
          window.clearInterval(this.prefsTimer);
          const poll = () => {
            void this.send(encode.extPrefs());
            if ((this.ext ?? 0) >= EXT_NET_VERSION) void this.send(encode.net(NetOp.STATUS));
          };
          this.prefsTimer = window.setInterval(poll, PREFS_MS);
          poll();
        }
        break;
      case ExtTag.PREFS:
        if ("prefs" in m) this.prefs = m.prefs;
        break;
      case ExtTag.NET:
        if ("net" in m) this.net = m.net;
        break;
      case ExtTag.ACK:
        if ("status" in m) {
          const sent = this.lightsSent;
          if (m.cmd === ExtCmd.LIGHTS && m.status === ExtStatus.BAD_PARAM && sent && !sent.retried) {
            sent.retried = true; // the knob was still applying the one before
            window.setTimeout(() => void this.send(encode.lights(sent.l, sent.save)), LIGHTS_RETRY_MS);
            return;
          }
          if (m.status !== ExtStatus.OK) this.error = m.status === ExtStatus.STORAGE ? "THE KNOB COULDN'T STORE THAT" : `the knob refused 0x${m.cmd.toString(16)} (${m.status})`;
          else if (m.cmd === ExtCmd.TEXT) void this.send(encode.extPrefs());
        }
        break;
      case Tag.ERROR:
        if ("cmd" in m) {
          if (m.cmd >= ExtCmd.HELLO && m.cmd <= 0x2f) {
            this.ext = 0; // stock firmware: no extensions
            break;
          }
          if (m.cmd === Cmd.PROFILE_READ && this.download) {
            const d = this.download;
            this.download = null;
            d.fail(new DeviceError("THE KNOB COULDN'T READ THAT PROFILE"));
            return;
          }
          this.error = `device refused command 0x${m.cmd.toString(16)} (${m.code})`;
        }
        break;
      default:
        return;
    }
    this.changed();
  }

  private finishDownload() {
    const d = this.download!;
    this.download = null;
    if (crc32(d.buf!) !== d.crc) d.fail(new DeviceError("PROFILE CORRUPTED ON THE WAY (CRC)"));
    else d.done(new TextDecoder().decode(d.buf!));
  }

  // SYS arrives twice a second: HISTORY_SECONDS worth of samples.
  private push(key: keyof History, v: number) {
    const a = this.history[key];
    a.push(v);
    if (a.length > HISTORY_SECONDS * 2) a.shift();
  }

  private onProfile(p: Profile) {
    const old = this.profiles[p.index];
    const same = !!old && old.id === p.id;
    // Keep the old icon on screen until the new one is in (no flicker on a reload).
    this.profiles[p.index] = { ...p, icon: same ? old.icon : null };
    const fresh = same && (old.icon !== null) === p.hasIcon && this.staleIcons !== null && !this.staleIcons.has(p.index);
    if (p.hasIcon && !fresh) {
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
    if (this.profiles[index]) this.profiles[index].icon = rgb565ToImage(buf, 48);
    this.iconBuf.delete(index);
    this.nextProfile(index);
    this.changed();
  }

  private nextProfile(index: number) {
    if (this.hello && index + 1 < this.hello.profileCount) this.send(encode.profile(index + 1));
  }
}

export { Cmd };
