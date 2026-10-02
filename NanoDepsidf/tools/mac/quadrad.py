#!/usr/bin/env python3
"""quadrad -- the Quadra knob's companion service on the Mac.

Agents. Hooks in Claude Code, Codex and Cursor (quadra_hook.py) talk to this service over a
Unix socket (~/.quadra/agentd.sock, owner-only). Approvals show on the knob (vendor HID,
src/ext_proto.h EXT_CMD_NOTIFY) and wait for its answer: hold F1 = allow, F3 = deny, F2 / F4 =
answer in the app. Nothing here approves on its own: without a knob, after `approval_wait_s`,
or when the hook goes away, the answer is "no decision" and the agent asks in its own UI. The
same events keep the AGENTS profile's dashboard (EXT_CMD_AGENTS) current.

Music. What's playing (Kaset via its saved session, Music and Spotify via AppleScript) goes to
the MUSIC profile: the cover as a 240x240 JPEG (EXT_CMD_COVER), the title, artist, the cover's
colours, the system volume and whether audio is running (EXT_CMD_TRACK).

Config: ~/.quadra/config.json (see DEFAULTS). Log: stdout (the LaunchAgent sends it to
~/Library/Logs/quadrad.log).
"""
import asyncio
import ctypes
import io
import json
import os
import plistlib
import queue
import ssl
import struct
import subprocess
import threading
import time
import urllib.parse
import urllib.request
import zlib

import hid


def share_hid():
    """macOS hidapi opens devices exclusively by default, which would lock this service, the
    CLI and the companion app out of each other. Ask for shared opens (no-op elsewhere)."""
    try:
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
    "music": True,          # now playing on the MUSIC profile
    "players": ["Kaset", "Music", "Spotify"],
}

# --- device (src/ext_proto.h, src/notify.h, src/media.h, src/agent_board.h) ---
REPORT = 64
VENDOR_USAGE_PAGE, PRODUCT = 0xFF00, "Quadra"
EXT_CMD_NOTIFY, EXT_CMD_COVER, EXT_CMD_TRACK, EXT_CMD_AGENTS = 0x25, 0x26, 0x27, 0x28
EXT_TAG_ACK, EXT_TAG_NOTIFY = 0xC1, 0xC3
EXT_NOTIFY_NUDGE = 0x80
POST, CLEAR, CLEAR_ALL = 1, 2, 3
COVER_BEGIN, COVER_DATA, COVER_END, COVER_CHUNK = 1, 2, 3, 58
TRACK_PLAYING, TRACK_NONE = 0x01, 0x80
ASK, INFO = 0, 1
SOURCE_ID = {"claude": 0, "codex": 1, "cursor": 2}
ALLOW, DENY, LATER, DISMISS = 1, 2, 3, 4
SHOWN_MAX = 4   # NOTIFY_MAX
BOARD_MAX = 4   # AGENT_BOARD_MAX
IDLE, WORKING, YOUR_TURN, ASKING = 0, 1, 2, 3
BOARD_STATE = {"working": WORKING, "turn": YOUR_TURN, "asking": ASKING, "idle": IDLE}
SESSION_TTL_S = 1800


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
    s = "".join(c if 0x20 <= ord(c) <= 0x7E else "?" for c in str(s))
    return s.encode("ascii")[:n].ljust(n, b"\0")


class Device(threading.Thread):
    """Owns the HID handle: reconnects forever, writes queued reports, reads what comes back."""

    def __init__(self, loop, on_decision, on_ack, on_link):
        super().__init__(daemon=True)
        self.loop, self.on_decision, self.on_ack, self.on_link = loop, on_decision, on_ack, on_link
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
                    data = dev.read(REPORT, 20)
                    if data and data[0] == EXT_TAG_NOTIFY:
                        self.loop.call_soon_threadsafe(self.on_decision, data[2] | data[3] << 8, data[1])
                    elif data and data[0] == EXT_TAG_ACK:
                        self.loop.call_soon_threadsafe(self.on_ack, data[1], data[2])
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


# ── agents ─────────────────────────────────────────────────────────────────────

