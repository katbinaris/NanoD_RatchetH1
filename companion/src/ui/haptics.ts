// HAPTICS: the Orbit's settings as sliders + the three feels as cards. Live on the knob while
// you drag; SAVE (or F2 on the device) keeps them.

import type { Device } from "../device";
import { Limits, Set, type SetId } from "../proto";
import { C, cards, curve, el, section, slider } from "./kit";
import { throttled } from "./throttle";

export function hapticsView(device: Device) {
  const root = el("div");
  const send = throttled((id: SetId, v: number) => device.set(id, v));
  const bit = (id: number) => ((device.settings?.dirty ?? 0) >> id) & 1;

  // FEEL
  const feelSec = section("FEEL", "HOW A DETENT PUSHES BACK");
  const feels = [
    { value: 0, name: "SAW", sub: "CRISP SNAP" },
    { value: 1, name: "SINE", sub: "ROUND BUMP" },
    { value: 2, name: "VISCOSE", sub: "SMOOTH DRAG" },
  ];
  const curves: HTMLCanvasElement[] = [];
  const feelCards = cards<number>(
    feels.map((f) => ({
      value: f.value,
      body: () => {
        const cv = el("canvas", { width: 56, height: 16 });
        cv.style.width = "56px";
        cv.style.height = "16px";
        curves[f.value] = cv;
        return [cv, el("span", { class: "big" }, f.name), el("span", { class: "sub" }, f.sub)];
      },
    })),
    (v) => device.set(Set.FEEL, v),
    64,
  );
  feelSec.body.append(feelCards.root);

  // Sliders, named like the device (STEPS / SNAP / DAMP / SHAPE / AMP / PITCH) with the engineering name.
  const tuneSec = section("TUNE", "SCROLL OR DRAG");
  const rows: { id: SetId; key: "detents" | "kp" | "kd" | "shape" | "amp" | "pitch"; s: ReturnType<typeof slider> }[] = [
    { id: Set.DETENTS, key: "detents", s: slider({ label: "STEPS", caption: "DETENTS", ...Limits.detents, format: (v) => `${v}`, onInput: (v) => send(Set.DETENTS, v) }) },
    { id: Set.KP, key: "kp", s: slider({ label: "SNAP", caption: "KP", ...Limits.kp, format: (v) => v.toFixed(2), onInput: (v) => send(Set.KP, v) }) },
    { id: Set.KD, key: "kd", s: slider({ label: "DAMP", caption: "KD", ...Limits.kd, format: (v) => v.toFixed(3).replace(/^0/, ""), onInput: (v) => send(Set.KD, v) }) },
    { id: Set.SHAPE, key: "shape", s: slider({ label: "SHAPE", caption: "SAW ONLY", ...Limits.shape, format: (v) => `${v}%`, onInput: (v) => send(Set.SHAPE, v) }) },
    { id: Set.AMP, key: "amp", s: slider({ label: "AMP", caption: "CLICK VOLUME", ...Limits.amp, format: (v) => `${v}%`, onInput: (v) => send(Set.AMP, v) }) },
    { id: Set.PITCH, key: "pitch", s: slider({ label: "PITCH", caption: "CLICK", ...Limits.pitch, format: (v) => `${v.toFixed(2)}X`, onInput: (v) => send(Set.PITCH, v) }) },
  ];
  for (const r of rows) tuneSec.body.append(r.s.root);

  root.append(feelSec.root, tuneSec.root);

  let drawnFeel = -1;
  function update() {
    const s = device.settings;
    if (!s) return;
    feelCards.update(s.feel, !!bit(Set.FEEL));
    if (s.feel !== drawnFeel) {
      drawnFeel = s.feel;
      curves.forEach((cv, i) => curve(cv, i, i === s.feel ? C.amber : C.grey));
    }
    for (const r of rows) r.s.update(s[r.key], !!bit(r.id));
  }
  return { root, update };
}
