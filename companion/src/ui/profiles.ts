// PROFILES: what the knob is for (APP / KEYBOARD / MOUSE / MIDI) and, in APP, which app. Every
// profile can be edited here -- the built-in ones too (a changed built-in is stored on the knob
// and RESET TO DEFAULT brings the original back) -- and new ones made from scratch or copied.

import { DeviceError, type Device } from "../device";
import { HapticProfiles, HidType, ProfileFlag, Set } from "../proto";
import { blankProfile, ID_RE } from "../profile";
import { profileEditor } from "./editor";
import { cards, el, section } from "./kit";

export function profilesView(device: Device) {
  const root = el("div");
  const listView = el("div");
  let editor: ReturnType<typeof profileEditor> | null = null;
  const bit = (id: number) => ((device.settings?.dirty ?? 0) >> id) & 1;

  const modeSec = section("MODE", "WHAT THE KNOB DRIVES");
  const modes = cards<number>(
    [
      { value: HidType.APP, body: () => [el("span", { class: "big" }, "APP"), el("span", { class: "sub" }, "APP PROFILES")] },
      { value: HidType.MOUSE, body: () => [el("span", { class: "big" }, "MOUSE"), el("span", { class: "sub" }, "SCROLL WHEEL")] },
      { value: HidType.KEYBOARD, body: () => [el("span", { class: "big" }, "KEYS"), el("span", { class: "sub" }, "KEYBOARD")] },
      { value: HidType.MIDI, body: () => [el("span", { class: "big" }, "MIDI"), el("span", { class: "sub" }, "SETTINGS ONLY")] },
    ],
    (v) => device.set(Set.HID_TYPE, v),
    100,
  );
  modeSec.body.append(modes.root);

  // APP: the profile grid (rebuilt when the device's profile list changes), then what to do
  // with the chosen one.
  const appSec = section("APP PROFILE", "CLICK TO USE");
  const grid = el("div");
  const note = el("div", { class: "hint ed-status" });
  const editBtn = el("button", { class: "btn primary", onclick: () => openEditor(device.settings?.profile ?? 0) }, "EDIT");
  const dupBtn = el("button", { class: "btn", onclick: () => void duplicate() }, "DUPLICATE");
  const newBtn = el("button", { class: "btn", onclick: () => void create() }, "+ NEW PROFILE");
  appSec.body.append(grid, el("div", { class: "actions" }, editBtn, dupBtn, newBtn), note);
  let builtFor = "";
  let profileCards: ReturnType<typeof cards<number>> | null = null;

  // MOUSE / KEYS: the haptic profile the knob uses in that mode (tuned on the HAPTICS tab).
  const modeHapticSec = section("HAPTIC", "HOW THE KNOB FEELS IN THIS MODE");
  const modeHaptic = cards<number>(
    HapticProfiles.map((p, i) => ({
      value: i,
      body: () => [el("span", { class: "big" }, p.name), el("span", { class: "sub" }, p.detents ? `${p.detents} PER TURN` : "NO STEPS")],
    })),
    (v) => device.set(Set.MODE_HAPTIC, v),
    64,
  );
  modeHapticSec.body.append(modeHaptic.root);

  // MIDI: the channel.
  const midiSec = section("MIDI CHANNEL");
  const chan = el("span");
  midiSec.body.append(
    el(
      "div",
      { class: "stepper" },
      el("button", { onclick: () => device.set(Set.MIDI_CH, (device.settings?.midiChannel ?? 1) - 1) }, "<"),
      chan,
      el("button", { onclick: () => device.set(Set.MIDI_CH, (device.settings?.midiChannel ?? 1) + 1) }, ">"),
    ),
    el("p", { class: "hint" }, "STORED ONLY FOR NOW: NO MIDI IS SENT YET."),
  );

  listView.append(modeSec.root, appSec.root, modeHapticSec.root, midiSec.root);
  root.append(listView);

  function origin(flags: number): { text: string; warn: boolean } {
    const live = (flags & ProfileFlag.LIVE) !== 0;
    const base = flags & ProfileFlag.BUILTIN ? (flags & ProfileFlag.STORED ? "BUILT-IN, CHANGED" : "BUILT-IN") : flags & ProfileFlag.STORED ? "YOURS" : "NEW";
    return { text: live ? "NOT SAVED" : base, warn: live };
  }

  function buildProfiles() {
    const key = device.profiles.map((p) => `${p?.id}:${p?.name}:${p?.flags}:${!!p?.icon}`).join(",");
    if (key === builtFor) return;
    builtFor = key;
    grid.replaceChildren();
    profileCards = cards<number>(
      device.profiles.filter(Boolean).map((p) => ({
        value: p.index,
        body: () => {
          const icon = el("canvas", { width: 48, height: 48 });
          icon.style.width = icon.style.height = "48px";
          if (p.icon) icon.getContext("2d")!.putImageData(p.icon, 0, 0);
          const o = origin(p.flags);
          return [icon, el("span", {}, p.name), el("span", { class: o.warn ? "sub warn" : "sub" }, o.text)];
        },
      })),
      (v) => device.set(Set.PROFILE, v),
      88,
    );
    grid.append(profileCards.root);
  }

  // --- editor ---

  function openEditor(index: number) {
    if (!device.profiles[index]) return;
    editor = profileEditor(device, index, () => {
      editor = null;
      root.replaceChildren(listView);
      update();
    });
    root.replaceChildren(editor.root);
  }

  // A free id from a name: "MY APP" -> "my_app", "my_app2", ...
  function freeId(name: string): string {
    const base = (name.toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "") || "profile").slice(0, 9);
    const taken = new globalThis.Set(device.profiles.map((p) => p?.id));
    for (let n = 1; n < 100; n++) {
      const id = n === 1 ? base : `${base}${n}`;
      if (!taken.has(id) && ID_RE.test(id)) return id;
    }
    return `p${Date.now() % 1e9}`;
  }

  async function addAndEdit(make: () => Promise<Parameters<Device["uploadProfile"]>[0]>) {
    note.textContent = "MAKING IT...";
    try {
      const p = await make();
      const r = await device.uploadProfile(p, true);
      await device.set(Set.PROFILE, r.index);
      await waitFor(() => device.profiles[r.index]?.id === p.id);
      note.textContent = "";
      openEditor(r.index);
    } catch (e) {
      note.textContent = e instanceof DeviceError ? e.message : String(e).toUpperCase();
    }
  }

  function create() {
    return addAndEdit(async () => blankProfile(freeId("NEW PROFILE"), "NEW PROFILE"));
  }

  function duplicate() {
    const from = device.settings?.profile ?? 0;
    return addAndEdit(async () => {
      const p = await device.readProfile(from);
      const name = `${p.name.slice(0, 10)} COPY`;
      return { ...p, id: freeId(name), name };
    });
  }

  // ?edit=<index> opens the editor on that profile once the list is in (screenshots, demo links).
  let editParam = new URLSearchParams(location.search).get("edit");

  function update() {
    if (editParam !== null && device.profiles[Number(editParam)]) {
      const i = Number(editParam);
      editParam = null;
      openEditor(i);
    }
    if (editor) {
      editor.update();
      return;
    }
    const s = device.settings;
    if (!s) return;
    modes.update(s.hidType, !!bit(Set.HID_TYPE));
    appSec.root.style.display = s.hidType === HidType.APP ? "" : "none";
    midiSec.root.style.display = s.hidType === HidType.MIDI ? "" : "none";
    modeHapticSec.root.style.display = s.hidType === HidType.MOUSE || s.hidType === HidType.KEYBOARD ? "" : "none";
    modeHaptic.update(s.modeHaptic, !!bit(Set.MODE_HAPTIC));
    buildProfiles();
    profileCards?.update(s.profile, !!bit(Set.PROFILE));
    const full = device.profiles.length >= 16;
    dupBtn.disabled = newBtn.disabled = full;
    editBtn.textContent = `EDIT ${device.profiles[s.profile]?.name ?? ""}`.trim();
    chan.textContent = String(s.midiChannel).padStart(2, "0");
  }
  return { root, update };
}

// The profile list reloads after a change; resolves once it shows what we're waiting for.
function waitFor(ok: () => boolean, ms = 3000): Promise<void> {
  return new Promise((resolve) => {
    const t0 = performance.now();
    const tick = () => (ok() || performance.now() - t0 > ms ? resolve() : window.setTimeout(tick, 50));
    tick();
  });
}