class Daemon:
    def __init__(self, loop):
        self.loop = loop
        self.cfg = load_config()
        self.items: dict[int, Item] = {}
        self.shown: list[int] = []  # what the knob holds, in its order
        self.timers: dict[tuple, asyncio.TimerHandle] = {}
        self.next_id = 1
        self.sessions: dict[tuple, dict] = {}  # (source, session) -> {name, state, t}
        self.board_sent = None
        self.acks: dict[int, asyncio.Future] = {}
        self.dev = Device(loop, self.on_decision, self.on_ack, self.on_link)
        self.music = Music(self)

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

    # --- the AGENTS dashboard ---
    def touch(self, req):
        source, session = req.get("source", ""), req.get("session", "")
        state = req.get("state")
        if not source or not session or not state:
            return
        key = (source, session)
        if state == "end":
            self.sessions.pop(key, None)
        else:
            name = os.path.basename(str(req.get("cwd") or "").rstrip("/")) or source
            s = self.sessions.setdefault(key, {"name": name})
            s.update(state=BOARD_STATE.get(state, IDLE), t=time.monotonic())
            if req.get("cwd"):
                s["name"] = name
        self.push_board()

    def push_board(self, force=False):
        order = {ASKING: 0, YOUR_TURN: 1, WORKING: 2, IDLE: 3}
        rows = sorted(self.sessions.items(), key=lambda kv: (order[kv[1]["state"]], -kv[1]["t"]))[:BOARD_MAX]
        payload = bytes([EXT_CMD_AGENTS, len(rows)])
        for (source, _), s in rows:
            payload += bytes([SOURCE_ID.get(source, 3), s["state"]]) + cstr(s["name"], 12)
        if force or payload != self.board_sent:
            self.board_sent = payload
            self.dev.send(payload)

    # --- device callbacks (event loop thread) ---
    def on_link(self, up: bool):
        if up:
            self.dev.send(bytes([EXT_CMD_NOTIFY, CLEAR_ALL, 0, 0]))
            self.shown = []
            self.sync()
            self.push_board(force=True)
            self.music.resend()
        else:
            self.shown = []
            for item in list(self.items.values()):  # nobody can answer on the knob now
                if item.kind == ASK:
                    self.remove(item.id, None)
            for f in self.acks.values():
                if not f.done():
                    f.set_result(None)

    def on_decision(self, iid: int, decision: int):
        if iid in self.shown:
            self.shown.remove(iid)  # the knob already dropped it
        item = self.items.get(iid)
        if item is None:
            return
        log(f"knob: {item.source} #{iid} {item.title!r} -> {({ALLOW: 'ALLOW', DENY: 'DENY', LATER: 'LATER', DISMISS: 'DISMISS'}).get(decision, decision)}")
        if item.kind == ASK and (item.source, item.session) in self.sessions:
            self.touch({"source": item.source, "session": item.session, "state": "working"})
        self.remove(iid, decision)

    def on_ack(self, cmd: int, status: int):
        f = self.acks.pop(cmd, None)
        if f and not f.done():
            f.set_result(status)

    async def request_ack(self, report: bytes, timeout=2.0):
        """Send one command that the knob acknowledges; its status, or None."""
        cmd = report[0]
        f = self.loop.create_future()
        self.acks[cmd] = f
        self.dev.send(report)
        try:
            return await asyncio.wait_for(f, timeout)
        except asyncio.TimeoutError:
            self.acks.pop(cmd, None)
            return None

    # --- hook requests ---
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
            self.touch(req)
            op = req.get("op")
            if op == "ask":
                reply = await self.ask(req, reader)
            elif op == "info":
                self.info(req)
                reply = {"ok": True}
            elif op == "clear":
                self.clear(req.get("source", ""), req.get("session", ""), req.get("what", "all"), req.get("fp", ""))
                reply = {"ok": True}
            elif op == "seen":
                reply = {"ok": True}
            elif op == "status":
                reply = {"knob": self.dev.connected,
                         "items": [{"id": i.id, "kind": "ask" if i.kind == ASK else "info", "source": i.source,
                                    "title": i.title, "body": i.body} for i in self.items.values()],
                         "agents": [{"source": k[0], "name": s["name"], "state": s["state"]} for k, s in self.sessions.items()],
                         "music": self.music.status()}
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
            stale = [k for k, s in self.sessions.items() if now - s["t"] > SESSION_TTL_S]
            for k in stale:
                self.sessions.pop(k)
            if stale:
                self.push_board()


