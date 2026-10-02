// LOOK: the idle word and LIGHTS -- the fork's extensions (ext_proto.h). The word is stored on
// the knob as soon as it's set; LIGHTS are live while you change them and kept by SAVE (or F2
// on the knob's LIGHTS screen), like the other settings.

import type { Device } from "../device";
import { IDLE_TEXT_MAX, LIGHT_FX, LIGHT_FX_MOVING, LightSrc, type Lights } from "../proto";
import { text } from "./fields";
import { cards, el, section, slider } from "./kit";
import { throttled } from "./throttle";

const hsl = (hue: number, sat: number) => `hsl(${hue} ${sat}% ${50 + (100 - sat) / 4}%)`;

export function lookView(device: Device) {
  const root = el("div");

  // --- the idle word ---
  const wordSec = section("IDLE WORD", "ON THE LOADING AND IDLE SCREENS");
  let typed: string | null = null; // being edited here: the knob's word doesn't overwrite it
  // The knob's font (Silkscreen) has all of printable ASCII; lowercase draws as small capitals.
  const input = text("", IDLE_TEXT_MAX, (v) => (typed = v), { placeholder: "QUADRA", upper: false });
  input.addEventListener("keydown", (e) => e.key === "Enter" && setWord(input.value));
  const setBtn = el("button", { class: "btn primary", onclick: () => setWord(input.value) }, "SET");
  const stockBtn = el("button", { class: "btn", onclick: () => setWord("") }, "QUADRA");
  const wordHint = el("p", { class: "hint" }, `UP TO ${IDLE_TEXT_MAX} CHARACTERS. EMPTY = QUADRA. STORED ON THE KNOB AT ONCE.`);
  wordSec.body.append(el("div", { class: "actions" }, input, setBtn, stockBtn), wordHint);
  function setWord(w: string) {
    typed = null;
    w = w.replace(/[^\x20-\x7e]/g, "").trim();
    input.value = w;
    void device.setIdleText(w);
  }

  // --- LIGHTS ---
  // The whole look goes out each time (the knob takes one LIGHTS at a time), at most every 40ms.
  let want: Lights | null = null;
  let wantAt = 0;
  const send = throttled((_: string, __: number) => want && void device.setLights(want));
  const change = (patch: Partial<Lights>) => {
    if (!device.prefs) return;
    want = { ...(want ?? device.prefs), ...patch };
    wantAt = performance.now();
    send("lights", 0);
    update();
  };

  const colorSec = section("COLOR", "THE RING AND THE KEYS");
  const src = cards<number>(
    [
      { value: LightSrc.APP, body: () => [el("span", { class: "big" }, "APP"), el("span", { class: "sub" }, "THE PROFILE'S, OR THE COVER'S")] },
      { value: LightSrc.CUSTOM, body: () => [el("span", { class: "big" }, "CUSTOM"), el("span", { class: "sub" }, "YOUR HUE")] },
    ],
    (v) => change({ src: v }),
    120,
  );
  const swatch = el("div", { class: "swatch" });
  swatch.style.cssText = "height:10px;margin:8px 0 2px;border:1px solid #3a3a3a";
  const hue = slider({ label: "HUE", caption: "0-359", min: 0, max: 355, step: 5, format: (v) => `${v}`, onInput: (v) => change({ hue: v }) });
  const sat = slider({ label: "SAT", caption: "SATURATION", min: 0, max: 100, step: 5, format: (v) => `${v}%`, onInput: (v) => change({ sat: v }) });
  colorSec.body.append(src.root, swatch, hue.root, sat.root);

  const fxSec = section("EFFECT", "AT REST");
  const fx = cards<number>(
    LIGHT_FX.map((name, i) => ({ value: i, body: () => [el("span", { class: "big" }, name)] })),
    (v) => change({ fx: v }),
    84,
  );
  const speed = slider({ label: "SPEED", caption: "BREATHE, SPIN, RAINBOW", min: 1, max: 10, step: 1, format: (v) => `${v}`, onInput: (v) => change({ speed: v }) });
  const level = slider({ label: "LEVEL", caption: "BRIGHTNESS", min: 10, max: 200, step: 10, format: (v) => `${v}%`, onInput: (v) => change({ level: v }) });
  fxSec.body.append(fx.root, speed.root, level.root);

  root.append(wordSec.root, colorSec.root, fxSec.root);

  function update() {
    const p = device.prefs;
    if (!p) return;
    // What the knob says, unless a change of ours is still on its way there (not for ever: one
    // it turned down shows as what the knob really has).
    const arrived = want && want.src === p.src && want.fx === p.fx && want.hue === p.hue && want.sat === p.sat && want.speed === p.speed && want.level === p.level;
    if (arrived || performance.now() - wantAt > 2000) want = null;
    const l = want ?? p;
    const dirty = p.lightsDirty || want !== null;
    if (typed === null && document.activeElement !== input) input.value = p.idleText;
    src.update(l.src, dirty);
    const custom = l.src === LightSrc.CUSTOM;
    hue.update(l.hue, dirty);
    sat.update(l.sat, dirty);
    hue.mute(!custom);
    sat.mute(!custom);
    swatch.style.background = custom ? hsl(l.hue, l.sat) : "repeating-linear-gradient(90deg,#3a3a3a 0 6px,#000 6px 12px)";
    fx.update(l.fx, dirty);
    speed.update(l.speed, dirty);
    speed.mute(!LIGHT_FX_MOVING.has(l.fx));
    level.update(l.level, dirty);
  }
  return { root, update };
}
