// Making profiles: a new one from scratch, or a copy. Each is sent to the knob live (not
// saved), put in use, and opened in the editor.

import { blankProfile, ID_RE, type ProfileJson } from "./profile";
import { HidType, Set } from "./proto";
import { device, go, saveError } from "./store";

export const MAX_PROFILES = 16;

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

// The profile list reloads after a change; resolves once it shows what we're waiting for.
function waitFor(ok: () => boolean, ms = 3000): Promise<void> {
  return new Promise((resolve) => {
    const t0 = performance.now();
    const tick = () => (ok() || performance.now() - t0 > ms ? resolve() : window.setTimeout(tick, 50));
    tick();
  });
}

async function addAndEdit(make: () => Promise<ProfileJson>) {
  saveError.value = null;
  try {
    const p = await make();
    const r = await device.uploadProfile(p, false);
    if (device.settings?.hidType !== HidType.APP) await device.set(Set.HID_TYPE, HidType.APP);
    await device.set(Set.PROFILE, r.index);
    await waitFor(() => device.profiles[r.index]?.id === p.id);
    go({ page: "profile", id: p.id, tab: "general", input: "knob" });
  } catch (e) {
    saveError.value = e instanceof Error ? e.message : String(e);
  }
}

export function createProfile() {
  return addAndEdit(async () => blankProfile(freeId("NEW PROFILE"), "NEW PROFILE"));
}

export function duplicateProfile(index: number) {
  return addAndEdit(async () => {
    const p = await device.readProfile(index);
    const name = `${p.name.slice(0, 10)} COPY`;
    return { ...p, id: freeId(name), name };
  });
}
