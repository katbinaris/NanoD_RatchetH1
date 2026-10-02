#!/usr/bin/env python3
"""quadra_agentd -- AI-agent approvals and attention requests on the Quadra knob.

Hooks in Claude Code, Codex and Cursor (quadra_hook.py) talk to this daemon over a Unix socket
(~/.quadra/agentd.sock, owner-only). It shows each request on the knob (vendor HID,
src/ext_proto.h EXT_CMD_NOTIFY) and, for an approval, waits for the knob's answer:
hold F1 = allow, F3 = deny, F2 / F4 = answer in the app.

Nothing here approves on its own. Without a knob, after `approval_wait_s`, or when the hook
goes away, the answer is "no decision" and the agent shows its own prompt as usual. A request
that timed out stays on the knob as "APPROVE IN APP" until the session moves on.

Config: ~/.quadra/config.json (see DEFAULTS). Log: stdout (the LaunchAgent sends it to
~/Library/Logs/quadra-agentd.log).
"""
import asyncio
import json
import os
import queue
import threading
import time

import hid


def share_hid():
    """macOS hidapi opens devices exclusively by default, which would lock the daemon, this CLI
    and the companion app out of each other. Ask for shared opens (no-op elsewhere)."""
    try:
        import ctypes
        ctypes.CDLL(hid.__file__).hid_darwin_set_open_exclusive(0)
    except (OSError, AttributeError):
        pass


share_hid()

HOME = os.environ.get("QUADRA_HOME") or os.path.expanduser("~/.quadra")
SOCK = os.path.join(HOME, "agentd.sock")
CONFIG = os.path.join(HOME, "config.json")
DEFAULTS = {
    "approval_wait_s": 30,  # how long an approval waits for the knob before the app asks
    "your_turn": True,      # blink when an agent finishes its turn and waits for you
    "turn_delay_s": 4,      # ...unless you're already typing back within this
    "run_delay_s": 1.5,     # Cursor: only show "RUN?" if the command is still waiting by then
    "info_ttl_s": 1800,     # attention items clear themselves after this
    # Each agent's colour on the knob (ring, badge); "#000000" = the knob's own default.
    "colors": {"claude": "#E8825F", "codex": "#4F9DFF", "cursor": "#B98CFF"},
    # Which items tap the knob gently every few seconds: approvals, "needs input", an approval
    # that moved to the app, "your turn", Cursor's "run?".
    "nudge": {"ask": True, "attention": True, "inapp": True, "turn": False, "run": False},
}

# --- device (src/ext_proto.h, src/notify.h) ---
REPORT = 64
VENDOR_USAGE_PAGE, PRODUCT = 0xFF00, "Quadra"
EXT_CMD_NOTIFY, EXT_TAG_NOTIFY, EXT_NOTIFY_NUDGE = 0x25, 0xC3, 0x80
POST, CLEAR, CLEAR_ALL = 1, 2, 3
ASK, INFO = 0, 1
SOURCE_ID = {"claude": 0, "codex": 1, "cursor": 2}
ALLOW, DENY, LATER, DISMISS = 1, 2, 3, 4
SHOWN_MAX = 4  # NOTIFY_MAX


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


def load_config():
    cfg = json.loads(json.dumps(DEFAULTS))
    try:
        user = json.load(open(CONFIG))
    except (OSError, ValueError):
        user = {}
    for k, v in user.items():
        if isinstance(v, dict) and isinstance(cfg.get(k), dict):
            cfg[k].update(v)
        else:
            cfg[k] = v
    return cfg


