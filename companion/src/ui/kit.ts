// Small DOM + pixel-drawing kit shared by the views.

export const C = {
  black: "#000000",
  white: "#ffffff",
  grey: "#6e6e6e",
  dark: "#3a3a3a",
  amber: "#ffc94d",
  face: "#e4e4e4",
};

type Attrs = Record<string, string | number | boolean | ((e: any) => void)>;

export function el<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  attrs: Attrs = {},
  ...children: (Node | string | null | undefined)[]
): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (typeof v === "function") e.addEventListener(k.replace(/^on/, ""), v as EventListener);
    else if (k === "class") e.className = String(v);
    else if (v === true) e.setAttribute(k, "");
    else if (v !== false) e.setAttribute(k, String(v));
  }
  for (const c of children) if (c != null) e.append(c);
  return e;
}

export function section(title: string, note?: Node | string): { root: HTMLElement; body: HTMLElement } {
  const body = el("div");
  const root = el("div", { class: "section" }, el("div", { class: "section-head" }, title, note ? el("span", { class: "note" }, note) : null), body);
  return { root, body };
}

// --- stepped slider: a row of blocks, like the device's bars ---

export interface Slider {
  root: HTMLElement;
  update(value: number, dirty: boolean): void;
  limits(min: number, max: number): void; // the range changed (another haptic profile or feel)
  mute(on: boolean): void; // not usable right now: greyed, shows "--"
}

export interface SliderOpts {
  label: string;
  caption: string;
  min: number;
  max: number;
  step: number;
  format: (v: number) => string;
  onInput: (v: number) => void; // while dragging (throttled by the caller)
}

const BLOCKS = 24;

export function slider(opts: SliderOpts): Slider {
  const o = { ...opts };
  let muted = false;
  const blocks = el("div", { class: "blocks", tabindex: 0 });
  const cells: HTMLElement[] = [];
  for (let i = 0; i < BLOCKS; i++) {
    const c = el("i");
    cells.push(c);
    blocks.append(c);
  }
  const value = el("div", { class: "value" });
  const root = el("div", { class: "row" }, el("div", { class: "name" }, el("span", {}, o.label), el("span", { class: "cap" }, o.caption)), blocks, value);

  let current = o.min;
  let dragging = false;
  const snap = (v: number) => {
    const s = Math.round((v - o.min) / o.step) * o.step + o.min;
    return Math.min(o.max, Math.max(o.min, +s.toFixed(4)));
  };
  const draw = (v: number) => {
    const k = (v - o.min) / (o.max - o.min);
    const lit = v <= o.min ? 0 : Math.max(1, Math.round(k * BLOCKS));
    cells.forEach((c, i) => c.classList.toggle("on", !muted && i < lit));
    value.textContent = muted ? "--" : o.format(v);
  };
  const setLocal = (v: number) => {
    if (muted) return;
    v = snap(v);
    if (v === current) return;
    current = v;
    draw(v);
    o.onInput(v);
  };
  const fromX = (x: number) => {
    const r = blocks.getBoundingClientRect();
    return o.min + Math.min(1, Math.max(0, (x - r.left) / r.width)) * (o.max - o.min);
  };

  blocks.addEventListener("pointerdown", (e) => {
    dragging = true;
    blocks.classList.add("drag");
    blocks.setPointerCapture(e.pointerId);
    setLocal(fromX(e.clientX));
  });
  blocks.addEventListener("pointermove", (e) => dragging && setLocal(fromX(e.clientX)));
  const end = () => {
    dragging = false;
    blocks.classList.remove("drag");
  };
  blocks.addEventListener("pointerup", end);
  blocks.addEventListener("pointercancel", end);
  // Scrolling steps it, like turning the knob.
  blocks.addEventListener(
    "wheel",
    (e) => {
      e.preventDefault();
      setLocal(current + (e.deltaY < 0 || e.deltaX > 0 ? o.step : -o.step));
    },
    { passive: false },
  );
  blocks.addEventListener("keydown", (e) => {
    if (e.key === "ArrowRight" || e.key === "ArrowUp") setLocal(current + o.step);
    else if (e.key === "ArrowLeft" || e.key === "ArrowDown") setLocal(current - o.step);
    else return;
    e.preventDefault();
  });

  return {
    root,
    update(v, dirty) {
      root.classList.toggle("dirty", dirty);
      if (dragging) return; // the device echoes what we sent; don't fight the pointer
      current = v;
      draw(v);
    },
    limits(min, max) {
      if (min === o.min && max === o.max) return;
      o.min = min;
      o.max = max;
      draw(current);
    },
    mute(on) {
      if (on === muted) return;
      muted = on;
      root.classList.toggle("muted", on);
      draw(current);
    },
  };
}

