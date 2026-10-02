// DEVICE: bindings (MAC / PC), screen rotation, boot mode, and what firmware is on the knob.

import type { Device } from "../device";
import { Boot, EXT_NET_VERSION, EXT_WIFI_LINK_VERSION, Host, NET_STATE, NetState, Set } from "../proto";
import { text } from "./fields";
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

  // WiFi (extensions v4): the clock comes from the internet once it's connected.
  const wifiSec = section("WIFI", "SET OVER USB; THE PASSWORD STAYS ON THE KNOB");
  const wifiState = el("div", { class: "kv" });
  const ssidIn = text("", 32, () => (wifiEdited = true), { placeholder: "NETWORK NAME", upper: false });
  const passIn = el("input", { class: "txt", type: "password", maxlength: 63, spellcheck: "false", autocomplete: "off" });
  passIn.addEventListener("input", () => (wifiEdited = true));
  passIn.addEventListener("keydown", (e) => e.stopPropagation());
  let wifiEdited = false; // being typed here: the knob's status doesn't overwrite the field
  const connectBtn = el("button", {
    class: "btn primary",
    onclick: () => {
      const n = device.net, ssid = ssidIn.value.trim();
      // A new network, or a password typed: send them. Otherwise the stored ones, back on.
      if (ssid && (ssid !== n?.ssid || passIn.value)) void device.setWifi(true, ssid, passIn.value);
      else void device.setWifi(true);
      passIn.value = "";
      wifiEdited = false;
    },
  }, "CONNECT");
  const offBtn = el("button", { class: "btn", onclick: () => void device.setWifi(false) }, "TURN OFF");
  const setup = [
    el("div", { class: "field" }, el("span", { class: "flabel" }, "NETWORK"), el("span", { class: "fctl" }, ssidIn)),
    el("div", { class: "field" }, el("span", { class: "flabel" }, "PASSWORD"), el("span", { class: "fctl" }, passIn)),
    el("div", { class: "actions" }, connectBtn, offBtn),
  ];
  // This app over WiFi (extensions v7): paired here over USB, then no cable needed.
  const pairState = el("div", { class: "kv" });
  const pairBtn = el("button", { class: "btn primary", onclick: () => void device.pairWifi(false) }, "PAIR THIS APP");
  let rekeyArmed = 0; // a new key unpairs every other companion: a second click within 3 s
  const rekeyBtn = el("button", {
    class: "btn",
    onclick: () => {
      if (Date.now() - rekeyArmed < 3000) {
        rekeyArmed = 0;
        void device.pairWifi(true);
      } else {
        rekeyArmed = Date.now();
        window.setTimeout(update, 3100);
      }
      update();
    },
  }, "NEW KEY");
  const forgetBtn = el("button", { class: "btn", onclick: () => device.forgetWifi() }, "FORGET");
  const pairHint = el("p", { class: "hint" });
  const pairRow = el("div", {}, pairState, el("div", { class: "actions" }, pairBtn, rekeyBtn, forgetBtn), pairHint);
  wifiSec.body.append(wifiState, ...setup, pairRow);

  const fwSec = section("FIRMWARE");
  const fw = el("div", { class: "tile" });
  fwSec.body.append(fw);

  root.append(bindSec.root, rotSec.root, bootSec.root, wifiSec.root, fwSec.root);

  function update() {
    const s = device.settings;
    if (!s) return;
    bind.update(s.host, !!bit(Set.HOST));
    bindHint.textContent = s.host === Host.PC ? "CMD SHORTCUTS ARE SENT AS CTRL." : "SHORTCUTS ARE SENT AS WRITTEN.";
    rot.update(s.rotation, !!bit(Set.ROTATION));
    boot.update(s.boot, !!bit(Set.BOOT));
    bootWarn.style.display = s.boot === Boot.SERIAL ? "" : "none";
    const n = device.net;
    wifiSec.root.style.display = (device.ext ?? 0) >= EXT_NET_VERSION ? "" : "none";
    if (n) {
      const state = !n.on ? "OFF" : NET_STATE[n.state] ?? "?";
      const more = n.state === NetState.CONNECTED ? `   ${n.ip}   ${n.rssi} DBM   ${n.host}.LOCAL${n.timeSet ? "   CLOCK SET" : ""}` : "";
      wifiState.replaceChildren(el("span", {}, "STATUS"), el("span", {}, `${state}${n.ssid && n.on ? ` (${n.ssid.toUpperCase()})` : ""}${more}`));
      if (!wifiEdited && document.activeElement !== ssidIn) ssidIn.value = n.ssid;
      passIn.placeholder = n.ssid ? "UNCHANGED" : "NONE = OPEN NETWORK";
      offBtn.disabled = !n.on;
    }
    const usb = device.kind === "tauri", overWifi = device.kind === "wifi";
    for (const x of setup) x.style.display = overWifi ? "none" : "";
    bootSec.root.style.display = overWifi ? "none" : ""; // the knob takes it over USB only
    pairRow.style.display = (device.ext ?? 0) >= EXT_WIFI_LINK_VERSION && device.kind !== "webhid" ? "" : "none";
    const p = device.paired;
    pairState.replaceChildren(el("span", {}, "THIS APP"), el("span", {}, p ? `PAIRED: ${(p.host || p.ip).toUpperCase()}${p.host ? ".LOCAL" : ""}` : "NOT PAIRED"));
    pairBtn.textContent = p ? "PAIR AGAIN" : "PAIR THIS APP";
    pairBtn.disabled = !usb || n?.state !== NetState.CONNECTED || !n.host;
    rekeyBtn.disabled = !usb;
    rekeyBtn.textContent = Date.now() - rekeyArmed < 3000 ? "SURE? UNPAIRS OTHERS" : "NEW KEY";
    forgetBtn.disabled = !p;
    pairHint.textContent = overWifi
      ? "CONNECTED OVER WIFI. THE NETWORK AND THE KEY CHANGE OVER USB."
      : "PAIRED, THIS APP REACHES THE KNOB OVER WIFI WHEN NO CABLE IS IN.";
    const h = device.hello;
    fw.replaceChildren(
      ...[
        ["VERSION", h?.version ?? "-"],
        ["BUILT", h?.date ?? "-"],
        ["PROTOCOL", h ? String(h.proto) : "-"],
        ["PROFILES", h ? String(h.profileCount) : "-"],
        ["EXTENSIONS", device.ext ? `V${device.ext} (LOOK)` : device.ext === 0 ? "NONE" : "-"],
        ["LINK", device.kind === "tauri" ? "USB (APP)" : device.kind === "wifi" ? "WIFI (APP)" : "WEBHID"],
      ].map(([k, v]) => el("div", { class: "kv" }, el("span", {}, k), el("span", {}, v.toUpperCase()))),
    );
  }
  return { root, update };
}
