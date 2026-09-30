// Two ways to reach the knob's vendor HID interface, one shape: the Tauri app's Rust pipe
// (src-tauri/src/lib.rs) and WebHID (the same page opened in Chrome / Edge).

import { REPORT_SIZE } from "./proto";

export interface Transport {
  readonly kind: "tauri" | "webhid";
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

// --- Tauri ---

interface DeviceInfo {
  path: string;
  product: string;
  serial: string;
}

class TauriTransport implements Transport {
  readonly kind = "tauri";
  readonly needsGesture = false;
  onReport: (r: Uint8Array) => void = () => {};
  onClosed: () => void = () => {};
  private listening = false;

  private async listen() {
    if (this.listening) return;
    this.listening = true;
    const { listen } = await import("@tauri-apps/api/event");
    await listen<number[]>("hid-report", (e) => this.onReport(new Uint8Array(e.payload)));
    await listen<string>("hid-closed", () => this.onClosed());
  }

  async tryConnect(): Promise<boolean> {
    await this.listen();
    const { invoke } = await import("@tauri-apps/api/core");
    const devices = await invoke<DeviceInfo[]>("hid_list");
    if (devices.length === 0) return false;
    await invoke("hid_open", { path: devices[0].path });
    return true;
  }

  async send(report: Uint8Array): Promise<void> {
    const { invoke } = await import("@tauri-apps/api/core");
    await invoke("hid_write", { data: Array.from(report) });
  }

  async close(): Promise<void> {
    const { invoke } = await import("@tauri-apps/api/core");
    await invoke("hid_close");
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
