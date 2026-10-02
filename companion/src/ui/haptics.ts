// HAPTICS: the haptic profiles (STEPS) and, for the chosen one, its feel and tuning. Each
// profile keeps its own values per feel, inside limits the knob sets, so a profile can't be
// tuned into instability. Live on the knob while you drag; SAVE (or F2 on the device) keeps
// them; RESET TO FACTORY (F2 held on the device) puts the chosen profile back.

import type { Device } from "../device";
import { Feel, HapticProfiles, Limits, Set, type SetId } from "../proto";
import { C, cards, curve, el, section, slider } from "./kit";
import { throttled } from "./throttle";

// A dial with one tick per step, as on the device; SMOOTH (no steps) is an unbroken ring.
export function stepsDial(cv: HTMLCanvasElement, steps: number, color: string) {
  const ctx = cv.getContext("2d")!;
  const c = cv.width / 2, r = c - 3;
  ctx.clearRect(0, 0, cv.width, cv.height);
  ctx.fillStyle = color;
  if (steps < 1) for (let a = 0; a < 360; a += 3) ctx.fillRect(Math.round(c + r * Math.cos((a * Math.PI) / 180)), Math.round(c + r * Math.sin((a * Math.PI) / 180)), 1, 1);
  for (let i = steps - 1; i >= 0; i--) {
    const a = (-90 + (i * 360) / steps) * (Math.PI / 180);
    const x = Math.round(c + r * Math.cos(a)), y = Math.round(c + r * Math.sin(a));
    if (steps > 24) ctx.fillRect(x, y, 1, 1);
    else ctx.fillRect(x - 1, y - 1, 2, 2);
  }
  ctx.fillStyle = C.amber;
  ctx.fillRect(c - 2, c - r - 2, 4, 4);
}

export function hapticsView(device: Device) {
  const root = el("div");
  const send = throttled((id: SetId, v: number) => device.set(id, v));
  const bit = (id: number) => ((device.settings?.dirty ?? 0) >> id) & 1;

  // STEPS: the haptic profiles.
  const stepsSec = section("STEPS", "HAPTIC PROFILE: EACH KEEPS ITS OWN FEEL AND TUNING");
  const dials: HTMLCanvasElement[] = [];
  const stepCards = cards<number>(
    HapticProfiles.map((p, i) => ({
      value: i,
      body: () => {
        const cv = el("canvas", { width: 28, height: 28 });
        cv.style.width = cv.style.height = "28px";
        dials[i] = cv;
        return [cv, el("span", { class: "big" }, p.name), el("span", { class: "sub" }, p.detents ? `${p.detents} PER TURN` : "NO STEPS")];
      },
    })),
    (v) => device.set(Set.HAPTIC_PROFILE, v),
    64,
  );
  const resetBtn = el("button", { class: "btn", onclick: () => device.resetHaptic() }, "RESET TO FACTORY");
  stepsSec.body.append(stepCards.root, el("div", { class: "actions" }, resetBtn));

  // FEEL
  const feelSec = section("FEEL", "HOW A STEP PUSHES BACK");
  const feels = [
    { value: Feel.SAW, name: "SAW", sub: "CRISP SNAP" },
    { value: Feel.SINE, name: "SINE", sub: "ROUND BUMP" },
    { value: Feel.VISCOSE, name: "VISCOSE", sub: "SMOOTH DRAG" },
  ];
  const curves: HTMLCanvasElement[] = [];
  const feelCards = cards<number>(
    feels.map((f) => ({
      value: f.value,
      body: () => {
        const cv = el("canvas", { width: 56, height: 16 });
        cv.style.width = "56px";
        cv.style.height = "16px";
        curves[f.value] = cv;
        return [cv, el("span", { class: "big" }, f.name), el("span", { class: "sub" }, f.sub)];
      },
    })),
    (v) => device.set(Set.FEEL, v),
    64,
  );
  feelSec.body.append(feelCards.root);

  // Sliders, named like the device (SNAP / DAMP / SHAPE / AMP / PITCH) with the engineering
  // name. Their ranges are the chosen profile's limits in its feel.
  const tuneSec = section("TUNE", "SCROLL OR DRAG");
  const snap = slider({ label: "SNAP", caption: "KP", ...Limits.kp, format: (v) => v.toFixed(2), onInput: (v) => send(Set.KP, v) });
  const damp = slider({ label: "DAMP", caption: "KD", ...Limits.kd, format: (v) => v.toFixed(3).replace(/^0/, ""), onInput: (v) => send(Set.KD, v) });
  const shape = slider({ label: "SHAPE", caption: "SAW ONLY", ...Limits.shape, format: (v) => `${v}%`, onInput: (v) => send(Set.SHAPE, v) });
  const amp = slider({ label: "AMP", caption: "CLICK VOLUME", ...Limits.amp, format: (v) => `${v}%`, onInput: (v) => send(Set.AMP, v) });
  const pitch = slider({ label: "PITCH", caption: "CLICK", ...Limits.pitch, format: (v) => `${v.toFixed(2)}X`, onInput: (v) => send(Set.PITCH, v) });
  tuneSec.body.append(snap.root, damp.root, shape.root, amp.root, pitch.root);

  root.append(stepsSec.root, feelSec.root, tuneSec.root);

  let drawnFeel = -1, drawnShape = -1, drawnProfile = -1;
  function update() {
    const s = device.settings;
    if (!s) return;
    stepCards.update(s.hapticProfile, !!bit(Set.HAPTIC_PROFILE));
    if (s.hapticProfile !== drawnProfile) {
      drawnProfile = s.hapticProfile;
      dials.forEach((cv, i) => stepsDial(cv, HapticProfiles[i].detents, i === s.hapticProfile ? C.white : C.grey));
    }
    feelCards.update(s.feel, !!bit(Set.FEEL), (f) => ((s.feels >> f) & 1) === 1);
    if (s.feel !== drawnFeel || s.shape !== drawnShape) {
      drawnFeel = s.feel;
      drawnShape = s.shape;
      curves.forEach((cv, i) => curve(cv, i, i === s.feel ? C.amber : C.grey));
    }
    snap.limits(s.kpMin, s.kpMax);
    snap.mute(s.feel === Feel.VISCOSE);
    snap.update(s.kp, !!bit(Set.KP));
    damp.limits(s.kdMin, s.kdMax);
    damp.update(s.kd, !!bit(Set.KD));
    shape.mute(s.feel !== Feel.SAW);
    shape.update(s.shape, !!bit(Set.SHAPE));
    amp.limits(0, s.ampMax);
    amp.update(s.amp, !!bit(Set.AMP));
    pitch.limits(s.pitchMin, s.pitchMax);
    pitch.update(s.pitch, !!bit(Set.PITCH));
  }
  return { root, update };
}
