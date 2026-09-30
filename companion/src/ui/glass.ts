// The device, mirrored: the round 240x240 glass at 1x (2 screen pixels each on Retina), drawn like the firmware draws it --
// the 60-LED ring with the knob's spot, the detents, the knob's job and feel, and F1-F4.

import type { Device } from "../device";
import { HidType } from "../proto";
import { C, waveY } from "./kit";

const S = 240, CX = 120, CY = 120;
const RING = 60;
const MODE_NAMES: Record<number, string> = { 0: "KEYBOARD", 1: "MOUSE", 2: "MIDI", 3: "APP" };
const FEEL_NAMES = ["SAW", "SINE", "VISCOSE"];
const MENU_KEYS = ["SEL", "", "BACK", "MENU"];

export function glass(device: Device) {
  const canvas = document.createElement("canvas");
  canvas.width = S;
  canvas.height = S;
  canvas.style.width = `${S}px`;
  canvas.style.height = `${S}px`;
  const ctx = canvas.getContext("2d")!;
  ctx.imageSmoothingEnabled = false;

  const iconCanvas = document.createElement("canvas");
  iconCanvas.width = iconCanvas.height = 48;

  const text = (s: string, x: number, y: number, color: string, size = 8, align: CanvasTextAlign = "center") => {
    ctx.font = `${size}px Silkscreen`;
    ctx.textAlign = align;
    ctx.textBaseline = "top";
    ctx.fillStyle = color;
    ctx.fillText(s, Math.round(x), Math.round(y));
  };

  function draw() {
    ctx.clearRect(0, 0, S, S);
    // the glass
    ctx.fillStyle = C.black;
    ctx.beginPath();
    ctx.arc(CX, CY, 120, 0, Math.PI * 2);
    ctx.fill();
    // its edge, one pixel of dark, so the round screen reads on the black window
    ctx.fillStyle = C.dark;
    for (let i = 0; i < 720; i++) {
      const a = (i / 720) * Math.PI * 2;
      ctx.fillRect(Math.round(CX - 0.5 + 119 * Math.cos(a)), Math.round(CY - 0.5 + 119 * Math.sin(a)), 1, 1);
    }

    const st = device.state, set = device.settings;
    if (device.status !== "connected" || !st || !set) {
      text(device.status === "unsupported" ? "NO USB ACCESS" : "LOOKING FOR QUADRA", CX, 112, C.grey);
      ring(null);
      return;
    }

    ring(st.angle);
    ticks(set.detents, st.angle);

    // What the knob does: the app (icon + name) or the mode, and the feel.
    const profile = device.profiles[set.profile];
    if (set.hidType === HidType.APP && profile) {
      if (profile.icon) {
        iconCanvas.getContext("2d")!.putImageData(profile.icon, 0, 0);
        ctx.drawImage(iconCanvas, CX - 24, 56);
      }
      text(profile.name, CX, 110, C.white, 16);
    } else {
      text(MODE_NAMES[set.hidType] ?? "?", CX, 78, C.white, 16);
    }
    feel(set.feel, 132);

    // Status line: menu / idle on the device.
    if (st.menuScreen !== 0) text("MENU OPEN", CX, 36, C.amber);
    else if (st.screensaver) text("IDLE", CX, 36, C.grey);

    keys(st.buttons, set.hidType === HidType.APP && profile ? profile.legend : MENU_KEYS);
  }

  // 60 dots; the knob's spot lit amber with a short fade either side.
  function ring(angle: number | null) {
    const pos = angle === null ? -1 : ((angle / (Math.PI * 2)) * RING) % RING;
    for (let i = 0; i < RING; i++) {
      const a = (i / RING) * Math.PI * 2 - Math.PI / 2;
      const x = Math.round(CX + 110 * Math.cos(a)) - 1, y = Math.round(CY + 110 * Math.sin(a)) - 1;
      let d = pos < 0 ? 99 : Math.abs(i - (pos < 0 ? pos + RING : pos));
      d = Math.min(d, RING - d);
      ctx.fillStyle = d < 0.6 ? C.amber : d < 1.6 ? "#8a6d2a" : C.dark;
      ctx.fillRect(x, y, 3, 3);
    }
  }

  // Detent marks inside the ring; the one the knob sits in is white.
  function ticks(n: number, angle: number) {
    const pos = (((angle / (Math.PI * 2)) * n) % n + n) % n;
    const cur = Math.round(pos) % n;
    for (let i = 0; i < n; i++) {
      const a = (i / n) * Math.PI * 2 - Math.PI / 2;
      ctx.fillStyle = i === cur ? C.white : C.grey;
      for (let r = 98; r <= 102; r++) ctx.fillRect(Math.round(CX + r * Math.cos(a)), Math.round(CY + r * Math.sin(a)), 1, 1);
    }
  }

  function feel(type: number, y: number) {
    const name = FEEL_NAMES[type] ?? "?";
    ctx.font = "8px Silkscreen";
    const tw = ctx.measureText(name).width;
    const w = 28, x0 = Math.round(CX - (w + 8 + tw) / 2);
    ctx.fillStyle = C.white;
    let prev = -1;
    for (let x = 0; x < w; x++) {
      const yy = Math.round(y + 4 - waveY(type, x / w) * 4);
      const top = prev < 0 ? yy : Math.min(prev, yy), bot = prev < 0 ? yy : Math.max(prev, yy);
      ctx.fillRect(x0 + x, top, 1, bot - top + 1);
      prev = yy;
    }
    text(name, x0 + w + 8, y + 1, C.grey, 8, "left");
  }

  // Keycaps on the arc, like the Main Screen's legend.
  function keys(held: number, legend: string[]) {
    const xs = [60, 100, 140, 180], ys = [150, 158, 158, 150];
    for (let i = 0; i < 4; i++) {
      const down = (held >> i) & 1;
      const x = xs[i] - 13, y = ys[i];
      const disabled = !legend[i] || legend[i] === "-";
      ctx.fillStyle = down ? C.amber : disabled ? "#262626" : "#8a8a8a";
      ctx.fillRect(x, y + (down ? 2 : 3), 26, 20);
      ctx.fillStyle = down ? C.amber : disabled ? "#4a4a4a" : C.face;
      ctx.fillRect(x, y + (down ? 2 : 0), 26, 18);
      text(`F${i + 1}`, xs[i], y + (down ? 7 : 5), down || !disabled ? C.black : C.grey);
      text(disabled ? "--" : legend[i], xs[i], y + 27, disabled ? C.grey : C.white);
    }
  }

  document.fonts.load("8px Silkscreen").then(draw);
  device.subscribe(draw);
  return canvas;
}
