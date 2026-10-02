// Quadra companion: the knob's settings, live state and SYS INFO, in the device's own pixel
// language. The same page runs in the Tauri app (Rust HID pipe) and in Chrome / Edge (WebHID).

import { Device } from "./device";
import { createTransport, isTauri } from "./transport";
import { el, setSawShape } from "./ui/kit";
import { deviceView as deviceRender } from "./ui/deviceView";
import { hapticsView } from "./ui/haptics";
import { profilesView } from "./ui/profiles";
import { deviceView } from "./ui/devicePanel";
import { sysinfoView } from "./ui/sysinfo";

const device = new Device(await createTransport());

// --- header ---

const statusText = el("span");
const status = el("div", { class: "status" }, el("i", { class: "dot" }), statusText);
const revertBtn = el("button", { class: "btn", onclick: () => device.revert() }, "REVERT");
const saveBtn = el("button", { class: "btn primary", onclick: () => device.save() }, "SAVE");
const top = el(
  "header",
  { class: isTauri() ? "top" : "top web", "data-tauri-drag-region": true },
  el("div", { class: "brand", "data-tauri-drag-region": true }, "QUADRA"),
  status,
  el("div", { class: "spacer", "data-tauri-drag-region": true }),
  revertBtn,
  saveBtn,
);

// --- body: the glass | tabs + panel ---

const caption = el("div", { class: "glass-caption" });
const render3d = deviceRender(device);
const glassCol = el("div", { class: "glass-col" }, render3d.root, caption);
// The device fills its column (less padding and the caption), whatever the window size.
new ResizeObserver(() => render3d.fit(glassCol.clientWidth - 24, glassCol.clientHeight - 48)).observe(glassCol);

const views = {
  HAPTICS: hapticsView(device),
  PROFILES: profilesView(device),
  DEVICE: deviceView(device),
  "SYS INFO": sysinfoView(device),
};
type Tab = keyof typeof views;
// ?tab=PROFILES etc. opens on that tab (screenshots, demo links).
const tabParam = new URLSearchParams(location.search).get("tab")?.toUpperCase().replace("_", " ");
let tab: Tab = tabParam && tabParam in views ? (tabParam as Tab) : "HAPTICS";

const tabsBar = el("nav", { class: "tabs" });
const tabButtons = (Object.keys(views) as Tab[]).map((name) => {
  const b = el("button", { class: "tab", onclick: () => show(name) }, name);
  tabsBar.append(b);
  return { name, b };
});
const panel = el("div", { class: "panel" });
const panelCol = el("div", { class: "panel-col" }, tabsBar, panel);

document.getElementById("app")!.append(top, el("main", { class: "main" }, glassCol, panelCol));

function show(name: Tab) {
  tab = name;
  render();
}

// Not connected: what to do about it, in place of the panel.
function emptyState(): HTMLElement {
  if (device.status === "unsupported") {
    return el("div", { class: "empty" }, el("div", { class: "big" }, "NO USB ACCESS"), "OPEN THIS PAGE IN CHROME OR EDGE, OR USE THE QUADRA APP.");
  }
  if (device.status === "needs-permission") {
    return el(
      "div",
      { class: "empty" },
      el("div", { class: "big" }, "CONNECT YOUR QUADRA"),
      "THE BROWSER ASKS ONCE WHICH DEVICE THIS PAGE MAY USE.",
      el("button", { class: "btn primary", onclick: () => device.requestAccess() }, "CONNECT"),
    );
  }
  return el(
    "div",
    { class: "empty" },
    el("div", { class: "big" }, "LOOKING FOR QUADRA"),
    "PLUG IT IN. IT HAS TO BE IN HID BOOT MODE (THE NORMAL ONE).",
  );
}

let shownTab: Tab | null = null;
let shownConnected = false;

function render() {
  const connected = device.status === "connected" && device.settings !== null;

  statusText.textContent = connected
    ? `CONNECTED  ${device.hello?.version ?? ""}`.trim()
    : device.status === "needs-permission"
      ? "NOT CONNECTED"
      : device.status === "unsupported"
        ? "NO USB ACCESS"
        : "SEARCHING...";
  status.classList.toggle("on", connected);

  setSawShape(connected ? (device.settings!.shape ?? 0) / 100 : 0);
  const dirty = connected ? device.settings!.dirty : 0;
  const n = popcount(dirty);
  saveBtn.disabled = !connected || n === 0;
  revertBtn.disabled = !connected || n === 0;
  saveBtn.textContent = n > 0 ? `SAVE ${n}` : "SAVED";
  saveBtn.classList.toggle("fill", n > 0);
  saveBtn.classList.toggle("blink", n > 0);

  const st = device.state;
  caption.textContent = connected && st ? `DETENT ${st.detent}   CLICKS ${st.clicks}` : "";

  for (const { name, b } of tabButtons) b.classList.toggle("on", name === tab);

  if (!connected) {
    if (shownConnected || shownTab !== null || panel.childElementCount === 0) {
      panel.replaceChildren(emptyState());
      shownTab = null;
    }
    shownConnected = false;
    return;
  }
  if (!shownConnected || shownTab !== tab) {
    panel.replaceChildren(views[tab].root);
    panel.scrollTop = 0;
    shownTab = tab;
  }
  shownConnected = true;
  views[tab].update();
}

function popcount(v: number) {
  let n = 0;
  for (; v; v &= v - 1) n++;
  return n;
}

// The device notifies ~30x a second while streaming; one render per animation frame at most.
let queued = false;
device.subscribe(() => {
  if (queued) return;
  queued = true;
  requestAnimationFrame(() => {
    queued = false;
    render();
  });
});
render();
