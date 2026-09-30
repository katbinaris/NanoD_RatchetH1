// SYS INFO: the device's SYS INFO pages as four tiles, with a minute of history. Power is an
// estimate (the board has no current sense) -- labelled as such, like on the device.

import type { Device } from "../device";
import { C, el, section, spark } from "./kit";

const USB_SOURCE = ["READING", "PD CHIP NOT FOUND", "USB", "USB-C", "USB-C", "USB PD"];

const amps = (ma: number) => `${(ma / 1000).toFixed(2)}A`;
const us = (v: number) => `${v.toFixed(1)}US`;

export function sysinfoView(device: Device) {
  const root = el("div");
  const sec = section(
    "SYS INFO",
    el("button", { class: "btn", onclick: () => device.resetPeaks() }, "RESET PEAKS"),
  );

  function tile(title: string, note: string) {
    const big = el("div", { class: "big" });
    const sub = el("div", { class: "sub" });
    const gauge = el("div", { class: "gauge" });
    const rows = el("div");
    const chart = el("canvas", { class: "spark" });
    const t = el("div", { class: "tile" }, el("div", { class: "head" }, el("span", {}, title), el("span", {}, note)), big, sub, gauge, rows, chart);
    return { t, big, sub, gauge, rows, chart };
  }
  const power = tile("POWER", "ESTIMATED");
  const heat = tile("HEAT", "CHIP");
  const cpu = tile("CPU", "LOAD");
  const sys = tile("SYSTEM", "");
  sec.body.append(el("div", { class: "stack" }, power.t, heat.t, cpu.t, sys.t));
  root.append(sec.root);

  const kv = (k: string, v: string, amber = false) => el("div", { class: "kv" }, el("span", {}, k), el("span", { class: amber ? "amber" : "" }, v));
  const gaugeFill = (g: HTMLElement, frac: number, peak?: number, color = C.white) => {
    const f = Math.max(0, Math.min(1, frac));
    const bar = el("i");
    bar.style.width = `${f * 100}%`;
    bar.style.background = color;
    g.replaceChildren(bar);
    if (peak !== undefined) {
      const p = el("b");
      p.style.left = `${Math.min(1, peak) * 100}%`;
      g.append(p);
    }
  };

  function update() {
    const a = device.sysA, b = device.sysB, h = device.history;
    if (!a || !b) return;

    // POWER
    const have = a.usbMa > 0 && a.usbSource >= 2;
    power.big.textContent = amps(a.totalMa);
    power.sub.textContent = have ? `OF ${amps(a.usbMa)} ${(a.usbMv / 1000).toFixed(0)}V ${USB_SOURCE[a.usbSource] ?? ""}` : USB_SOURCE[a.usbSource] ?? "";
    gaugeFill(power.gauge, have ? a.totalMa / a.usbMa : 0, have ? a.totalPeakMa / a.usbMa : undefined, have && a.totalMa > 0.8 * a.usbMa ? C.amber : C.white);
    power.rows.replaceChildren(kv("MOTOR", amps(a.motorMa)), kv("LEDS", amps(a.ledMa)), kv("BOARD", amps(a.boardMa)), kv("PEAK", amps(a.totalPeakMa), true));
    spark(power.chart, h.totalMa, have ? a.usbMa : Math.max(1000, ...h.totalMa));

    // HEAT
    heat.big.textContent = a.chipOk ? `${a.chipC.toFixed(1)}C` : "--";
    heat.sub.textContent = a.chipOk ? `PEAK ${a.chipPeakC.toFixed(1)}C` : "NO SENSOR";
    gaugeFill(heat.gauge, (a.chipC - 20) / 60, (a.chipPeakC - 20) / 60, a.chipC > 60 ? C.amber : C.white);
    heat.rows.replaceChildren(kv("COIL", amps(a.coilMa)), kv("COIL PEAK", amps(a.coilPeakMa), true), kv("COIL HEAT", `${a.copperW.toFixed(2)}W`));
    spark(heat.chart, h.chipC, 80);

    // CPU
    cpu.big.textContent = `${b.load[0]}%  ${b.load[1]}%`;
    cpu.sub.textContent = "CORE 0 (CONTROL LOOP)  CORE 1 (EVERYTHING ELSE)";
    gaugeFill(cpu.gauge, b.load[0] / 100, b.loadPeak[0] / 100);
    cpu.rows.replaceChildren(
      kv("LOOP", `${b.loopKhz.toFixed(2)}KHZ`),
      kv("WORK", us(b.workAvgUs)),
      kv("WORK MAX", us(b.workMaxUs), true),
      kv("JITTER", us(b.jitterUs), true),
      kv("SPIKES", `${b.spikesPerS.toFixed(0)}/S`, b.spikesPerS >= 1),
      kv("MISSED", String(b.missed), b.missed > 0),
    );
    spark(cpu.chart, h.load0, 100, C.grey);

    // SYSTEM
    const s = b.uptimeS;
    sys.big.textContent = `${Math.floor(s / 3600)}:${String(Math.floor(s / 60) % 60).padStart(2, "0")}:${String(s % 60).padStart(2, "0")}`;
    sys.sub.textContent = "UPTIME";
    gaugeFill(sys.gauge, 1 - b.heapFree / Math.max(b.heapFree, 320 * 1024));
    sys.rows.replaceChildren(
      kv("RAM FREE", `${Math.round(b.heapFree / 1024)}K`),
      kv("RAM LOW", `${Math.round(b.heapMin / 1024)}K`),
      kv("HID DROPS", String(b.hidDrops), b.hidDrops > 0),
      kv("AUDIO GAPS", String(b.audioGaps), b.audioGaps > 0),
      kv("SENSOR CRC ERR", String(b.sensorCrcErrors), b.sensorCrcErrors > 0),
    );
    spark(sys.chart, h.load1, 100, C.grey);
  }
  return { root, update };
}