def rgb(spec) -> bytes:
    try:
        v = int(str(spec).lstrip("#"), 16)
        return bytes([(v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF])
    except ValueError:
        return b"\0\0\0"


def cstr(s: str, n: int) -> bytes:
    return s.encode("ascii", "replace")[:n].ljust(n, b"\0")


class Device(threading.Thread):
    """Owns the HID handle: reconnects forever, writes queued reports, reads decisions."""

    def __init__(self, loop, on_decision, on_link):
        super().__init__(daemon=True)
        self.loop, self.on_decision, self.on_link = loop, on_decision, on_link
        self.out = queue.Queue()
        self.connected = False

    def send(self, report: bytes):
        if self.connected:
            self.out.put(report.ljust(REPORT, b"\0"))

    def _open(self):
        for d in hid.enumerate():
            if d.get("usage_page") == VENDOR_USAGE_PAGE and d.get("product_string") == PRODUCT:
                dev = hid.device()
                dev.open_path(d["path"])
                return dev
        return None

    def run(self):
        while True:
            try:
                dev = self._open()
            except OSError:
                dev = None
            if dev is None:
                time.sleep(1)
                continue
            while not self.out.empty():  # anything queued was for the previous connection
                self.out.get_nowait()
            self.connected = True
            log("knob connected")
            self.loop.call_soon_threadsafe(self.on_link, True)
            try:
                while True:
                    while not self.out.empty():
                        if dev.write(b"\0" + self.out.get_nowait()) < 0:
                            raise OSError("write failed")
                    data = dev.read(REPORT, 50)
                    if data and data[0] == EXT_TAG_NOTIFY:
                        self.loop.call_soon_threadsafe(self.on_decision, data[2] | data[3] << 8, data[1])
            except (OSError, ValueError) as e:
                log("knob gone:", e)
            finally:
                self.connected = False
                try:
                    dev.close()
                except Exception:
                    pass
                self.loop.call_soon_threadsafe(self.on_link, False)
            time.sleep(0.5)


class Item:
    def __init__(self, iid, kind, source, session, key, title, body, fp=""):
        self.id, self.kind, self.source, self.session, self.key = iid, kind, source, session, key
        self.title, self.body, self.fp = title, body, fp
        self.created = time.monotonic()
        self.expires = None
        self.waiter = None  # asyncio.Future for an approval

    def report(self, cfg) -> bytes:
        nudge = cfg["nudge"].get("ask" if self.kind == ASK else self.key, False)
        return (bytes([EXT_CMD_NOTIFY, POST]) + self.id.to_bytes(2, "little")
                + bytes([SOURCE_ID.get(self.source, 3), self.kind | (EXT_NOTIFY_NUDGE if nudge else 0)])
                + cstr(self.title, 16) + cstr(self.body, 39) + rgb(cfg["colors"].get(self.source, "#000000")))


class Daemon:
    def __init__(self, loop):
        self.loop = loop
        self.cfg = load_config()
        self.items: dict[int, Item] = {}
        self.shown: list[int] = []  # what the knob holds, in its order
        self.timers: dict[tuple, asyncio.TimerHandle] = {}
        self.next_id = 1
        self.dev = Device(loop, self.on_decision, self.on_link)

    # --- the knob's queue: approvals first (oldest first), then attention items (newest first)
    def sync(self):
        if not self.dev.connected:
            self.shown = []
            return
        asks = sorted((i for i in self.items.values() if i.kind == ASK), key=lambda i: i.created)
        infos = sorted((i for i in self.items.values() if i.kind == INFO), key=lambda i: -i.created)
        want = [i.id for i in (asks + infos)[:SHOWN_MAX]]
        keep = 0
        while keep < min(len(want), len(self.shown)) and want[keep] == self.shown[keep]:
            keep += 1
        for iid in self.shown[keep:]:  # everything after the common head comes off...
            self.dev.send(bytes([EXT_CMD_NOTIFY, CLEAR]) + iid.to_bytes(2, "little"))
        for iid in want[keep:]:        # ...and goes back on in the right order
            self.dev.send(self.items[iid].report(self.cfg))
        self.shown = want

    def repost(self, item: Item):
        if item.id in self.shown:
            self.dev.send(item.report(self.cfg))  # same id: the knob updates it in place

    def new_id(self) -> int:
        while True:
            iid, self.next_id = self.next_id, self.next_id % 65535 + 1
            if iid not in self.items:
                return iid

    def remove(self, iid: int, answer=None):
        item = self.items.pop(iid, None)
        if item and item.waiter and not item.waiter.done():
            item.waiter.set_result(answer)
        self.sync()

    # --- device callbacks (event loop thread) ---
    def on_link(self, up: bool):
        if up:
            self.dev.send(bytes([EXT_CMD_NOTIFY, CLEAR_ALL, 0, 0]))
            self.shown = []
            self.sync()
        else:
            self.shown = []
            for item in list(self.items.values()):  # nobody can answer on the knob now
                if item.kind == ASK:
                    self.remove(item.id, None)

    def on_decision(self, iid: int, decision: int):
        if iid in self.shown:
            self.shown.remove(iid)  # the knob already dropped it
        item = self.items.get(iid)
        if item is None:
            return
        log(f"knob: {item.source} #{iid} {item.title!r} -> {({ALLOW: 'ALLOW', DENY: 'DENY', LATER: 'LATER', DISMISS: 'DISMISS'}).get(decision, decision)}")
        self.remove(iid, decision)

    # --- requests ---
    async def ask(self, req, reader) -> dict:
        if not self.dev.connected:
            return {"decision": "none"}
        item = Item(self.new_id(), ASK, req.get("source", ""), req.get("session", ""), "ask",
                    req.get("title", "APPROVE?"), req.get("body", ""), req.get("fp", ""))
        item.waiter = self.loop.create_future()
        self.items[item.id] = item
        # A pending "approve in app" reminder for this session is superseded by the real thing.
        self.clear(item.source, item.session, "inapp")
        self.sync()
        log(f"ask: {item.source} #{item.id} {item.title!r} {item.body!r}")
        gone = asyncio.ensure_future(reader.read(1))  # EOF: the agent dropped the hook
        done, _ = await asyncio.wait({item.waiter, gone}, timeout=float(self.cfg["approval_wait_s"]),
                                     return_when=asyncio.FIRST_COMPLETED)
        gone.cancel()
        if item.waiter.done():
            answer = item.waiter.result()
            return {"decision": "allow" if answer == ALLOW else "deny" if answer == DENY else "none"}
        if gone in done:
            log(f"ask #{item.id}: hook went away")
            self.remove(item.id)
            return {"decision": "none"}
        # Timed out: the app asks now. Leave a reminder that the session moving on clears.
        item.waiter.set_result(None)
        item.waiter, item.kind, item.key = None, INFO, "inapp"
        item.title, item.body = "APPROVE IN APP", item.title + ": " + item.body
        item.expires = time.monotonic() + float(self.cfg["info_ttl_s"])
        self.repost(item)
        self.sync()
        log(f"ask #{item.id}: no answer on the knob, the app asks")
        return {"decision": "none"}

    def info(self, req):
        source, session, key = req.get("source", ""), req.get("session", ""), req.get("key", "info")
        if key == "turn" and not self.cfg["your_turn"]:
            return
        if req.get("unless_ask") and any(i.kind == ASK and i.source == source and i.session == session
                                         for i in self.items.values()):
            return
        delay = {"turn": self.cfg["turn_delay_s"], "run": self.cfg["run_delay_s"]}.get(req.get("delay"), 0)
        tk = (source, session, key)
        if tk in self.timers:
            self.timers.pop(tk).cancel()

        def post():
            self.timers.pop(tk, None)
            existing = next((i for i in self.items.values()
                             if i.kind == INFO and (i.source, i.session, i.key) == tk), None)
            if existing:
                existing.title, existing.body = req.get("title", ""), req.get("body", "")
                existing.expires = time.monotonic() + float(self.cfg["info_ttl_s"])
                self.repost(existing)
                return
            item = Item(self.new_id(), INFO, source, session, key, req.get("title", ""), req.get("body", ""))
            item.expires = time.monotonic() + float(self.cfg["info_ttl_s"])
            self.items[item.id] = item
            log(f"info: {source} #{item.id} {item.title!r} {item.body!r}")
            self.sync()

        if delay:
            self.timers[tk] = self.loop.call_later(float(delay), post)
        else:
            post()

    def clear(self, source, session, what="all", fp=""):
        for tk in [k for k in self.timers if k[0] == source and k[1] == session
                   and (what == "all" or k[2] == what or (what == "asks" and k[2] == "inapp"))]:
            self.timers.pop(tk).cancel()
        for item in list(self.items.values()):
            if item.source != source or item.session != session:
                continue
            hit = (what == "all"
                   or (what == "asks" and (item.kind == ASK or item.key == "inapp"))
                   or (what == "fp" and fp and item.fp == fp)
                   or (what not in ("all", "asks", "fp") and item.key == what))
            if hit:
                self.remove(item.id, None)

    async def handle(self, reader, writer):
        try:
            line = await asyncio.wait_for(reader.readline(), 5)
            req = json.loads(line or b"{}")
            op = req.get("op")
            if op == "ask":
                reply = await self.ask(req, reader)
            elif op == "info":
                self.info(req)
                reply = {"ok": True}
            elif op == "clear":
                self.clear(req.get("source", ""), req.get("session", ""), req.get("what", "all"), req.get("fp", ""))
                reply = {"ok": True}
            elif op == "status":
                reply = {"knob": self.dev.connected, "items": [
                    {"id": i.id, "kind": "ask" if i.kind == ASK else "info", "source": i.source,
                     "title": i.title, "body": i.body} for i in self.items.values()]}
            else:
                reply = {"error": "unknown op"}
            writer.write((json.dumps(reply) + "\n").encode())
            await writer.drain()
        except (asyncio.TimeoutError, ValueError, ConnectionError):
            pass
        finally:
            writer.close()

    async def janitor(self):
        while True:
            await asyncio.sleep(10)
            now = time.monotonic()
            for item in list(self.items.values()):
                if item.expires and now > item.expires:
                    self.remove(item.id)


async def main():
    os.makedirs(HOME, mode=0o700, exist_ok=True)
    if os.path.exists(SOCK):
        try:  # another instance answering? then leave it be
            r, w = await asyncio.open_unix_connection(SOCK)
            w.close()
            log("already running")
            return
        except OSError:
            os.unlink(SOCK)
    loop = asyncio.get_running_loop()
    d = Daemon(loop)
    old = os.umask(0o177)  # the socket is owner-only
    server = await asyncio.start_unix_server(d.handle, SOCK)
    os.umask(old)
    d.dev.start()
    log(f"listening on {SOCK}, config {d.cfg}")
    loop.create_task(d.janitor())
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
