// The device, drawn as it is: the top-down render (assets/device.png) with the live screen
// inside the knob, the LED ring glowing in the groove round it and the keys lighting up --
// from what the knob streams (HOST_TAG_LEDS), or made up from the knob's angle and the keys
// on firmware that doesn't stream its LEDs.
//
// Geometry is in the render's own pixels (800x1100), measured off the image; the view scales
// it to fit its column.
//
// With the fork's extensions (v6) the glass shows the knob's own screen, streamed (the app's
// re-drawing stays as the fallback), and the picture is a remote: press a key (held while the
// button is down), drag round the knob or scroll over it to turn it -- the knob reacts as if
// touched.

import deviceUrl from "../assets/device.png";
import type { Device } from "../device";
import { EXT_SCREEN_VERSION, HidType, LED_COUNT, SCREEN_SIZE } from "../proto";
import { el } from "./kit";
import { glass } from "./glass";

const IMG = { w: 800, h: 1100 };
const CROP = { x: 55, y: 120, w: 690, h: 925 }; // the body, with the cable stubs
const GLASS = { x: 398, y: 498, r: 170 }; // the screen, inside the knob
const RING_R = 242; // the groove the LED ring lights
const KEYS = [176, 324, 475, 623].map((x) => ({ x, y: 919, w: 118, h: 132 }));
const LED_MAX = 51; // the firmware caps the LEDs at 20% (led_task.c LED_MAX)
const LEDS_STALE_MS = 2000;
const MENU_KEYS = ["SEL", "", "BACK", "MENU"];
const TURN_STEP = (2 * Math.PI) / 24; // a drag this far round the knob is one detent
const WHEEL_STEP = 40; // and this much scrolling
const KEY_REPEAT_MS = 250; // the knob lets a key go 600ms after the last word on it

