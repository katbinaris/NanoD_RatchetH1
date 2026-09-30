// Form controls for the profile editor, in the device's style: 1px dark frames, amber when
// focused or on, uppercase pixel text. Each one calls `on` with the new value; none of them
// re-renders anything else.

import { hidFromCode, keyName, Mod, modifiersFromEvent, type Key } from "../profile";
import { el } from "./kit";

// A labelled row: name on the left, control(s) on the right.
export function field(label: string, ...controls: (Node | string | null)[]) {
  return el("div", { class: "field" }, el("span", { class: "flabel" }, label), el("span", { class: "fctl" }, ...controls));
}

export function text(value: string, max: number, on: (v: string) => void, opts: { upper?: boolean; width?: number; placeholder?: string } = {}) {
  const i = el("input", { class: "txt", value, maxlength: max, spellcheck: "false", placeholder: opts.placeholder ?? "" });
  if (opts.width) i.style.width = `${opts.width}px`;
  i.addEventListener("input", () => {
    if (opts.upper !== false) {
      const at = i.selectionStart;
      i.value = i.value.toUpperCase();
      i.setSelectionRange(at, at);
    }
    on(i.value);
  });
  i.addEventListener("keydown", (e) => e.stopPropagation());
  return i;
}

export function num(value: number, o: { min: number; max: number; step: number; on: (v: number) => void; width?: number; zero?: string }) {
  const i = el("input", { class: "txt num", type: "number", value, min: o.min, max: o.max, step: o.step });
  i.style.width = `${o.width ?? 44}px`;
  if (o.zero) i.placeholder = o.zero;
  if (o.zero && value === 0) i.value = "";
  i.addEventListener("change", () => {
    const v = i.value === "" ? 0 : Math.min(o.max, Math.max(o.min, Number(i.value)));
    i.value = o.zero && v === 0 ? "" : String(v);
    o.on(v);
  });
  i.addEventListener("keydown", (e) => e.stopPropagation());
  return i;
}

// One of a few: small buttons in a row, the chosen one amber.
export function seg<T>(options: { value: T; label: string; disabled?: boolean }[], value: T, on: (v: T) => void) {
  const root = el("span", { class: "seg" });
  const btns = options.map((o) => {
    const b = el("button", { type: "button", disabled: !!o.disabled }, o.label);
    b.addEventListener("click", (e) => {
      e.preventDefault();
      set(o.value);
      on(o.value);
    });
    root.append(b);
    return { o, b };
  });
  const set = (v: T) => btns.forEach(({ o, b }) => b.classList.toggle("on", o.value === v));
  set(value);
  return root;
}

// Any of a few bits.
export function bits(options: { bit: number; label: string }[], value: number, on: (v: number) => void) {
  const root = el("span", { class: "seg" });
  let cur = value;
  for (const o of options) {
    const b = el("button", { type: "button" }, o.label);
    b.classList.toggle("on", (cur & o.bit) !== 0);
    b.addEventListener("click", (e) => {
      e.preventDefault();
      cur ^= o.bit;
      b.classList.toggle("on", (cur & o.bit) !== 0);
      on(cur);
    });
    root.append(b);
  }
  return root;
}

export const MOD_BITS = [
  { bit: Mod.GUI, label: "CMD" },
  { bit: Mod.SHIFT, label: "SHIFT" },
  { bit: Mod.ALT, label: "OPT" },
  { bit: Mod.CTRL, label: "CTRL" },
];

// A shortcut: click, then press it. Modifiers can also be toggled by hand -- Cmd+Q and friends
// never reach the page (the OS takes them), so those are set as CMD + Q.
export function keyInput(value: Key | undefined, on: (k: Key | undefined) => void) {
  let k: Key = value ? [value[0], value[1]] : [0, 0];
  const box = el("button", { type: "button", class: "keybox" });
  const clear = el("button", { type: "button", class: "keyclear", title: "CLEAR" }, "X");
  const mods = el("span");
  const show = () => {
    box.textContent = keyName(k) || "NONE";
    box.classList.toggle("empty", !k[0] && !k[1]);
    clear.style.visibility = k[0] || k[1] ? "" : "hidden";
    mods.replaceChildren(
      bits(MOD_BITS, k[0], (m) => {
        k = [m, k[1]];
        commit();
      }),
    );
  };
  const commit = () => {
    show();
    on(k[0] || k[1] ? [k[0], k[1]] : undefined);
  };
  let listening = false;
  const stop = () => {
    listening = false;
    box.classList.remove("listen");
    show();
  };
  box.addEventListener("click", (e) => {
    e.preventDefault();
    listening = !listening;
    box.classList.toggle("listen", listening);
    box.textContent = listening ? "PRESS A KEY" : keyName(k) || "NONE";
    if (listening) box.focus();
  });
  box.addEventListener("keydown", (e) => {
    if (!listening) return;
    e.preventDefault();
    e.stopPropagation();
    const code = hidFromCode(e.code);
    if (code === undefined) return; // a modifier alone: wait for the key
    k = [modifiersFromEvent(e), code];
    stop();
    commit();
  });
  box.addEventListener("blur", () => listening && stop());
  clear.addEventListener("click", (e) => {
    e.preventDefault();
    k = [0, 0];
    commit();
  });
  show();
  return el("span", { class: "keyin" }, box, clear, mods);
}

// A button that asks once: the first click arms it ("SURE?"), a second within 3s does it.
export function confirmBtn(label: string, on: () => void, cls = "btn") {
  const b = el("button", { type: "button", class: cls }, label);
  let armed = 0;
  b.addEventListener("click", () => {
    if (armed) {
      window.clearTimeout(armed);
      armed = 0;
      b.textContent = label;
      b.classList.remove("primary");
      on();
      return;
    }
    b.textContent = `${label}?`;
    b.classList.add("primary");
    armed = window.setTimeout(() => {
      armed = 0;
      b.textContent = label;
      b.classList.remove("primary");
    }, 3000);
  });
  return b;
}
