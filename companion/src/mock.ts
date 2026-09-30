// A pretend knob for working on the UI without hardware: open the page with ?demo. Answers
// the protocol like host_link.c does and streams a slowly turning knob.

import { Cmd, ICON_BYTES, REPORT_SIZE, Set, Tag } from "./proto";
import type { Transport } from "./transport";

const PROFILES = [
  { id: "figma", name: "FIGMA", legend: ["TOOL", "ZOOM", "UNDO", "MENU"], color: [0xa2, 0x59, 0xff] },
  { id: "plasticity", name: "PLASTICITY", legend: ["ORBIT", "PAN", "UNDO", "MENU"], color: [0xff, 0x8a, 0x3d] },
  { id: "onshape", name: "ONSHAPE", legend: ["ORBIT", "PAN", "UNDO", "MENU"], color: [0x2f, 0x9b, 0xff] },
];

export class MockTransport implements Transport {
  readonly kind = "tauri";
  readonly needsGesture = false;
  onReport: (r: Uint8Array) => void = () => {};
  onClosed: () => void = () => {};

  private live = { detents: 12, kp: 6, kd: 0.01, feel: 0, amp: 100, pitch: 1, sound: 0, hidType: 3, midi: 1, profile: 0, boot: 0, rotation: 0, host: 0 };
  private saved = { ...this.live };
  private timer = 0;
  private t0 = performance.now();
  private seq = 0;
  private tick = 0;

  async tryConnect() {
    return true;
  }
  async close() {
    window.clearInterval(this.timer);
  }

  async send(r: Uint8Array) {
    const out = new Uint8Array(REPORT_SIZE);
    const v = new DataView(out.buffer);
    const inV = new DataView(r.buffer, r.byteOffset);
    const reply = (): void => void setTimeout(() => this.onReport(out), 5);
    switch (r[0]) {
      case Cmd.HELLO: {
        out[0] = Tag.HELLO;
        out[1] = 1;
        out[2] = PROFILES.length;
        out.set(new TextEncoder().encode("DEMO 7142FDA"), 4);
        out.set(new TextEncoder().encode("SEP 30 2026"), 36);
        return reply();
      }
      case Cmd.SET: {
        const keys = ["detents", "kp", "kd", "feel", "amp", "pitch", "sound", "hidType", "midi", "profile", "boot", "rotation", "host"] as const;
        const k = keys[r[1]];
        const f = r[1] === Set.KP || r[1] === Set.KD || r[1] === Set.PITCH;
        if (k) (this.live as any)[k] = f ? inV.getFloat32(4, true) : inV.getInt32(4, true);
        this.settings(out);
        return reply();
      }
      case Cmd.SAVE:
        this.saved = { ...this.live };
        this.settings(out);
        return reply();
      case Cmd.REVERT:
        this.live = { ...this.saved };
        this.settings(out);
        return reply();
      case Cmd.GET_SETTINGS:
        this.settings(out);
        return reply();
      case Cmd.STREAM:
        window.clearInterval(this.timer);
        if (r[1] > 0) this.timer = window.setInterval(() => this.stream(), 1000 / r[1]);
        return;
      case Cmd.PROFILE: {
        const p = PROFILES[r[1]];
        out[0] = Tag.PROFILE;
        out[1] = r[1];
        out[2] = PROFILES.length;
        out[3] = 1;
        out.set(new TextEncoder().encode(p.id), 4);
        out.set(new TextEncoder().encode(p.name), 16);
        p.legend.forEach((l, i) => out.set(new TextEncoder().encode(l), 32 + i * 8));
        return reply();
      }
      case Cmd.PROFILE_ICON: {
        const idx = r[1], off = inV.getUint16(2, true);
        const icon = this.icon(idx);
        const len = Math.min(56, ICON_BYTES - off);
        out[0] = Tag.PROFILE_ICON;
        out[1] = idx;
        v.setUint16(2, off, true);
        out[4] = len;
        out.set(icon.subarray(off, off + len), 8);
        return reply();
      }
    }
  }

