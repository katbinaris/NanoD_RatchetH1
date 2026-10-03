// Pixel drawing that mirrors the knob (PIXEL_ART.md: whole pixels, the device palette): its
// wave shapes, and its main screen drawn from data -- the app's icon and name, the feel, F1-F4.
// Used where the app shows the knob itself: the sidebar's picture, a profile's preview.

export const C = { black: "#000000", white: "#ffffff", grey: "#6e6e6e", dark: "#3a3a3a", amber: "#ffc94d", face: "#e4e4e4" };

// The device's wave shapes, exactly (ui_gfx.cpp wave_y): one period in u, output -1..1. SAW
// bends with SHAPE (0..0.9) like the control loop does.
export function waveY(feel: number, u: number, shape = 0): number {
  u -= Math.floor(u);
  if (feel === 1) return -Math.sin(u * 2 * Math.PI); // SINE
  if (feel === 0) {
    const e = 1 - 2 * u; // SAW
    return e * (1 - shape + shape * e * e);
  }
  return 0.2 * Math.sin(u * 4 * Math.PI); // VISCOSE: drag, barely any shape
}

export interface ScreenInfo {
  icon: ImageData | null; // 48x48
  name: string;
  feel: number; // Feel
  legend: string[]; // F1-F4; "" or "-" = not used
}

const S = 240, CX = 120;
const FEEL_NAMES = ["SAW", "SINE", "VISCOSE"];

// The knob's main screen on a 240x240 canvas (ui_screens.cpp's layout, without the live parts).
export function drawMainScreen(canvas: HTMLCanvasElement, info: ScreenInfo) {
  if (canvas.width !== S) canvas.width = canvas.height = S;
  const ctx = canvas.getContext("2d")!;
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = C.black;
  ctx.fillRect(0, 0, S, S);
  const text = (s: string, x: number, y: number, color: string, size = 8, align: CanvasTextAlign = "center") => {
    ctx.font = `${size}px Silkscreen`;
    ctx.textAlign = align;
    ctx.textBaseline = "top";
    ctx.fillStyle = color;
    ctx.fillText(s, x, y);
  };
  if (info.icon) {
    const ic = document.createElement("canvas");
    ic.width = ic.height = 48;
    ic.getContext("2d")!.putImageData(info.icon, 0, 0);
    ctx.drawImage(ic, CX - 24, 56);
  }
  text(info.name, CX, 110, C.white, 16);

  // The feel: its wave and name.
  const name = FEEL_NAMES[info.feel] ?? "";
  ctx.font = "8px Silkscreen";
  const tw = ctx.measureText(name).width, w = 28, x0 = Math.round(CX - (w + 8 + tw) / 2), y = 132;
  ctx.fillStyle = C.white;
  let prev = -1;
  for (let x = 0; x < w; x++) {
    const yy = Math.round(y + 4 - waveY(info.feel, x / w) * 4);
    const top = prev < 0 ? yy : Math.min(prev, yy), bot = prev < 0 ? yy : Math.max(prev, yy);
    ctx.fillRect(x0 + x, top, 1, bot - top + 1);
    prev = yy;
  }
  text(name, x0 + w + 8, y + 1, C.grey, 8, "left");

  // Keycaps on the arc.
  const xs = [60, 100, 140, 180], ys = [150, 158, 158, 150];
  for (let i = 0; i < 4; i++) {
    const off = !info.legend[i] || info.legend[i] === "-";
    const x = xs[i] - 13, yk = ys[i];
    ctx.fillStyle = off ? "#262626" : "#8a8a8a";
    ctx.fillRect(x, yk + 3, 26, 20);
    ctx.fillStyle = off ? "#4a4a4a" : C.face;
    ctx.fillRect(x, yk, 26, 18);
    text(`F${i + 1}`, xs[i], yk + 5, off ? C.grey : C.black);
    text(off ? "--" : info.legend[i], xs[i], yk + 27, off ? C.grey : C.white);
  }
}
