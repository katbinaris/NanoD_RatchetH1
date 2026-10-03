// Command wheel: the rings (each a tab on the wheel, with a jump key) and their commands -- a
// shortcut, a search in the app, or a macro. Commands with an animated card or a value step
// keep them (shown as tags; not edited here yet).

import { useSignal } from "@preact/signals";
import { MAX, type Command, type ProfileJson, type Ring, type SlotName } from "../../profile";
import { href } from "../../store";
import { Box, Card, Confirm, KeyField, Num, Row, Seg, Text } from "../../ui/controls";
import { titleCase } from "../../ui/shell";
import type { Session } from "./session";

export function WheelTab(x: { s: Session; p: ProfileJson }) {
  const { s, p } = x;
  void s.rev.value;
  const sel = useSignal(0);
  const rings = p.rings ?? [];
  const opener = (Object.entries(p.slots ?? {}) as [SlotName, { kind?: string }][]).find(([, a]) => a?.kind === "commands")?.[0];
  const i = Math.min(sel.value, rings.length - 1);
  const t = () => s.touch();
  const addRing = () => {
    p.rings ??= [];
    p.rings.push({ name: "NEW RING", tab: "NEW", slot: "f1", cmds: [{ name: "NEW COMMAND" }] });
    sel.value = p.rings.length - 1;
    t();
  };
  return (
    <div class="split side">
      <Box title="Rings" note={`${MAX.rings} at most`} class="list-col">
        {rings.map((r, k) => (
          <Card left on={k === i} onClick={() => (sel.value = k)}>
            <span class="nm">{titleCase(r.name)}</span>
            <span class="sub">
              Tab {r.tab} · jump key {(r.slot ?? "—").toUpperCase()} · {r.cmds.length} command{r.cmds.length === 1 ? "" : "s"}
            </span>
          </Card>
        ))}
        <button class="btn ghost" disabled={rings.length >= MAX.rings} onClick={addRing}>
          + Ring
        </button>
        <span class="hint foot">
          {opener ? (
            <>
              Opened by <b>{opener.toUpperCase()}</b>: hold it, turn to pick, let go to run.
            </>
          ) : (
            <>
              No input opens it yet: set one to Wheel menu under <a href={href({ page: "profile", id: p.id, tab: "keys", input: "f1" })}>Knob &amp; keys</a>.
            </>
          )}
        </span>
      </Box>
      <div class="stack">
        {i >= 0 ? (
          <RingEditor
            key={i}
            p={p}
            r={rings[i]}
            touch={t}
            remove={() => {
              p.rings!.splice(i, 1);
              sel.value = Math.max(0, i - 1);
              t();
            }}
          />
        ) : (
          <Box>
            <span class="hint">No rings yet.</span>
          </Box>
        )}
        {rings.some((r) => r.cmds.some((c) => c.kind === "actions")) && <SearchBox p={p} touch={t} />}
      </div>
    </div>
  );
}

function RingEditor(x: { p: ProfileJson; r: Ring; touch: () => void; remove: () => void }) {
  const { r, touch } = x;
  const move = (j: number, d: number) => {
    const k = j + d;
    if (k < 0 || k >= r.cmds.length) return;
    [r.cmds[j], r.cmds[k]] = [r.cmds[k], r.cmds[j]];
    touch();
  };
  return (
    <Box>
      <div class="line">
        <label class="line" style={{ gap: "8px" }}>
          <span class="lab">Name</span>
          <Text value={r.name} max={MAX.label} width={150} on={(v) => ((r.name = v), touch())} />
        </label>
        <label class="line" style={{ gap: "8px" }}>
          <span class="lab">Tab</span>
          <Text value={r.tab} max={MAX.tab} width={80} on={(v) => ((r.tab = v), touch())} />
        </label>
        <span class="line" style={{ gap: "8px" }}>
          <span class="lab">Jump key</span>
          <Seg options={(["f1", "f2", "f3", "f4"] as SlotName[]).map((k) => ({ value: k, label: k.toUpperCase() }))} value={r.slot ?? "knob"} on={(v) => ((r.slot = v), touch())} />
        </span>
        <span class="spacer" />
        <Confirm label="Delete ring" class="danger sm" on={x.remove} />
      </div>
      <div class="list">
        <div class="li head cmdrow">
          <span>Command</span>
          <span>Runs</span>
          <span />
          <span />
        </div>
        {r.cmds.map((c, j) => (
          <CmdRow p={x.p} c={c} touch={touch} up={() => move(j, -1)} down={() => move(j, 1)} remove={() => (r.cmds.splice(j, 1), touch())} first={j === 0} last={j === r.cmds.length - 1} />
        ))}
      </div>
      <div class="line">
        <button class="btn sm" disabled={r.cmds.length >= MAX.cmds} onClick={() => (r.cmds.push({ name: "NEW COMMAND" }), touch())}>
          + Command
        </button>
        <span class="hint">{MAX.cmds} per ring</span>
      </div>
    </Box>
  );
}