  private settings(out: Uint8Array) {
    const v = new DataView(out.buffer);
    const l = this.live, s = this.saved;
    const keys = ["detents", "kp", "kd", "feel", "amp", "pitch", "sound", "hidType", "midi", "profile", "boot", "rotation", "host"] as const;
    let dirty = 0;
    keys.forEach((k, i) => {
      if (Math.abs((l as any)[k] - (s as any)[k]) > 1e-6) dirty |= 1 << i;
    });
    out[0] = Tag.SETTINGS;
    v.setUint16(1, dirty, true);
    v.setInt32(4, l.detents, true);
    v.setFloat32(8, l.kp, true);
    v.setFloat32(12, l.kd, true);
    out[16] = l.feel;
    out[17] = l.amp;
    v.setFloat32(18, l.pitch, true);
    out[22] = l.sound;
    out[23] = l.hidType;
    out[24] = l.midi;
    out[25] = l.profile;
    out[26] = l.boot;
    out[27] = l.rotation;
    out[28] = l.host;
  }

  // A pixel badge in the profile's colour: rounded square + white initial bar.
  private icon(i: number): Uint8Array {
    const [r, g, b] = PROFILES[i].color;
    const px = new Uint8Array(ICON_BYTES);
    const c565 = (r: number, g: number, b: number) => ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    for (let y = 0; y < 48; y++)
      for (let x = 0; x < 48; x++) {
        const inside = x >= 6 && x < 42 && y >= 6 && y < 42 && !((x < 9 || x >= 39) && (y < 9 || y >= 39));
        const mark = x >= 18 && x < 30 && y >= 16 && y < 32 && !(x >= 21 && x < 27 && y >= 19 && y < 29);
        const c = inside ? (mark ? c565(255, 255, 255) : c565(r, g, b)) : 0;
        px[(y * 48 + x) * 2] = c >> 8;
        px[(y * 48 + x) * 2 + 1] = c & 0xff;
      }
    return px;
  }

  private stream() {
    const t = (performance.now() - this.t0) / 1000;
    const angle = Math.sin(t * 0.6) * 2.4 + t * 0.3;
    const st = new Uint8Array(REPORT_SIZE);
    const v = new DataView(st.buffer);
    st[0] = Tag.STATE;
    v.setUint16(1, this.seq++, true);
    v.setInt32(4, Math.round(angle * 1e4), true);
    v.setInt32(8, Math.round((angle / (2 * Math.PI)) * this.live.detents), true);
    st[12] = Math.floor(t) % 7 === 3 ? 1 : 0;
    v.setInt32(16, Math.floor(t * 3), true);
    this.onReport(st);

    if (++this.tick % 15 !== 0) return; // SYS twice a second at 30 Hz
    const a = new Uint8Array(REPORT_SIZE);
    const av = new DataView(a.buffer);
    const motor = 40 + Math.round(30 * Math.abs(Math.sin(t)));
    a[0] = Tag.SYS_A;
    av.setUint16(4, motor, true);
    av.setUint16(6, 71, true);
    av.setUint16(8, 150, true);
    av.setUint16(10, motor + 221, true);
    av.setUint16(12, 512, true);
    a[14] = 1;
    av.setFloat32(16, 37.2 + Math.sin(t / 9), true);
    av.setFloat32(20, 38.4, true);
    av.setUint16(24, 180, true);
    av.setUint16(26, 350, true);
    av.setFloat32(28, 0.1, true);
    a[32] = 5;
    av.setUint16(34, 3000, true);
    av.setUint16(36, 5000, true);
    this.onReport(a);

    const b = new Uint8Array(REPORT_SIZE);
    const bv = new DataView(b.buffer);
    b[0] = Tag.SYS_B;
    b[4] = 58;
    b[5] = 9 + Math.round(3 * Math.random());
    b[6] = 61;
    b[7] = 14;
    bv.setFloat32(8, 10, true);
    bv.setFloat32(12, 32, true);
    bv.setFloat32(16, 533, true);
    bv.setFloat32(20, 438, true);
    bv.setUint32(24, 0, true);
    bv.setFloat32(28, 0, true);
    bv.setUint32(32, 186 * 1024, true);
    bv.setUint32(36, 151 * 1024, true);
    bv.setUint32(48, Math.floor(t) + 3700, true);
    this.onReport(b);
  }
}
