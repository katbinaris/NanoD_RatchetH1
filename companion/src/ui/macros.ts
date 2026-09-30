// Macros in the profile editor: the list (name + steps: key / text / pause) with RECORD, which
// turns what you type into steps. They're stored in the profile and run by the knob itself.
// Keys and wheel commands refer to a macro by name; renaming one updates them, deleting one
// clears them.

import { ASCII_RE, hidFromCode, MAX, modifiersFromEvent, type Macro, type ProfileJson, type Step } from "../profile";
import { confirmBtn, keyInput, num, seg, text } from "./fields";
import { el, section } from "./kit";

// Pick one of the profile's macros (or show how to make one).
export function macroPick(p: ProfileJson, value: string | undefined, on: (name: string) => void) {
  const names = (p.macros ?? []).map((m) => m.name).filter(Boolean);
  if (!names.length) return el("span", { class: "hint" }, "NO MACROS YET: ADD ONE UNDER MACROS");
  return seg(
    names.map((n) => ({ value: n, label: n })),
    value ?? "",
    on,
  );
}

// Every place that names a macro, for renames and deletes.
function eachRef(p: ProfileJson, fn: (get: () => string | undefined, set: (v: string | undefined) => void) => void) {
  for (const a of Object.values(p.slots ?? {})) {
    if (!a) continue;
    fn(() => a.macro, (v) => (a.macro = v));
    fn(() => a.tap_macro, (v) => (a.tap_macro = v));
  }
  for (const r of p.rings ?? []) for (const c of r.cmds) fn(() => c.macro, (v) => (c.macro = v));
}

export function macrosSection(p: ProfileJson, touch: () => void, render: () => void) {
  p.macros ??= [];
  const macros = p.macros;
  const s = section("MACROS", "TYPED BY THE KNOB ITSELF");
  if (!macros.length) s.body.append(el("div", { class: "hint" }, "KEY PRESSES, TEXT AND PAUSES THE KNOB SENDS ON ONE PRESS. USE ONE FROM A KEY (TAP) OR THE COMMAND WHEEL."));
  macros.forEach((m, i) => s.body.append(macroEditor(p, m, i, touch, render)));
  const add = el("button", { type: "button", class: "btn", disabled: macros.length >= MAX.macros }, "+ MACRO");
  add.addEventListener("click", () => {
    let n = 1;
    while (macros.some((m) => m.name === `MACRO ${n}`)) n++;
    macros.push({ name: `MACRO ${n}`, steps: [] });
    render();
    touch();
  });
  s.body.append(add);
  return s.root;
}