function CmdRow(x: { p: ProfileJson; c: Command; touch: () => void; up: () => void; down: () => void; remove: () => void; first: boolean; last: boolean }) {
  const { c, touch } = x;
  const kind = c.kind ?? "keys";
  const macros = (x.p.macros ?? []).map((m) => m.name).filter(Boolean);
  return (
    <div class="li cmdrow">
      <Text value={c.name} max={MAX.label} class="wide" on={(v) => ((c.name = v), touch())} />
      <div class="line">
        <Seg
          options={[
            { value: "keys" as const, label: "Shortcut" },
            { value: "actions" as const, label: "Search" },
            { value: "macro" as const, label: "Macro" },
          ]}
          value={kind}
          on={(v) => ((c.kind = v), touch())}
        />
        {kind === "actions" ? (
          <Text value={c.phrase ?? ""} max={MAX.phrase} upper={false} width={190} placeholder="Text to search for" on={(v) => ((c.phrase = v), touch())} />
        ) : kind === "macro" ? (
          macros.length ? (
            <Seg options={macros.map((n) => ({ value: n, label: titleCase(n) }))} value={c.macro ?? ""} on={(v) => ((c.macro = v), touch())} />
          ) : (
            <span class="hint">No macros yet</span>
          )
        ) : (
          <KeyField value={c.key} on={(k) => ((c.key = k), touch())} />
        )}
        {c.scene && (
          <span class="tag" title="Has an animated card (kept as it is)">
            Card
          </span>
        )}
        {c.param && (
          <span class="tag" title="Sets a value afterwards (kept as it is)">
            Value
          </span>
        )}
      </div>
      <span class="updown">
        <button class="x" aria-label="Move up" disabled={x.first} onClick={x.up}>
          ↑
        </button>
        <button class="x" aria-label="Move down" disabled={x.last} onClick={x.down}>
          ↓
        </button>
      </span>
      <button class="x" aria-label="Remove" onClick={x.remove}>
        ×
      </button>
    </div>
  );
}

function SearchBox(x: { p: ProfileJson; touch: () => void }) {
  const sr = (x.p.search ??= {});
  return (
    <Box title="Command search" note="For Search commands: opens the app's search, types, runs the first result">
      <Row label="Opens with">
        <KeyField value={sr.open} on={(k) => ((sr.open = k), x.touch())} />
      </Row>
      <Row label="Wait to open" for="sr-open">
        <Num id="sr-open" value={(sr.open_wait ?? 0) * 10} min={0} max={2550} step={10} unit="ms" on={(v) => ((sr.open_wait = Math.round(v / 10)), x.touch())} />
      </Row>
      <Row label="Wait for results" for="sr-res">
        <Num id="sr-res" value={(sr.result_wait ?? 0) * 10} min={0} max={2550} step={10} unit="ms" on={(v) => ((sr.result_wait = Math.round(v / 10)), x.touch())} />
      </Row>
    </Box>
  );
}
