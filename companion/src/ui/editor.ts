// The profile editor: one app profile, read from the knob, edited here, and sent back live
// (a moment after each change) so the knob follows as you go. SAVE stores it on the knob;
// REVERT goes back to what's stored; RESET TO DEFAULT / DELETE removes the stored copy.
//
// Command cards (the animated illustrations) and parameter mode come along untouched: they
// show as CARD / VALUE badges and aren't edited here yet. Macros: ./macros.ts.

import { DeviceError, type Device } from "../device";
import { Op, ProfileFlag } from "../proto";
import {
  BUTTON_NAMES,
  FEELS,
  iconImage,
  imageToIcon,
  MAX,
  MEDIA_USAGES,
  Mod,
  problem,
  SLOTS,
  type Action,
  type Command,
  type Kind,
  type ProfileJson,
  type Ring,
  type SlotName,
} from "../profile";
import { bits, confirmBtn, field, keyInput, MOD_BITS, num, seg, text } from "./fields";
import { el, section } from "./kit";
import { macroPick, macrosSection } from "./macros";

const APPLY_MS = 350;

const SLOT_TITLE: Record<SlotName, string> = { knob: "KNOB", f1: "F1", f2: "F2", f3: "F3", f4: "F4" };
const SLOT_NOTE: Record<SlotName, string> = {
  knob: "TURNING, NO KEY HELD",
  f1: "HOLD F1 + TURN, OR TAP",
  f2: "HOLD F2 + TURN, OR TAP",
  f3: "HOLD F3 + TURN, OR TAP",
  f4: "HOLD F4 + TURN (HOLD STILL = MENU)",
};
const KIND_LABEL: Record<Kind, string> = { none: "OFF", wheel: "SCROLL", drag: "DRAG", keys: "KEYS", tap: "TAP", commands: "WHEEL MENU", media: "MEDIA" };
// What each input may do (app_profile.h): the knob only turns; F4 is also the menu key, so
// it has no press actions. MEDIA on a key fires on press, like TAP.
const KINDS_FOR: Record<SlotName, Kind[]> = {
  knob: ["none", "wheel", "drag", "keys", "media"],
  f1: ["none", "wheel", "drag", "keys", "tap", "commands", "media"],
  f2: ["none", "wheel", "drag", "keys", "tap", "commands", "media"],
  f3: ["none", "wheel", "drag", "keys", "tap", "commands", "media"],
  f4: ["none", "wheel", "drag", "keys"],
};
const FINE = Mod.SHIFT | Mod.ALT; // on a volume key: a quarter step on a Mac