# ── music ──────────────────────────────────────────────────────────────────────

try:
    import certifi
    SSL_CTX = ssl.create_default_context(cafile=certifi.where())
except Exception:
    SSL_CTX = ssl.create_default_context()

KASET_PLIST = os.path.expanduser(
    "~/Library/Containers/com.sertacozercan.Kaset/Data/Library/Preferences/com.sertacozercan.Kaset.plist")
MAX_ART = 8 * 1024 * 1024


class CoreAudio:
    """The system volume and whether anything is playing, straight from CoreAudio (cheap)."""

    class Addr(ctypes.Structure):
        _fields_ = [("sel", ctypes.c_uint32), ("scope", ctypes.c_uint32), ("elem", ctypes.c_uint32)]

    def __init__(self):
        self.ca = ctypes.cdll.LoadLibrary("/System/Library/Frameworks/CoreAudio.framework/CoreAudio")

    @staticmethod
    def _fcc(s):
        return struct.unpack(">I", s.encode())[0]

    def _get(self, obj, sel, scope, ctype):
        a = self.Addr(self._fcc(sel), self._fcc(scope), 0)
        size, v = ctypes.c_uint32(ctypes.sizeof(ctype)), ctype()
        err = self.ca.AudioObjectGetPropertyData(ctypes.c_uint32(obj), ctypes.byref(a), 0, None,
                                                 ctypes.byref(size), ctypes.byref(v))
        return None if err else v.value

    def state(self):
        """(volume 0..100 or -1, audio running)"""
        dev = self._get(1, "dOut", "glob", ctypes.c_uint32)
        if not dev:
            return -1, False
        vol = self._get(dev, "vmvc", "outp", ctypes.c_float)
        running = self._get(dev, "gone", "glob", ctypes.c_uint32)
        return (round(vol * 100) if vol is not None else -1), bool(running)


def osa(script, timeout=3):
    try:
        r = subprocess.run(["osascript", "-e", script], capture_output=True, text=True, timeout=timeout)
        return r.stdout.strip() if r.returncode == 0 else None
    except (subprocess.TimeoutExpired, OSError):
        return None


def running(app):
    return subprocess.run(["pgrep", "-x", app], capture_output=True).returncode == 0


def fetch(url):
    """An image's bytes, or None. Only image/* responses, size-capped, TLS verified."""
    if not url or not url.startswith("https://"):
        return None
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=8, context=SSL_CTX) as r:
            if not r.headers.get_content_type().startswith("image/"):
                return None
            data = r.read(MAX_ART + 1)
        return data if 100 < len(data) <= MAX_ART else None
    except Exception:
        return None


def itunes_cover(title, artist):
    try:
        term = urllib.parse.quote(f"{artist} {title}".strip())
        with urllib.request.urlopen(f"https://itunes.apple.com/search?term={term}&entity=song&limit=1",
                                    timeout=5, context=SSL_CTX) as r:
            res = json.loads(r.read(256 * 1024)).get("results") or []
        art = res[0].get("artworkUrl100", "").replace("100x100bb", "600x600bb") if res else ""
        return fetch(art)
    except Exception:
        return None


