"""Music's cover logic against a scripted Now Playing. Run: tools/.venv/bin/python tools/mac/test_music.py"""
import asyncio
import io
import os
import sys
import types

sys.modules["hid"] = types.ModuleType("hid")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import quadrad  # noqa: E402
from PIL import Image  # noqa: E402


def png(rgb):
    buf = io.BytesIO()
    Image.new("RGB", (64, 64), rgb).save(buf, "PNG")
    return buf.getvalue()


RED, BLUE = png((220, 30, 30)), png((30, 60, 220))


class Dev:
    connected = True

    def __init__(self):
        self.sent = []

    def send(self, report):
        self.sent.append(report)


class Daemon:
    cfg = {"music": True}

    def __init__(self):
        self.dev, self.covers = Dev(), []

    async def request_ack(self, report, timeout=1.0):
        if report[1] == quadrad.COVER_BEGIN:
            self.covers.append(report)
        return 0


async def main():
    ca = quadrad.CoreAudio()
    ca.on_change = lambda: object()  # like call_soon_threadsafe's Handle
    assert ca._changed(1, 1, None, None) == 0, "the CoreAudio listener answers an OSStatus"
    lookups = []
    quadrad.itunes_cover = lambda title, artist: lookups.append(title)  # finds nothing
    quadrad.Music.POLL_S, quadrad.Music.ART_WAIT_S = 0.01, 0.15
    d = Daemon()
    m = quadrad.Music(d)
    m.np = types.SimpleNamespace(usable=True, latest=None, start=lambda: None)
    m.audio = types.SimpleNamespace(volume=lambda: 50, watch=lambda on_change: None, follow=lambda: None)
    task = asyncio.create_task(m.run())
    seq = 0

    async def play(title, art, new_art=True, playing=True, wait=0.08):
        nonlocal seq
        seq += new_art
        m.np.latest = ({"bundle": "com.sertacozercan.Kaset", "title": title, "artist": "X", "playing": playing,
                        "art_seq": seq}, art)
        await asyncio.sleep(wait)

    await play("A", RED)
    assert len(d.covers) == 1 and m.cover_of.endswith("A\tX"), "the player's cover goes up"
    await play("A", None, wait=0.3)  # Kaset's stand-in card: no artwork, same track
    assert len(d.covers) == 1 and not lookups, "same track losing its artwork keeps the cover"
    await play("A", RED)
    assert len(d.covers) == 1, "the same picture again isn't re-sent"
    await play("B", RED, new_art=False, wait=0.3)
    assert len(d.covers) == 1 and not lookups, "the next track of the same album keeps the cover"
    await play("C", None, wait=0.05)
    assert len(d.covers) == 1 and not lookups, "a track without artwork gets a moment for it to come"
    await asyncio.sleep(0.3)
    assert lookups == ["C"] and len(d.covers) == 2, "then the iTunes Store, then a placeholder"
    await play("C", BLUE)
    assert len(d.covers) == 3 and m.cover_of.endswith("C\tX"), "artwork that comes late still wins"
    await play("C", BLUE, new_art=False, playing=False)
    track = [r for r in d.dev.sent if r[0] == quadrad.EXT_CMD_TRACK][-1]
    assert track[1] == 0, "a paused player reports paused"
    task.cancel()
    print("all good")


asyncio.run(main())