export function profileEditor(device: Device, startIndex: number, onClose: () => void) {
  const root = el("div", { class: "editor" });
  let draft: ProfileJson | null = null;
  let id = device.profiles[startIndex]?.id ?? "";
  let timer = 0;
  let dirtyLocal = false; // changed here, not yet sent
  let busy = false;

  // --- header: back, the profile, its state, actions ---
  const icon = el("canvas", { width: 24, height: 24 });
  icon.style.width = icon.style.height = "24px";
  const title = el("span", { class: "ed-name" });
  const badge = el("span", { class: "ed-badge" });
  const statusLine = el("div", { class: "ed-status" });
  const saveBtn = el("button", { class: "btn primary", onclick: () => void save() }, "SAVE TO KNOB");
  const revertBtn = el("button", { class: "btn", onclick: () => void revert() }, "REVERT");
  const resetSlot = el("span");
  // Sticky: SAVE stays in reach while scrolling through the form.
  const head = el(
    "div",
    { class: "ed-head" },
    el(
      "div",
      { class: "ed-row" },
      el("button", { class: "btn", onclick: () => close() }, "< BACK"),
      icon,
      title,
      el("span", { class: "spacer" }),
      badge,
    ),
    el("div", { class: "ed-row" }, statusLine, el("span", { class: "spacer" }), revertBtn, saveBtn),
  );
  const body = el("div");
  root.append(head, body, el("div", { class: "ed-foot" }, resetSlot));

  const index = () => device.profiles.findIndex((p) => p?.id === id);
  const flags = () => device.profiles[index()]?.flags ?? 0;

  function status(msg: string, bad = false) {
    statusLine.textContent = msg;
    statusLine.classList.toggle("warn", bad);
  }

  // --- loading ---
  async function load() {
    busy = true;
    status("READING FROM THE KNOB...");
    try {
      const i = index();
      draft = await device.readProfile(i >= 0 ? i : startIndex);
      id = draft.id;
      dirtyLocal = false;
      status("");
      render();
    } catch (e) {
      status(msg(e), true);
    } finally {
      busy = false;
      update();
    }
  }

  // --- sending: every change, a moment later ---
  function touch() {
    dirtyLocal = true;
    update();
    window.clearTimeout(timer);
    timer = window.setTimeout(() => void apply(false), APPLY_MS);
  }

  async function apply(saveToo: boolean) {
    window.clearTimeout(timer);
    if (!draft) return;
    const bad = problem(draft);
    if (bad) {
      status(bad, true);
      return;
    }
    busy = true;
    update();
    status(saveToo ? "SAVING..." : "SENDING...");
    try {
      await device.uploadProfile(draft, saveToo);
      dirtyLocal = false;
      status(saveToo ? "SAVED ON THE KNOB" : "LIVE ON THE KNOB, NOT SAVED YET");
    } catch (e) {
      status(msg(e), true);
    } finally {
      busy = false;
      update();
    }
  }

  async function save() {
    if (dirtyLocal) return apply(true);
    await run(() => device.profileOp(index(), Op.SAVE), "SAVED ON THE KNOB");
  }

  async function revert() {
    window.clearTimeout(timer);
    dirtyLocal = false;
    const r = await run(() => device.profileOp(index(), Op.REVERT), "");
    if (r?.removed) close();
    else if (r) await load();
  }

  async function remove(custom: boolean) {
    window.clearTimeout(timer);
    dirtyLocal = false;
    const r = await run(() => device.profileOp(index(), Op.REMOVE), custom ? "" : "BACK TO THE BUILT-IN VERSION");
    if (r?.removed) close();
    else if (r) await load();
  }

  async function run<T>(job: () => Promise<T>, done: string): Promise<T | null> {
    busy = true;
    update();
    try {
      const r = await job();
      status(done);
      return r;
    } catch (e) {
      status(msg(e), true);
      return null;
    } finally {
      busy = false;
      update();
    }
  }

  function close() {
    window.clearTimeout(timer);
    if (dirtyLocal) void apply(false); // don't lose the last edit
    onClose();
  }

  // --- the form ---
  function render() {
    if (!draft) return;
    const p = draft;
    body.replaceChildren(identity(p), screen(p), slots(p), wheel(p), macrosSection(p, touch, render));
    drawIcon();
  }

  function drawIcon() {
    const img = draft && iconImage(draft, 24);
    const ctx = icon.getContext("2d")!;
    ctx.clearRect(0, 0, 24, 24);
    if (img) ctx.putImageData(img, 0, 0);
  }

  function identity(p: ProfileJson) {
    const s = section("PROFILE", p.id.toUpperCase());
    const big = el("canvas", { width: 48, height: 48, class: "ed-icon" });
    const small = el("canvas", { width: 24, height: 24, class: "ed-icon" });
    const paint = () => {
      for (const [c, size] of [
        [big, 48],
        [small, 24],
      ] as const) {
        const ctx = c.getContext("2d")!;
        ctx.fillStyle = "#000";
        ctx.fillRect(0, 0, size, size);
        const img = iconImage(p, size);
        if (img) ctx.putImageData(img, 0, 0);
      }
    };
    paint();
    const file = el("input", { type: "file", accept: "image/*" });
    file.style.display = "none";
    file.addEventListener("change", async () => {
      const f = file.files?.[0];
      if (!f) return;
      try {
        const bmp = await createImageBitmap(f);
        p.icon48 = imageToIcon(bmp, 48);
        p.icon24 = imageToIcon(bmp, 24);
        paint();
        drawIcon();
        touch();
      } catch {
        status("THAT FILE ISN'T AN IMAGE THIS APP CAN READ", true);
      }
      file.value = "";
    });
    s.body.append(
      field(
        "NAME",
        text(p.name, MAX.name, (v) => {
          p.name = v;
          title.textContent = v;
          touch();
        }),
      ),
      field("ICON", big, small, el("button", { type: "button", class: "btn", onclick: () => file.click() }, "IMPORT IMAGE"), file),
      field(
        "KEY LABELS",
        ...p.legend.map((l, i) =>
          text(
            l,
            MAX.legend,
            (v) => {
              p.legend[i] = v;
              touch();
            },
            { width: 52, placeholder: `F${i + 1}` },
          ),
        ),
      ),
    );
    return s.root;
  }

  function screen(p: ProfileJson) {
    const s = section("SCREEN", "THE MIDDLE OF THE MAIN SCREEN");
    const shapeRow = el("div");
    const drawShape = () => {
      shapeRow.replaceChildren();
      if (p.visual !== "shape") return;
      shapeRow.append(
        field(
          "SHAPE",
          seg<NonNullable<ProfileJson["shape"]>>(
            [
              { value: "cube", label: "CUBE" },
              { value: "pyramid", label: "PYRAMID" },
              { value: "octa", label: "OCTA" },
            ],
            p.shape ?? "cube",
            (v) => ((p.shape = v), touch()),
          ),
        ),
        field(
          "STYLE",
          seg<NonNullable<ProfileJson["shape_style"]>>(
            [
              { value: "face", label: "FACE" },
              { value: "grips", label: "GRIPS" },
              { value: "thick", label: "THICK" },
            ],
            p.shape_style ?? "face",
            (v) => ((p.shape_style = v), touch()),
          ),
        ),
      );
    };
    s.body.append(
      field(
        "SHOWS",
        seg<NonNullable<ProfileJson["visual"]>>(
          [
            { value: "label", label: "ACTION NAME" },
            { value: "shape", label: "3D SHAPE" },
          ],
          p.visual ?? "label",
          (v) => {
            p.visual = v;
            drawShape();
            touch();
          },
        ),
      ),
      shapeRow,
    );
    drawShape();
    return s.root;
  }

  function slots(p: ProfileJson) {
    const s = section("KNOB AND KEYS", "WHAT EACH ONE SENDS");
    p.slots ??= {};
    for (const name of SLOTS) s.body.append(slotEditor(p, name));
    return s.root;
  }

  function slotEditor(p: ProfileJson, name: SlotName) {
    const box = el("div", { class: "slot" });
    const fields = el("div", { class: "slot-fields" });
    const a = (): Action => (p.slots![name] ??= {});
    const draw = () => {
      fields.replaceChildren();
      const act = p.slots![name];
      const kind = act?.kind ?? "none";
      if (kind === "none") {
        if (name !== "knob" && name !== "f4") fields.append(...quickTap(a()));
        return;
      }
      const x = a();
      const feel = () =>
        field(
          "FEEL",
          seg(
            FEELS.map((f) => ({ value: f, label: f.toUpperCase() })),
            x.feel ?? "saw",
            (v) => ((x.feel = v), touch()),
          ),
        );
      const steps = () => field("STEPS", num(x.detents ?? 0, { min: 0, max: 36, step: 1, zero: "MENU", on: (v) => ((x.detents = v), touch()) }), el("span", { class: "hint" }, "PER TURN: USES THE NEAREST HAPTIC PROFILE. EMPTY = THE MODE'S"));
      const dir = () =>
        field(
          "DIRECTION",
          seg(
            [
              { value: 1, label: "NORMAL" },
              { value: -1, label: "REVERSED" },
            ],
            x.sign ?? 1,
            (v) => ((x.sign = v), touch()),
          ),
        );
      const mods = () => field("HOLDING", bits(MOD_BITS, x.modifier ?? 0, (v) => ((x.modifier = v), touch())));
      fields.append(field("NAME ON SCREEN", text(x.label ?? "", MAX.label, (v) => ((x.label = v), touch()), { placeholder: "OPTIONAL" })));
      if (kind === "wheel") fields.append(mods(), dir(), feel(), steps());
      if (kind === "drag")
        fields.append(
          field("BUTTON", bits(BUTTON_NAMES.map(([bit, label]) => ({ bit, label })), x.buttons ?? 0, (v) => ((x.buttons = v), touch()))),
          mods(),
          field(
            "AXIS",
            seg(
              [
                { value: false, label: "LEFT / RIGHT" },
                { value: true, label: "UP / DOWN" },
              ],
              !!x.axis_y,
              (v) => ((x.axis_y = v), touch()),
            ),
          ),
          field("SPEED", num(x.px_per_rad ?? 120, { min: 10, max: 2000, step: 10, width: 52, on: (v) => ((x.px_per_rad = v), touch()) }), el("span", { class: "hint" }, "PX PER RADIAN")),
          dir(),
          feel(),
        );
      if (kind === "keys")
        fields.append(
          field("TURN RIGHT", keyInput(x.cw, (k) => ((x.cw = k), touch()))),
          field("TURN LEFT", keyInput(x.ccw, (k) => ((x.ccw = k), touch()))),
          feel(),
          steps(),
        );
      if (kind === "tap") fields.append(...keyOrMacro("SENDS", x, "cw", "macro"));
      if (kind === "commands") fields.append(steps(), el("div", { class: "hint" }, "HOLD TO OPEN THE WHEEL, TURN TO PICK, LET GO TO RUN. RINGS BELOW."));
      if (kind === "media") {
        // A media key: the usage in the key's keycode; FINE puts SHIFT+OPT on both turns.
        const usage = (field_: "cw" | "ccw") =>
          seg(
            MEDIA_USAGES.map((m) => ({ value: m.usage as number, label: m.label })),
            x[field_]?.[1] ?? 0,
            (v) => ((x[field_] = [x[field_]?.[0] ?? 0, v]), touch()),
          );
        if (name === "knob") {
          const fine = !!((x.cw?.[0] ?? 0) & FINE);
          fields.append(
            field("TURN RIGHT", usage("cw")),
            field("TURN LEFT", usage("ccw")),
            field(
              "VOLUME STEP",
              seg(
                [
                  { value: true, label: "FINE" },
                  { value: false, label: "NORMAL" },
                ],
                fine,
                (v) => {
                  for (const f of ["cw", "ccw"] as const) if (x[f]) x[f] = [v ? FINE : 0, x[f]![1]];
                  touch();
                },
              ),
              el("span", { class: "hint" }, "FINE: A QUARTER STEP ON A MAC (SHIFT+OPT)"),
            ),
            feel(),
            steps(),
          );
        } else {
          fields.append(field("SENDS", usage("cw")), el("div", { class: "hint" }, "ON PRESS. GOES TO WHATEVER IS PLAYING."));
        }
      }
      if (name !== "knob" && name !== "f4" && kind !== "tap" && kind !== "media") fields.append(...quickTap(x));
    };
    const kindSeg = seg(
      KINDS_FOR[name].map((k) => ({ value: k, label: KIND_LABEL[k] })),
      p.slots![name]?.kind ?? "none",
      (k) => {
        const x = a();
        const was = x.kind;
        x.kind = k;
        // Sensible starting values for what's new.
        if ((k === "wheel" || k === "drag") && !x.sign) x.sign = 1;
        if (k === "drag") {
          x.buttons ||= 4;
          x.px_per_rad ||= 120;
          x.feel ??= "viscose";
        }
        if (k === "media" && was !== "media") {
          // As the MUSIC profile has them: the knob is a fine volume dial, a key plays / pauses.
          if (name === "knob") {
            x.cw = [FINE, 0xe9];
            x.ccw = [FINE, 0xea];
          } else {
            x.cw = [0, 0xcd];
          }
          x.tap = x.tap_macro = undefined;
        }
        if (k === "commands") {
          p.rings = p.rings?.length ? p.rings : [{ name: "COMMANDS", tab: "CMDS", slot: name, cmds: [{ name: "UNDO", key: [8, 29] }] }];
          render(); // the wheel section appears
        } else {
          draw();
        }
        touch();
      },
    );
    box.append(el("div", { class: "slot-head" }, el("span", { class: "slot-name" }, SLOT_TITLE[name]), el("span", { class: "hint" }, SLOT_NOTE[name])), el("div", { class: "slot-kind" }, kindSeg), fields);
    draw();
    return box;
  }

  // A key or one of the profile's macros: `keyField` / `macroField` of `x` (one is cleared
  // when the other is chosen).
  function keyOrMacro(label: string, x: Action, keyField: "cw" | "tap", macroField: "macro" | "tap_macro", hint?: string) {
    const p = draft!;
    const target = el("span", { class: "fctl" });
    const mode = () => (x[macroField] ? "macro" : "key");
    const draw = (m: "key" | "macro") =>
      target.replaceChildren(
        m === "macro"
          ? macroPick(p, x[macroField], (n) => ((x[macroField] = n), (x[keyField] = undefined), touch()))
          : keyInput(x[keyField], (k) => ((x[keyField] = k), (x[macroField] = undefined), touch())),
      );
    draw(mode());
    return [
      field(
        label,
        seg<"key" | "macro">(
          [
            { value: "key", label: "KEY" },
            { value: "macro", label: "MACRO" },
          ],
          mode(),
          (m) => {
            if (m === "key") x[macroField] = undefined;
            draw(m);
            touch();
          },
        ),
        hint ? el("span", { class: "hint" }, hint) : null,
      ),
      field("", target),
    ];
  }

  function quickTap(x: Action) {
    return keyOrMacro("QUICK TAP", x, "tap", "tap_macro", "PRESSED AND LET GO WITHOUT TURNING");
  }

  function wheel(p: ProfileJson) {
    const used = Object.values(p.slots ?? {}).some((a) => a?.kind === "commands");
    const s = section("COMMAND WHEEL", used ? `${p.rings?.length ?? 0} RINGS` : "NOT USED");
    if (!used && !(p.rings && p.rings.length)) {
      s.body.append(el("div", { class: "hint" }, "SET A KEY TO WHEEL MENU TO USE ONE."));
      return s.root;
    }
    p.rings ??= [];
    p.rings.forEach((r, i) => s.body.append(ringEditor(p, r, i)));
    const addRing = el("button", { type: "button", class: "btn", disabled: p.rings.length >= MAX.rings }, "+ RING");
    addRing.addEventListener("click", () => {
      p.rings!.push({ name: "NEW RING", tab: "NEW", slot: "f1", cmds: [{ name: "NEW COMMAND" }] });
      render();
      touch();
    });
    s.body.append(addRing);
    const usesSearch = p.rings.some((r) => r.cmds.some((c) => c.kind === "actions"));
    if (usesSearch) {
      p.search ??= {};
      const sr = p.search;
      s.body.append(
        el("div", { class: "subhead" }, "COMMAND SEARCH"),
        field("OPENS WITH", keyInput(sr.open, (k) => ((sr.open = k), touch()))),
        field(
          "WAIT",
          num(sr.open_wait ?? 0, { min: 0, max: 255, step: 1, on: (v) => ((sr.open_wait = v), touch()) }),
          el("span", { class: "hint" }, "X10MS TO OPEN,"),
          num(sr.result_wait ?? 0, { min: 0, max: 255, step: 1, on: (v) => ((sr.result_wait = v), touch()) }),
          el("span", { class: "hint" }, "X10MS FOR RESULTS"),
        ),
      );
    }
    return s.root;
  }

  function ringEditor(p: ProfileJson, r: Ring, i: number) {
    const box = el("div", { class: "ring" });
    const del = confirmBtn("DELETE RING", () => {
      p.rings!.splice(i, 1);
      render();
      touch();
    });
    box.append(
      el(
        "div",
        { class: "ring-head" },
        text(r.name, MAX.label, (v) => ((r.name = v), touch()), { width: 120 }),
        el("span", { class: "hint" }, "TAB"),
        text(r.tab, MAX.tab, (v) => ((r.tab = v), touch()), { width: 52 }),
        el("span", { class: "hint" }, "JUMP KEY"),
        seg(
          (["f1", "f2", "f3", "f4"] as SlotName[]).map((k) => ({ value: k, label: k.toUpperCase() })),
          r.slot ?? "knob",
          (v) => ((r.slot = v), touch()),
        ),
        el("span", { class: "spacer" }),
        del,
      ),
    );
    r.cmds.forEach((c, j) => box.append(cmdRow(r, c, j)));
    const add = el("button", { type: "button", class: "btn", disabled: r.cmds.length >= MAX.cmds }, "+ COMMAND");
    add.addEventListener("click", () => {
      r.cmds.push({ name: "NEW COMMAND" });
      render();
      touch();
    });
    box.append(add);
    return box;
  }

  function cmdRow(r: Ring, c: Command, j: number) {
    const target = el("span");
    const drawTarget = () =>
      target.replaceChildren(
        c.kind === "actions"
          ? text(c.phrase ?? "", MAX.phrase, (v) => ((c.phrase = v), touch()), { upper: false, width: 150, placeholder: "TEXT TO SEARCH FOR" })
          : c.kind === "macro"
            ? macroPick(draft!, c.macro, (n) => ((c.macro = n), touch()))
            : keyInput(c.key, (k) => ((c.key = k), touch())),
      );
    drawTarget();
    const move = (d: number) => {
      const k = j + d;
      if (k < 0 || k >= r.cmds.length) return;
      [r.cmds[j], r.cmds[k]] = [r.cmds[k], r.cmds[j]];
      render();
      touch();
    };
    return el(
      "div",
      { class: "cmd" },
      text(c.name, MAX.label, (v) => ((c.name = v), touch()), { width: 120 }),
      seg<NonNullable<Command["kind"]>>(
        [
          { value: "keys", label: "SHORTCUT" },
          { value: "actions", label: "SEARCH" },
          { value: "macro", label: "MACRO" },
        ],
        c.kind ?? "keys",
        (v) => {
          c.kind = v;
          drawTarget();
          render(); // the search settings may appear
          touch();
        },
      ),
      target,
      c.scene ? el("span", { class: "tag", title: "HAS AN ANIMATED CARD (KEPT AS IS)" }, "CARD") : null,
      c.param ? el("span", { class: "tag", title: "SETS A VALUE AFTERWARDS (KEPT AS IS)" }, "VALUE") : null,
      el("span", { class: "spacer" }),
      el("button", { type: "button", class: "mini", onclick: () => move(-1) }, "UP"),
      el("button", { type: "button", class: "mini", onclick: () => move(1) }, "DN"),
      el(
        "button",
        {
          type: "button",
          class: "mini",
          onclick: () => {
            r.cmds.splice(j, 1);
            render();
            touch();
          },
        },
        "X",
      ),
    );
  }

  // --- header state ---
  let resetFor = "";
  function update() {
    const f = flags();
    const live = (f & ProfileFlag.LIVE) !== 0;
    const unsaved = live || dirtyLocal;
    title.textContent = draft?.name ?? device.profiles[startIndex]?.name ?? "";
    badge.textContent = f & ProfileFlag.BUILTIN ? (f & ProfileFlag.STORED ? "BUILT-IN, CHANGED" : "BUILT-IN") : f & ProfileFlag.STORED ? "YOURS" : "NEW";
    badge.classList.toggle("warn", unsaved);
    if (unsaved) badge.textContent += "  NOT SAVED";
    saveBtn.disabled = busy || !unsaved;
    saveBtn.classList.toggle("fill", unsaved);
    revertBtn.disabled = busy || !unsaved;
    // RESET TO DEFAULT for a changed built-in, DELETE for one of the user's own.
    const which = f & ProfileFlag.BUILTIN ? (f & (ProfileFlag.STORED | ProfileFlag.LIVE) ? "reset" : "") : "delete";
    if (which !== resetFor) {
      resetFor = which;
      resetSlot.replaceChildren(
        which === "reset" ? confirmBtn("RESET TO DEFAULT", () => void remove(false)) : which === "delete" ? confirmBtn("DELETE PROFILE", () => void remove(true)) : "",
      );
    }
  }

  void load();
  return { root, update };
}

function msg(e: unknown): string {
  return e instanceof DeviceError ? e.message : String(e).toUpperCase();
}