export function deviceView(device: Device) {
  const root = el("div", { class: "device" });
  const img = el("img", { src: deviceUrl, alt: "", draggable: "false", class: "device-body" });
  const leds = el("canvas", { class: "device-leds" });
  const screen = glass(device, { embedded: true });
  screen.classList.add("device-glass");
  const mirror = el("canvas", { class: "device-glass device-mirror", width: SCREEN_SIZE, height: SCREEN_SIZE });
  mirror.style.display = "none";
  const pad = el("div", { class: "device-turn" }); // over the knob: drag round it, or scroll
  const keys = KEYS.map((_, i) => el("div", { class: "device-key", "data-key": i }));
  root.append(img, leds, screen, mirror, pad, ...keys);
  const remote = () => device.status === "connected" && (device.ext ?? 0) >= EXT_SCREEN_VERSION;

  // Keys: held while the pointer is down, said again every KEY_REPEAT_MS so the knob keeps them.
  let held = 0, repeat = 0;
  const sendKeys = () => void device.pressKeys(held);
  keys.forEach((k, i) => {
    k.addEventListener("pointerdown", (e) => {
      if (!remote()) return;
      e.preventDefault();
      k.setPointerCapture(e.pointerId);
      held |= 1 << i;
      sendKeys();
      window.clearInterval(repeat);
      repeat = window.setInterval(sendKeys, KEY_REPEAT_MS);
    });
    const up = () => {
      if (!(held & (1 << i))) return;
      held &= ~(1 << i);
      sendKeys();
      if (!held) window.clearInterval(repeat);
    };
    k.addEventListener("pointerup", up);
    k.addEventListener("pointercancel", up);
  });

  // The knob: round it by dragging (clockwise = the knob's clockwise), or by scrolling.
  let lastAngle: number | null = null, swept = 0, scrolled = 0;
  const angleAt = (e: PointerEvent) => {
    const r = root.getBoundingClientRect();
    return Math.atan2(e.clientY - r.top - (GLASS.y - CROP.y) * s, e.clientX - r.left - (GLASS.x - CROP.x) * s);
  };
  pad.addEventListener("pointerdown", (e) => {
    if (!remote()) return;
    pad.setPointerCapture(e.pointerId);
    lastAngle = angleAt(e);
    swept = 0;
  });
  pad.addEventListener("pointermove", (e) => {
    if (lastAngle === null) return;
    const a = angleAt(e);
    let d = a - lastAngle;
    if (d > Math.PI) d -= 2 * Math.PI;
    if (d < -Math.PI) d += 2 * Math.PI;
    lastAngle = a;
    swept += d;
    const n = Math.trunc(swept / TURN_STEP);
    if (n) {
      swept -= n * TURN_STEP;
      void device.turn(n);
    }
  });
  const release = () => (lastAngle = null);
  pad.addEventListener("pointerup", release);
  pad.addEventListener("pointercancel", release);
  pad.addEventListener(
    "wheel",
    (e) => {
      if (!remote()) return;
      e.preventDefault();
      scrolled += e.deltaY;
      const n = Math.trunc(scrolled / WHEEL_STEP);
      if (n) {
        scrolled -= n * WHEEL_STEP;
        void device.turn(-n); // scrolling up turns it clockwise
      }
    },
    { passive: false },
  );

  let s = 0.5;
  let fitW = 0, fitH = 0;

  // Fit the body into w x h.
  function fit(w: number, h: number) {
    if (w <= 0 || h <= 0 || (w === fitW && h === fitH)) return;
    fitW = w;
    fitH = h;
    s = Math.min(w / CROP.w, h / CROP.h);
    root.style.width = `${CROP.w * s}px`;
    root.style.height = `${CROP.h * s}px`;
    Object.assign(img.style, { left: `${-CROP.x * s}px`, top: `${-CROP.y * s}px`, width: `${IMG.w * s}px`, height: `${IMG.h * s}px` });
    const d = GLASS.r * 2 * s;
    Object.assign(screen.style, { left: `${(GLASS.x - GLASS.r - CROP.x) * s}px`, top: `${(GLASS.y - GLASS.r - CROP.y) * s}px`, width: `${d}px`, height: `${d}px` });
    Object.assign(mirror.style, { left: screen.style.left, top: screen.style.top, width: `${d}px`, height: `${d}px` });
    const pr = (RING_R + 36) * s;
    Object.assign(pad.style, { left: `${(GLASS.x - CROP.x) * s - pr}px`, top: `${(GLASS.y - CROP.y) * s - pr}px`, width: `${pr * 2}px`, height: `${pr * 2}px` });
    keys.forEach((k, i) => {
      const g = KEYS[i];
      Object.assign(k.style, { left: `${(g.x - g.w / 2 - CROP.x) * s}px`, top: `${(g.y - g.h / 2 - CROP.y) * s}px`, width: `${g.w * s}px`, height: `${g.h * s}px` });
    });
    const dpr = window.devicePixelRatio || 1;
    leds.width = Math.round(CROP.w * s * dpr);
    leds.height = Math.round(CROP.h * s * dpr);
    leds.style.width = `${CROP.w * s}px`;
    leds.style.height = `${CROP.h * s}px`;
    draw();
  }

  // The LED colours to show: streamed, or made up like led_task.c does (a dim amber ring, a
  // bright spot at the knob, a held key bright).
  const guess = new Uint8Array(LED_COUNT * 3);
  function ledColours(): Uint8Array | null {
    if (device.status !== "connected") return null;
    if (performance.now() - device.ledsAt < LEDS_STALE_MS) return device.leds;
    const st = device.state;
    if (!st) return null;
    const pos = (((st.angle / (2 * Math.PI)) * 60) % 60 + 60) % 60;
    const amber = [255, 201, 77];
    for (let i = 0; i < 60; i++) {
      let d = Math.abs(i - pos);
      d = Math.min(d, 60 - d);
      const k = Math.max(0.2, 1 - d / 2.5);
      for (let c = 0; c < 3; c++) guess[i * 3 + c] = Math.round(LED_MAX * k * (amber[c] / 255));
    }
    for (let key = 0; key < 4; key++) {
      const k = (st.buttons >> key) & 1 ? 1 : 0.2;
      for (let j = 0; j < 2; j++) for (let c = 0; c < 3; c++) guess[(60 + key * 2 + j) * 3 + c] = Math.round(LED_MAX * k * (amber[c] / 255));
    }
    return guess;
  }

  // One LED as light: its hue at full saturation, its level (0..1 of what the firmware allows)
  // lifted so a dim LED still reads on screen -- the strips run at 20%, the eye sees them bright.
  function glow(ctx: CanvasRenderingContext2D, rgb: Uint8Array, i: number, x: number, y: number, radius: number, gain: number) {
    const r = rgb[i * 3], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
    const peak = Math.max(r, g, b);
    if (peak === 0) return;
    const level = Math.min(1, Math.pow(peak / LED_MAX, 0.6)) * gain;
    const k = 255 / peak;
    const col = `${Math.round(r * k)},${Math.round(g * k)},${Math.round(b * k)}`;
    const grad = ctx.createRadialGradient(x, y, 0, x, y, radius);
    grad.addColorStop(0, `rgba(${col},${level})`);
    grad.addColorStop(0.45, `rgba(${col},${level * 0.45})`);
    grad.addColorStop(1, `rgba(${col},0)`);
    ctx.fillStyle = grad;
    ctx.fillRect(x - radius, y - radius, radius * 2, radius * 2);
  }

  // The knob's own screen, when it streams one.
  let shown = -1;
  function drawScreen() {
    const live = device.status === "connected" && device.screenLive;
    mirror.style.display = live ? "" : "none";
    screen.style.visibility = live ? "hidden" : "";
    root.classList.toggle("remote", remote());
    if (live && device.screenVersion !== shown) {
      shown = device.screenVersion;
      mirror.getContext("2d")!.putImageData(device.screen, 0, 0);
    }
  }

  function draw() {
    drawScreen();
    const ctx = leds.getContext("2d")!;
    const dpr = leds.width / (CROP.w * s || 1);
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, leds.width, leds.height);
    const rgb = ledColours();
    root.classList.toggle("off", device.status !== "connected");
    const held = device.state?.buttons ?? 0;
    keys.forEach((k, i) => k.classList.toggle("down", ((held >> i) & 1) === 1));
    if (!rgb) return;
    ctx.setTransform(dpr * s, 0, 0, dpr * s, -CROP.x * dpr * s, -CROP.y * dpr * s); // draw in image pixels
    ctx.globalCompositeOperation = "lighter";
    for (let i = 0; i < 60; i++) {
      const a = (i / 60) * Math.PI * 2 - Math.PI / 2;
      glow(ctx, rgb, i, GLASS.x + RING_R * Math.cos(a), GLASS.y + RING_R * Math.sin(a), 20, 0.9);
    }
    // Keys: two LEDs under each cap, shining up round it.
    for (let key = 0; key < 4; key++) {
      const g = KEYS[key];
      glow(ctx, rgb, 60 + key * 2, g.x - 18, g.y, 70, 0.45);
      glow(ctx, rgb, 60 + key * 2 + 1, g.x + 18, g.y, 70, 0.45);
    }
    ctx.globalCompositeOperation = "source-over";
  }

  // What each key does now, on hover.
  function titles() {
    const set = device.settings;
    const profile = set ? device.profiles[set.profile] : undefined;
    const legend = set?.hidType === HidType.APP && profile ? profile.legend : MENU_KEYS;
    keys.forEach((k, i) => (k.title = `F${i + 1}${legend[i] && legend[i] !== "-" ? `: ${legend[i]}` : ""}`));
  }

  // LED reports come ~60 times a second: one redraw per frame at most.
  let queued = false;
  device.subscribe(() => {
    if (queued) return;
    queued = true;
    requestAnimationFrame(() => {
      queued = false;
      draw();
      titles();
    });
  });
  return { root, fit };
}