class Music:
    POLL_S = 0.5
    GONE_S = 15  # nothing playing for this long: the MUSIC screen goes back to normal

    def __init__(self, daemon):
        self.d = daemon
        self.audio = CoreAudio()
        self.track = None       # {"id", "title", "artist", "player"}
        self.cover = None       # JPEG bytes of the current track
        self.palette = [0, 0, 0]
        self.sent = None        # the last EXT_CMD_TRACK report
        self.need_cover = False
        self.last_seen = 0.0
        self.kaset_warned = False
        self.busy = False

    def status(self):
        t = self.track or {}
        return {"player": t.get("player"), "title": t.get("title"), "artist": t.get("artist"),
                "cover_bytes": len(self.cover or b"")}

    def resend(self):
        self.sent = None
        self.need_cover = self.cover is not None

    # --- what's playing (blocking: runs in the executor) ---
    def kaset(self):
        if not running("Kaset"):
            return None
        try:
            with open(KASET_PLIST, "rb") as fh:
                d = plistlib.load(fh)
        except PermissionError:
            if not self.kaset_warned:
                self.kaset_warned = True
                log("Kaset: macOS won't let this Python read Kaset's data -- allow it under System "
                    "Settings > Privacy & Security > Full Disk Access (or App Data) for "
                    "/Library/Frameworks/Python.framework/Versions/3.13/Resources/Python.app")
            return None
        except (OSError, plistlib.InvalidFileException):
            return None
        try:
            raw = d.get("kaset.saved.playbackSession")
            ps = json.loads(raw.decode("utf-8", "replace") if isinstance(raw, (bytes, bytearray)) else raw)
            q, vid = ps.get("queue") or [], ps.get("currentVideoId")
            cur = next((t for t in q if isinstance(t, dict) and t.get("videoId") == vid), None)
            if cur is None:
                i = ps.get("currentIndex", 0)
                cur = q[i] if isinstance(i, int) and 0 <= i < len(q) else None
            if not cur:
                return None
            artist = ((cur.get("artists") or [{}])[0] or {}).get("name", "")
            return {"id": cur.get("videoId") or cur.get("title"), "title": cur.get("title", ""), "artist": artist,
                    "thumb": cur.get("thumbnailURL") or "", "player": "Kaset"}
        except (ValueError, TypeError, AttributeError):
            return None

    def scriptable(self, app):
        if not running(app):  # never `tell` an app that isn't running: that would launch it
            return None
        out = osa(f'''tell application "{app}"
            if player state is playing or player state is paused then
              set t to current track
              return (name of t) & "\t" & (artist of t) & "\t" & (album of t) & "\t" & (player state as text)
            end if
          end tell''')
        parts = (out or "").split("\t")
        if len(parts) < 4:
            return None
        return {"id": "\t".join(parts[:3]), "title": parts[0], "artist": parts[1], "player": app,
                "playing": parts[3] == "playing"}

    def now_playing(self):
        """The track to show: a scriptable player that's actually playing wins; then Kaset (whose
        saved session can outlive the music); then a paused scriptable player."""
        found = {}
        for p in self.d.cfg.get("players", DEFAULTS["players"]):
            t = self.kaset() if p == "Kaset" else self.scriptable(p) if p in ("Music", "Spotify") else None
            if t:
                found[p] = t
        for t in found.values():
            if t.get("playing"):
                return t
        return found.get("Kaset") or next(iter(found.values()), None)

    def artwork(self, t):
        """The cover's bytes for track `t`, trying the player's own art first."""
        if t["player"] == "Kaset":
            url = t.get("thumb", "")
            if url:
                i = url.rfind("=")
                square = (url[:i] if i != -1 else url) + "=w544-h544-l90-rj"  # a square 544 px cover
                data = fetch(square) or fetch(url)
                if data:
                    return data
        elif t["player"] == "Music":
            tmp = os.path.join(HOME, "cover.tmp")
            ok = osa(f'''tell application "Music" to set ad to data of artwork 1 of current track
                set f to open for access POSIX file "{tmp}" with write permission
                set eof f to 0
                write ad to f
                close access f
                return "ok"''')
            if ok == "ok":
                try:
                    with open(tmp, "rb") as fh:
                        return fh.read(MAX_ART)
                except OSError:
                    pass
        elif t["player"] == "Spotify":
            data = fetch(osa('tell application "Spotify" to get artwork url of current track') or "")
            if data:
                return data
        return itunes_cover(t["title"], t["artist"])

    @staticmethod
    def render(art):
        """(JPEG bytes for the knob, its 3 colours) from any image's bytes; None if unreadable."""
        from PIL import Image
        try:
            img = Image.open(io.BytesIO(art)).convert("RGB")
        except Exception:
            return None
        w, h = img.size
        s = min(w, h)
        img = img.crop(((w - s) // 2, (h - s) // 2, (w + s) // 2, (h + s) // 2)).resize((240, 240), Image.LANCZOS)
        for q in (88, 80, 70, 60):  # baseline JPEG (the knob's decoder doesn't do progressive)
            buf = io.BytesIO()
            img.save(buf, "JPEG", quality=q, optimize=True, progressive=False)
            if buf.tell() <= 60 * 1024:
                break
        # The cover's colours for the ring: weighted by how much of the cover they cover and how
        # vivid they are, each clearly different from the ones already picked (three shades of
        # one orange make a flat ring). Near-black and near-white make a dead ring: skipped.
        quant = img.quantize(colors=12, method=Image.Quantize.FASTOCTREE)
        pal, cands = quant.getpalette(), []
        for count, idx in quant.getcolors() or []:
            r, g, b = pal[idx * 3:idx * 3 + 3]
            mx, mn = max(r, g, b), min(r, g, b)
            if mx < 50 or (mx > 225 and mx - mn < 30):
                continue
            cands.append((count * (mx - mn + 20), (r, g, b)))
        picked = []
        for _, c in sorted(cands, reverse=True):
            if all(sum((a - b) ** 2 for a, b in zip(c, p)) > 90 ** 2 for p in picked):
                picked.append(c)
            if len(picked) == 3:
                break
        vivid = [(r << 16) | (g << 8) | b for r, g, b in picked] or [0xFFB030]
        while len(vivid) < 3:
            vivid.append(vivid[len(vivid) % len(picked)] if picked else vivid[-1])
        return buf.getvalue(), vivid

    # --- to the knob ---
    async def upload_cover(self):
        data = self.cover
        if not data or not self.d.dev.connected:
            return False
        begin = bytes([EXT_CMD_COVER, COVER_BEGIN, 0, 0]) + len(data).to_bytes(4, "little") \
            + (zlib.crc32(data) & 0xFFFFFFFF).to_bytes(4, "little")
        if await self.d.request_ack(begin) != 0:
            return False
        for off in range(0, len(data), COVER_CHUNK):
            piece = data[off:off + COVER_CHUNK]
            self.d.dev.send(bytes([EXT_CMD_COVER, COVER_DATA]) + off.to_bytes(3, "little") + bytes([len(piece)]) + piece)
        return await self.d.request_ack(bytes([EXT_CMD_COVER, COVER_END]), timeout=5.0) == 0

    def track_report(self, vol, playing):
        t = self.track
        if t is None:
            return bytes([EXT_CMD_TRACK, TRACK_NONE])
        flags = TRACK_PLAYING if playing else 0
        pal = b"".join(c.to_bytes(3, "big") for c in self.palette)
        return (bytes([EXT_CMD_TRACK, flags, vol if 0 <= vol <= 100 else 0xFF]) + pal
                + cstr(t["title"], 24) + cstr(t["artist"], 24))

    async def run(self):
        loop = asyncio.get_running_loop()
        while True:
            await asyncio.sleep(self.POLL_S)
            if not self.d.cfg.get("music", True) or not self.d.dev.connected:
                continue
            t = await loop.run_in_executor(None, self.now_playing)
            vol, audio = self.audio.state()
            now = time.monotonic()
            if t:
                self.last_seen = now
                if self.track is None or t["id"] != self.track["id"]:
                    self.track, self.cover, self.palette = t, None, [0, 0, 0]
                    log(f"now playing ({t['player']}): {t['title']} -- {t['artist']}")
                    art = await loop.run_in_executor(None, self.artwork, t)
                    out = await loop.run_in_executor(None, self.render, art) if art else None
                    if out:
                        self.cover, self.palette = out
                        self.need_cover = True
                    else:
                        log("  no cover found")
            elif self.track is not None and now - self.last_seen > self.GONE_S:
                log("nothing playing")
                self.track, self.cover = None, None
            if self.need_cover:
                self.need_cover = False
                t0 = time.monotonic()
                ok = await self.upload_cover()
                log(f"  cover {'sent' if ok else 'FAILED'}: {len(self.cover or b'')} B in {time.monotonic() - t0:.2f} s")
                if not ok:
                    self.need_cover = True
            # Kaset has no play state of its own: audio running on the output device stands in.
            playing = bool(t and t.get("playing", audio))
            report = self.track_report(vol, playing)
            if report != self.sent:
                self.sent = report
                self.d.dev.send(report)


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
    loop.create_task(d.music.run())
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
