// LOOK: the idle word, LIGHTS, MUSIC's cover and the CLOCK app -- the fork's extensions
// (ext_proto.h). The word and the cover are stored on the knob as soon as they're set; LIGHTS
// are live while you change them and kept by SAVE (or F2 on the knob's LIGHTS screen), like the
// other settings.

import type { Device } from "../device";
import { CLOCK_SLOTS, COVER_STYLES, ClockFlag, EXT_CLOCK_VERSION, IDLE_TEXT_MAX, LIGHT_FX, LIGHT_FX_MOVING, LightSrc, type Lights } from "../proto";
import { CITIES } from "../tzdata";
import { bits, text } from "./fields";
import { cards, el, section, slider } from "./kit";
import { throttled } from "./throttle";

const utc = (min: number) => {
  const a = Math.abs(min);
  return `UTC${min < 0 ? "-" : "+"}${Math.floor(a / 60)}${a % 60 ? `:${String(a % 60).padStart(2, "0")}` : ""}`;
};
const FORMAT = [
  { bit: ClockFlag.H24, label: "24 HOUR" },
  { bit: ClockFlag.SECONDS, label: "SECONDS" },
  { bit: ClockFlag.DATE, label: "DATE" },
  { bit: ClockFlag.LED, label: "LED SECONDS" },
];

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

  // --- MUSIC's cover (extensions v8): stored on the knob at once ---
  const COVER_SUB = ["THE COVER, FULL SCREEN", "THE GLASS IS THE RECORD", "A SLEEVE, THE RECORD SLIDES OUT", "A BIG SLEEVE SLIDES AWAY"];
  const coverSec = section("MUSIC", "THE COVER, NOW PLAYING");
  const cover = cards<number>(
    COVER_STYLES.map((name, i) => ({ value: i, body: () => [el("span", { class: "big" }, name), el("span", { class: "sub" }, COVER_SUB[i])] })),
    (v) => void device.setCoverStyle(v),
    120,
  );
  coverSec.body.append(cover.root, el("p", { class: "hint" }, "ON THE KNOB: TAP F4 WHILE MUSIC PLAYS TO STEP THROUGH THEM."));

  // --- the CLOCK app (extensions v5): stored on the knob as you change it ---
  const clockSec = section("CLOCK", "THE CLOCK APP");
  const fmtBox = el("span");
  let fmtShown = -1;
  const localLine = el("div", { class: "kv" });
  const zoneSel: HTMLSelectElement[] = [], zoneOff: HTMLElement[] = [];
  const zoneRows = el("div");
  for (let s = 1; s < CLOCK_SLOTS; s++) {
    const sel = el("select", { class: "txt" });
    sel.append(el("option", { value: "" }, "NONE"), ...CITIES.map((c, i) => el("option", { value: String(i) }, c.city.toUpperCase())));
    sel.addEventListener("change", () => {
      const c = sel.value === "" ? null : CITIES[Number(sel.value)];
      if (c || sel.value === "") void device.setClockZone(s, c ? c.label : "", c ? c.rule : "");
    });
    const off = el("span", { class: "hint" });
    zoneSel[s] = sel;
    zoneOff[s] = off;
    zoneRows.append(el("div", { class: "field" }, el("span", { class: "flabel" }, `ZONE ${s}`), el("span", { class: "fctl" }, sel, off)));
  }
  clockSec.body.append(
    el("div", { class: "field" }, el("span", { class: "flabel" }, "SHOW"), el("span", { class: "fctl" }, fmtBox)),
    localLine,
    zoneRows,
    el("p", { class: "hint" }, "IN THE CLOCK APP: TURN THE KNOB TO STEP THROUGH THE ZONES. F1 12/24 HOURS, F2 SECONDS, F3 DATE."),
  );

  root.append(wordSec.root, colorSec.root, fxSec.root, coverSec.root, clockSec.root);

  function updateClock() {
    clockSec.root.style.display = (device.ext ?? 0) >= EXT_CLOCK_VERSION ? "" : "none";
    const c0 = device.clockSlots[0];
    if (c0) {
      if (c0.flags !== fmtShown) {
        fmtShown = c0.flags;
        fmtBox.replaceChildren(bits(FORMAT, c0.flags, (v) => void device.setClockFlags(v)));
      }
      const local = c0.valid ? `${c0.label}   ${utc(c0.offsetMin)}   THIS MAC'S` : "THE TIME ISN'T SET YET: WIFI, OR THE MAC SERVICE";
      localLine.replaceChildren(el("span", {}, "LOCAL"), el("span", {}, local));
    }
    for (let s = 1; s < CLOCK_SLOTS; s++) {
      const z = device.clockSlots[s];
      if (!z) continue;
      zoneOff[s].textContent = z.label ? `  ${utc(z.offsetMin)}` : "";
      if (document.activeElement === zoneSel[s]) continue;
      let v = "";
      if (z.label) {
        const i = CITIES.findIndex((c) => c.label === z.label && c.rule === z.rule);
        v = i >= 0 ? String(i) : "other";
        let other = zoneSel[s].querySelector<HTMLOptionElement>('option[value="other"]');
        if (i < 0 && !other) zoneSel[s].append((other = el("option", { value: "other" })));
        if (other) other.textContent = z.label; // set elsewhere (quadra.py clock), not in the list
      }
      zoneSel[s].value = v;
    }
  }

  function update() {
    updateClock();
    const p = device.prefs;
    if (!p) return;
    // What the knob says, unless a change of ours is still on its way there (not for ever: one
    // it turned down shows as what the knob really has).
    const arrived = want && want.src === p.src && want.fx === p.fx && want.hue === p.hue && want.sat === p.sat && want.speed === p.speed && want.level === p.level;
    if (arrived || performance.now() - wantAt > 2000) want = null;
    const l = want ?? p;
    const dirty = p.lightsDirty || want !== null;
    if (typed === null && document.activeElement !== input) input.value = p.idleText;
    coverSec.root.style.display = p.coverStyles > 0 ? "" : "none";
    cover.update(p.coverStyle, false);
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
