// PROFILES: what the knob is for (APP / KEYBOARD / MOUSE / MIDI) and, in APP, which app --
// the built-in profiles with the icons the device itself draws.

import type { Device } from "../device";
import { HidType, Set } from "../proto";
import { cards, el, section } from "./kit";

export function profilesView(device: Device) {
  const root = el("div");
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

  // APP: the profile grid (rebuilt when the device's profile list changes).
  const appSec = section("APP PROFILE", "BUILT INTO THE FIRMWARE");
  const grid = el("div");
  appSec.body.append(grid);
  let builtFor = "";
  let profileCards: ReturnType<typeof cards<number>> | null = null;

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

  root.append(modeSec.root, appSec.root, midiSec.root);

  function buildProfiles() {
    const key = device.profiles.map((p) => `${p?.id}:${!!p?.icon}`).join(",");
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
          return [
            icon,
            el("span", {}, p.name),
            el("span", { class: "sub" }, p.legend.map((l, i) => `F${i + 1} ${l || "-"}`).slice(0, 2).join("  ")),
            el("span", { class: "sub" }, p.legend.map((l, i) => `F${i + 1} ${l || "-"}`).slice(2).join("  ")),
          ];
        },
      })),
      (v) => device.set(Set.PROFILE, v),
      88,
    );
    grid.append(profileCards.root);
  }

  function update() {
    const s = device.settings;
    if (!s) return;
    modes.update(s.hidType, !!bit(Set.HID_TYPE));
    appSec.root.style.display = s.hidType === HidType.APP ? "" : "none";
    midiSec.root.style.display = s.hidType === HidType.MIDI ? "" : "none";
    buildProfiles();
    profileCards?.update(s.profile, !!bit(Set.PROFILE));
    chan.textContent = String(s.midiChannel).padStart(2, "0");
  }
  return { root, update };
}