// --- choice cards ---

export interface Choice<T> {
  value: T;
  body: () => Node[];
}

export function cards<T>(choices: Choice<T>[], onPick: (v: T) => void, minWidth = 64) {
  const root = el("div", { class: "cards" });
  root.style.gridTemplateColumns = `repeat(auto-fill, minmax(${minWidth}px, 1fr))`;
  const items = choices.map((c) => {
    const b = el("button", { class: "card", onclick: () => onPick(c.value) }, ...c.body());
    root.append(b);
    return { c, b };
  });
  return {
    root,
    update(selected: T, dirty: boolean, allowed?: (v: T) => boolean) {
      for (const { c, b } of items) {
        const on = c.value === selected;
        b.classList.toggle("on", on);
        b.classList.toggle("dirty", on && dirty);
        b.disabled = allowed ? !allowed(c.value) : false;
      }
    },
  };
}

// --- pixel drawing ---

// The device's wave shapes, exactly (ui_gfx.cpp wave_y): one period in u, output -1..1.
let sawShape = 0;
// The SHAPE setting (0..0.9): every SAW curve drawn after this bends with it.
export function setSawShape(shape: number) {
  sawShape = shape;
}

export function waveY(feel: number, u: number): number {
  u -= Math.floor(u);
  if (feel === 1) return -Math.sin(u * 2 * Math.PI); // SINE
  if (feel === 0) {
    const e = 1 - 2 * u; // SAW: the straight line, bent by SHAPE like the control loop does
    return e * (1 - sawShape + sawShape * e * e);
  }
  return 0.2 * Math.sin(u * 4 * Math.PI); // VISCOSE: drag, barely any shape
}

// A crisp 2px curve on a small canvas (drawn at 1x, scaled by CSS with pixelated rendering).
export function curve(canvas: HTMLCanvasElement, feel: number, color: string, periods = 2) {
  const ctx = canvas.getContext("2d")!;
  const w = canvas.width, h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = color;
  const amp = h / 2 - 2;
  let prev = -1;
  for (let x = 0; x < w; x++) {
    const y = Math.round(h / 2 - waveY(feel, (x / w) * periods) * amp);
    const y0 = prev < 0 ? y : prev;
    const top = Math.min(y, y0), bot = Math.max(y, y0);
    ctx.fillRect(x, top - 1, 1, bot - top + 2);
    prev = y;
  }
}

// History as blocky columns, newest on the right.
export function spark(canvas: HTMLCanvasElement, values: number[], max: number, color = C.white) {
  const dpr = 1;
  const w = canvas.clientWidth * dpr, h = canvas.clientHeight * dpr;
  if (w === 0) return;
  if (canvas.width !== w) canvas.width = w;
  if (canvas.height !== h) canvas.height = h;
  const ctx = canvas.getContext("2d")!;
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = C.dark;
  ctx.fillRect(0, h - 2, w, 2);
  const n = 120, col = 2, gap = 1;
  const cols = Math.min(n, Math.floor(w / (col + gap)));
  const start = Math.max(0, values.length - cols);
  ctx.fillStyle = color;
  for (let i = start; i < values.length; i++) {
    const x = w - (values.length - i) * (col + gap);
    const bh = Math.max(2, Math.round((Math.min(values[i], max) / max) * (h - 4)));
    ctx.fillRect(x, h - bh, col, bh);
  }
}
