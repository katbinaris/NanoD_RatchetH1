// DEVICE: bindings (MAC / PC), screen rotation, boot mode, and what firmware is on the knob.

import type { Device } from "../device";
import { Boot, Host, Set } from "../proto";
import { cards, el, section } from "./kit";

// The DISPLAY screen's arrow: a triangle head on a short shaft, 12x14 px at 2x.
function upArrow(): HTMLCanvasElement {
  const c = el("canvas", { width: 12, height: 14 });
  c.style.width = "24px";
  c.style.height = "28px";
  const ctx = c.getContext("2d")!;
  ctx.fillStyle = "#ffffff";
  for (let y = 0; y < 6; y++) ctx.fillRect(5 - y, y, 2 + y * 2, 1);
  ctx.fillRect(4, 6, 4, 8);
  return c;
}

export function deviceView(device: Device) {
  const root = el("div");
  const bit = (id: number) => ((device.settings?.dirty ?? 0) >> id) & 1;

  const bindSec = section("BINDINGS", "THE COMPUTER ON THE OTHER END");
  const bind = cards<number>(
    [
      { value: Host.MAC, body: () => [el("span", { class: "big" }, "MAC"), el("span", { class: "sub" }, "CMD")] },
      { value: Host.PC, body: () => [el("span", { class: "big" }, "PC"), el("span", { class: "sub" }, "CTRL")] },
    ],
    (v) => device.set(Set.HOST, v),
    80,
  );
  const bindHint = el("p", { class: "hint" });
  bindSec.body.append(bind.root, bindHint);

  const rotSec = section("DISPLAY", "SCREEN ROTATION");
  const rot = cards<number>(
    [0, 1, 2, 3].map((q) => ({
      value: q,
      body: () => {
        const arrow = upArrow();
        arrow.style.transform = `rotate(${q * 90}deg)`;
        arrow.dataset.arrow = "";
        return [arrow, el("span", { class: "sub" }, `${q * 90}`)];
      },
    })),
    (v) => device.set(Set.ROTATION, v),
    50,
  );
  rotSec.body.append(rot.root);

  const bootSec = section("BOOT MODE", "APPLIES AFTER A RESTART");
  const boot = cards<number>(
    [
      { value: Boot.HID, body: () => [el("span", { class: "big" }, "HID"), el("span", { class: "sub" }, "NORMAL USE")] },
      { value: Boot.SERIAL, body: () => [el("span", { class: "big" }, "SERIAL"), el("span", { class: "sub" }, "FOR FLASHING")] },
    ],
    (v) => device.set(Set.BOOT, v),
    80,
  );
  const bootWarn = el("p", { class: "warn" }, "IN SERIAL MODE THE KNOB HAS NO HID: THIS APP CAN'T REACH IT UNTIL IT BOOTS IN HID AGAIN.");
  bootSec.body.append(boot.root, bootWarn);

  const fwSec = section("FIRMWARE");
  const fw = el("div", { class: "tile" });
  fwSec.body.append(fw);

  root.append(bindSec.root, rotSec.root, bootSec.root, fwSec.root);

  function update() {
    const s = device.settings;
    if (!s) return;
    bind.update(s.host, !!bit(Set.HOST));
    bindHint.textContent = s.host === Host.PC ? "CMD SHORTCUTS ARE SENT AS CTRL." : "SHORTCUTS ARE SENT AS WRITTEN.";
    rot.update(s.rotation, !!bit(Set.ROTATION));
    boot.update(s.boot, !!bit(Set.BOOT));
    bootWarn.style.display = s.boot === Boot.SERIAL ? "" : "none";
    const h = device.hello;
    fw.replaceChildren(
      ...[
        ["VERSION", h?.version ?? "-"],
        ["BUILT", h?.date ?? "-"],
        ["PROTOCOL", h ? String(h.proto) : "-"],
        ["PROFILES", h ? String(h.profileCount) : "-"],
        ["EXTENSIONS", device.ext ? `V${device.ext} (LOOK)` : device.ext === 0 ? "NONE" : "-"],
        ["LINK", device.kind === "tauri" ? "USB (APP)" : "WEBHID"],
      ].map(([k, v]) => el("div", { class: "kv" }, el("span", {}, k), el("span", {}, v.toUpperCase()))),
    );
  }
  return { root, update };
}