function macroEditor(p: ProfileJson, m: Macro, i: number, touch: () => void, render: () => void) {
  const box = el("div", { class: "ring" });
  const steps = el("div");
  const count = el("span", { class: "hint" });

  const drawSteps = () => {
    steps.replaceChildren(...m.steps.map((st, j) => stepRow(m, st, j)));
    count.textContent = `${m.steps.length} / ${MAX.steps} STEPS`;
    addKey.disabled = addText.disabled = addWait.disabled = recBtn.disabled = m.steps.length >= MAX.steps && !recording;
  };

  const stepRow = (m: Macro, st: Step, j: number) => {
    let ctl: Node;
    let kind: string;
    if ("key" in st) {
      kind = "KEY";
      ctl = keyInput(st.key, (k) => ((st.key = k ?? [0, 0]), touch()));
    } else if ("text" in st) {
      kind = "TEXT";
      const t = text(st.text, MAX.text, (v) => {
        st.text = v;
        t.classList.toggle("bad", !ASCII_RE.test(v));
        touch();
      }, { upper: false, width: 200, placeholder: "PLAIN ASCII" });
      ctl = t;
    } else {
      kind = "PAUSE";
      ctl = el("span", { class: "fctl" }, num(st.wait, { min: 0, max: MAX.waitMs, step: 10, width: 56, on: (v) => ((st.wait = v), touch()) }), el("span", { class: "hint" }, "MS"));
    }
    const move = (d: number) => {
      const k = j + d;
      if (k < 0 || k >= m.steps.length) return;
      [m.steps[j], m.steps[k]] = [m.steps[k], m.steps[j]];
      drawSteps();
      touch();
    };
    return el(
      "div",
      { class: "cmd" },
      el("span", { class: "steptag" }, kind),
      ctl,
      el("span", { class: "spacer" }),
      el("button", { type: "button", class: "mini", onclick: () => move(-1) }, "UP"),
      el("button", { type: "button", class: "mini", onclick: () => move(1) }, "DN"),
      el("button", { type: "button", class: "mini", onclick: () => (m.steps.splice(j, 1), drawSteps(), touch()) }, "X"),
    );
  };

  const push = (st: Step) => {
    if (m.steps.length >= MAX.steps) return;
    m.steps.push(st);
    drawSteps();
    touch();
  };
  const addKey = el("button", { type: "button", class: "btn", onclick: () => push({ key: [0, 0] }) }, "+ KEY");
  const addText = el("button", { type: "button", class: "btn", onclick: () => push({ text: "" }) }, "+ TEXT");
  const addWait = el("button", { type: "button", class: "btn", onclick: () => push({ wait: 100 }) }, "+ PAUSE");

  // --- RECORD: keys pressed while it's on become steps (with the pauses between them, if
  // TIMING is on). Modifiers alone don't count; held-key repeats don't either. Keys the OS
  // takes first (Cmd+Q, Cmd+Tab) never arrive -- add those with + KEY.
  let recording = false;
  let timing = false;
  let last = 0;
  const recBtn = el("button", { type: "button", class: "btn" }, "RECORD");
  const onKey = (e: KeyboardEvent) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.repeat) return;
    const code = hidFromCode(e.code);
    if (code === undefined) return;
    const now = performance.now();
    if (timing && m.steps.length && last) {
      const gap = Math.min(MAX.waitMs, Math.round((now - last) / 10) * 10);
      if (gap >= 50) push({ wait: gap });
    }
    last = now;
    push({ key: [modifiersFromEvent(e), code] });
    if (m.steps.length >= MAX.steps) stop();
  };
  const start = () => {
    recording = true;
    last = 0;
    recBtn.textContent = "STOP";
    recBtn.classList.add("primary", "fill", "blink");
    window.addEventListener("keydown", onKey, true);
  };
  const stop = () => {
    recording = false;
    recBtn.textContent = "RECORD";
    recBtn.classList.remove("primary", "fill", "blink");
    window.removeEventListener("keydown", onKey, true);
    drawSteps();
  };
  recBtn.addEventListener("click", () => (recording ? stop() : start()));
  // The editor re-renders sections; a recording never outlives its button.
  new MutationObserver((_, obs) => {
    if (!recBtn.isConnected) {
      if (recording) stop();
      obs.disconnect();
    }
  }).observe(document.body, { childList: true, subtree: true });

  const oldName = () => m.name;
  let prev = oldName();
  box.append(
    el(
      "div",
      { class: "ring-head" },
      text(m.name, MAX.name, (v) => {
        eachRef(p, (get, set) => get() === prev && set(v));
        m.name = v;
        prev = v;
        touch();
      }, { width: 120 }),
      count,
      el("span", { class: "spacer" }),
      confirmBtn("DELETE MACRO", () => {
        eachRef(p, (get, set) => get() === m.name && set(undefined));
        p.macros!.splice(i, 1);
        render();
        touch();
      }),
    ),
    steps,
    el(
      "div",
      { class: "actions tight" },
      addKey,
      addText,
      addWait,
      el("span", { class: "spacer" }),
      seg(
        [
          { value: false, label: "KEYS ONLY" },
          { value: true, label: "WITH TIMING" },
        ],
        timing,
        (v) => (timing = v),
      ),
      recBtn,
    ),
  );
  drawSteps();
  return box;
}
