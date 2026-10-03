// Ways to reach the knob, one shape: the Tauri app's Rust pipe (src-tauri/src/lib.rs) -- USB, or
// WiFi once this app is paired -- and WebHID (the same page opened in Chrome / Edge).

import { REPORT_SIZE } from "./proto";

export interface Transport {
  readonly kind: "tauri" | "wifi" | "webhid";
  // True when a user gesture is needed before the first connect (WebHID's device picker).
  readonly needsGesture: boolean;
  // Connect to a knob if one is available (no prompt). Resolves false when none is.
  tryConnect(): Promise<boolean>;
  // WebHID only: show the browser's device picker (call from a click).
  request?(): Promise<boolean>;
  send(report: Uint8Array): Promise<void>;
  close(): Promise<void>;
  onReport: (report: Uint8Array) => void;
  onClosed: () => void;
}

export function isTauri(): boolean {
  return "__TAURI_INTERNALS__" in window;
}

export async function createTransport(): Promise<Transport | null> {
  if (new URLSearchParams(location.search).has("demo")) {
    const { MockTransport } = await import("./mock");
    return new MockTransport();
  }
  if (isTauri()) return new TauriTransport();
  if ("hid" in navigator) return new WebHidTransport();
  return null;
}

// --- Tauri: USB, else WiFi ---

interface DeviceInfo {
  path: string;
  product: string;
  serial: string;
}

// The knob this app paired with over USB (Device.pairWifi): where it was, and its key (hex).
// Plain local storage: whatever runs as this user can open the knob's HID interface and read the
// key from the knob itself anyway.
export interface Pairing {
  host: string; // mDNS name, without ".local"
  ip: string; // the last address seen, if the name doesn't resolve
  port: number;
  key: string;
}
const PAIRING = "quadra.wifi";

export function loadPairing(): Pairing | null {
  try {
    const p = JSON.parse(localStorage.getItem(PAIRING) ?? "null");
    return p && typeof p.key === "string" && /^[0-9a-f]{64}$/.test(p.key) && p.port > 0 ? (p as Pairing) : null;
  } catch {
    return null;
  }
}

export function savePairing(p: Pairing | null) {
  try {
    if (p) localStorage.setItem(PAIRING, JSON.stringify(p));
    else localStorage.removeItem(PAIRING);
  } catch {
    // no storage: paired for this session only
  }
}

const USB_WATCH_MS = 3000;

class TauriTransport implements Transport {
  kind: "tauri" | "wifi" = "tauri";
  readonly needsGesture = false;
  onReport: (r: Uint8Array) => void = () => {};
  onClosed: () => void = () => {};
  private listening = false;
  private usbWatch: number | undefined;

  private async listen() {
    if (this.listening) return;
    this.listening = true;
    const { listen } = await import("@tauri-apps/api/event");
    await listen<number[]>("hid-report", (e) => this.kind === "tauri" && this.onReport(new Uint8Array(e.payload)));
    await listen<string>("hid-closed", () => this.kind === "tauri" && this.onClosed());
    await listen<number[]>("net-report", (e) => this.kind === "wifi" && this.onReport(new Uint8Array(e.payload)));
    await listen<string>("net-closed", () => {
      if (this.kind !== "wifi") return;
      window.clearInterval(this.usbWatch);
      this.onClosed();
    });
  }

  // USB when a knob is plugged in; otherwise the paired one over WiFi. Throws why WiFi failed.
  async tryConnect(): Promise<boolean> {
    await this.listen();
    const { invoke } = await import("@tauri-apps/api/core");
    const devices = await invoke<DeviceInfo[]>("hid_list");
    if (devices.length > 0) {
      this.kind = "tauri";
      await invoke("hid_open", { path: devices[0].path });
      return true;
    }
    const p = loadPairing();
    if (!p) return false;
    const key = Array.from({ length: 32 }, (_, i) => parseInt(p.key.slice(i * 2, i * 2 + 2), 16));
    this.kind = "wifi";
    await invoke("net_open", { hosts: [p.host && `${p.host}.local`, p.ip].filter(Boolean), port: p.port, key });
    // A cable plugged in meanwhile: over to USB (faster, and it can change the WiFi setup).
    window.clearInterval(this.usbWatch);
    this.usbWatch = window.setInterval(async () => {
      if ((await invoke<DeviceInfo[]>("hid_list")).length === 0 || this.kind !== "wifi") return;
      await this.close();
      this.onClosed();
    }, USB_WATCH_MS);
    return true;
  }

  async send(report: Uint8Array): Promise<void> {
    const { invoke } = await import("@tauri-apps/api/core");
    await invoke(this.kind === "wifi" ? "net_write" : "hid_write", { data: Array.from(report) });
  }

  async close(): Promise<void> {
    window.clearInterval(this.usbWatch);
    const { invoke } = await import("@tauri-apps/api/core");
    await invoke(this.kind === "wifi" ? "net_close" : "hid_close");
  }
}

// --- WebHID ---

const FILTER = { vendorId: 0x303a, usagePage: 0xff00, usage: 0x01 };

// The vendor collection of a Quadra (WebHID lists one HIDDevice per top-level collection).
function isQuadraVendor(d: HIDDevice): boolean {
  return d.vendorId === FILTER.vendorId && d.collections.some((c) => c.usagePage === FILTER.usagePage && c.usage === FILTER.usage);
}

class WebHidTransport implements Transport {
  readonly kind = "webhid";
  readonly needsGesture = true;
  onReport: (r: Uint8Array) => void = () => {};
  onClosed: () => void = () => {};
  private device: HIDDevice | null = null;

  constructor() {
    navigator.hid.addEventListener("disconnect", (e) => {
      if (e.device === this.device) {
        this.device = null;
        this.onClosed();
      }
    });
  }

  private async open(d: HIDDevice): Promise<boolean> {
    if (!d.opened) await d.open();
    d.addEventListener("inputreport", (e) => {
      this.onReport(new Uint8Array(e.data.buffer, e.data.byteOffset, e.data.byteLength));
    });
    this.device = d;
    return true;
  }

  // Devices this site was already allowed to use connect without the picker.
  async tryConnect(): Promise<boolean> {
    const d = (await navigator.hid.getDevices()).find(isQuadraVendor);
    return d ? this.open(d) : false;
  }

  async request(): Promise<boolean> {
    const picked = await navigator.hid.requestDevice({ filters: [FILTER] });
    const d = picked.find(isQuadraVendor);
    return d ? this.open(d) : false;
  }

  async send(report: Uint8Array): Promise<void> {
    if (!this.device) throw new Error("not connected");
    const out = new Uint8Array(REPORT_SIZE);
    out.set(report.subarray(0, REPORT_SIZE));
    await this.device.sendReport(0, out);
  }

  async close(): Promise<void> {
    const d = this.device;
    this.device = null;
    if (d?.opened) await d.close();
  }
}
