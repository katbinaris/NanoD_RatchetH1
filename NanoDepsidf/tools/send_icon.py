#!/usr/bin/env python3
"""Upload a 48x48 icon to the Quadra over its vendor HID interface.

The PNG (any size, any mode) is fitted into 48x48 keeping its aspect ratio, alpha is
flattened onto black (the device's background), converted to RGB565 big-endian, and sent in
64-byte HID reports. The wire protocol is documented in src/icon_store.h.

Setup (once):
    python3 -m venv tools/.venv && tools/.venv/bin/pip install -r tools/requirements.txt

Examples:
    tools/.venv/bin/python tools/send_icon.py icon.png
    tools/.venv/bin/python tools/send_icon.py --test-pattern
    tools/.venv/bin/python tools/send_icon.py icon.png --dry-run --preview preview.png
    tools/.venv/bin/python tools/send_icon.py --clear
    tools/.venv/bin/python tools/send_icon.py --list
"""
import argparse
import struct
import sys
import zlib

from PIL import Image, ImageDraw

# --- must match src/icon_store.h ---
REPORT_SIZE = 64
DATA_HEADER = 4
DATA_MAX = REPORT_SIZE - DATA_HEADER
ICON_W = ICON_H = 48
FORMAT_RGB565_BE = 1
ICON_BYTES = ICON_W * ICON_H * 2
REPLY_TAG = 0xA0
CMD_BEGIN, CMD_DATA, CMD_END, CMD_CLEAR = 0x01, 0x02, 0x03, 0x04
STATUS = {0: "OK", 1: "BAD_STATE", 2: "BAD_PARAM", 3: "BAD_OFFSET", 4: "CRC_FAIL", 5: "INCOMPLETE"}

# --- device matching (src/usb_task.c) ---
VENDOR_USAGE_PAGE = 0xFF00
VENDOR_USAGE = 0x01
PRODUCT_STRING = "Quadra"

REPLY_TIMEOUT_MS = 1000


def fit_icon(img: Image.Image) -> Image.Image:
    """Fit into ICON_W x ICON_H (aspect kept, centered), alpha flattened onto black."""
    img = img.convert("RGBA")
    img.thumbnail((ICON_W, ICON_H), Image.LANCZOS)
    canvas = Image.new("RGBA", (ICON_W, ICON_H), (0, 0, 0, 255))
    canvas.alpha_composite(img, ((ICON_W - img.width) // 2, (ICON_H - img.height) // 2))
    return canvas.convert("RGB")


def to_rgb565_be(img: Image.Image) -> bytes:
    out = bytearray()
    for r, g, b in img.getdata():
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out += struct.pack(">H", v)
    assert len(out) == ICON_BYTES
    return bytes(out)


def rgb565_be_to_image(data: bytes) -> Image.Image:
    """Decode back to RGB -- exactly what the panel will show (for --preview)."""
    img = Image.new("RGB", (ICON_W, ICON_H))
    px = []
    for (v,) in struct.iter_unpack(">H", data):
        r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
        px.append(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    img.putdata(px)
    return img


def test_pattern() -> Image.Image:
    """Asymmetric color test: R/G/B/white quadrants + an arrow pointing up (orientation)."""
    img = Image.new("RGB", (ICON_W, ICON_H), (0, 0, 0))
    d = ImageDraw.Draw(img)
    h = ICON_W // 2
    d.rectangle([0, 0, h - 1, h - 1], fill=(255, 0, 0))
    d.rectangle([h, 0, ICON_W - 1, h - 1], fill=(0, 255, 0))
    d.rectangle([0, h, h - 1, ICON_H - 1], fill=(0, 0, 255))
    d.rectangle([h, h, ICON_W - 1, ICON_H - 1], fill=(255, 255, 255))
    d.polygon([(h, 4), (h - 10, 18), (h + 10, 18)], fill=(0, 0, 0))
    d.rectangle([h - 3, 18, h + 2, 30], fill=(0, 0, 0))
    return img


def find_devices():
    import hid
    return [d for d in hid.enumerate()
            if d.get("usage_page") == VENDOR_USAGE_PAGE and d.get("usage") == VENDOR_USAGE
            and d.get("product_string") == PRODUCT_STRING]


class Link:
    def __init__(self, info):
        import hid
        self.dev = hid.device()
        self.dev.open_path(info["path"])

    def close(self):
        self.dev.close()

    def send(self, payload: bytes):
        report = payload.ljust(REPORT_SIZE, b"\0")
        assert len(report) == REPORT_SIZE
        # hidapi wants the report ID first; 0 = no report ID on this interface.
        if self.dev.write(b"\0" + report) < 0:
            raise IOError("HID write failed: %s" % self.dev.error())

    def read_reply(self, timeout_ms):
        # hidapi's read() treats timeout_ms=0 as "block forever", not "poll" -- a poll has to
        # go through non-blocking mode instead.
        if timeout_ms <= 0:
            self.dev.set_nonblocking(1)
            try:
                data = self.dev.read(REPORT_SIZE)
            finally:
                self.dev.set_nonblocking(0)
        else:
            data = self.dev.read(REPORT_SIZE, timeout_ms)
        if not data:
            return None
        if data[0] != REPLY_TAG:
            raise IOError("unexpected report: %s" % bytes(data[:8]).hex())
        return data[1], data[2], data[3] | (data[4] << 8)  # cmd, status, received

    def drain(self):
        while self.read_reply(0) is not None:
            pass

    def expect_ok(self, cmd, what):
        reply = self.read_reply(REPLY_TIMEOUT_MS)
        if reply is None:
            raise IOError("%s: no reply from device (timeout)" % what)
        rcmd, status, received = reply
        if status != 0:
            raise IOError("%s: device reported %s (cmd 0x%02x, %d bytes received)"
                          % (what, STATUS.get(status, status), rcmd, received))


def upload(link: Link, data: bytes):
    link.drain()
    crc = zlib.crc32(data) & 0xFFFFFFFF
    link.send(struct.pack("<BBBBHI", CMD_BEGIN, ICON_W, ICON_H, FORMAT_RGB565_BE, len(data), crc))
    link.expect_ok(CMD_BEGIN, "BEGIN")
    chunks = 0
    for off in range(0, len(data), DATA_MAX):
        chunk = data[off:off + DATA_MAX]
        link.send(struct.pack("<BHB", CMD_DATA, off, len(chunk)) + chunk)
        chunks += 1
        # The device only answers a DATA chunk when it rejects it -- check without waiting.
        reply = link.read_reply(0)
        if reply is not None:
            _, status, received = reply
            raise IOError("DATA @%d rejected: %s (%d bytes received)" % (off, STATUS.get(status, status), received))
    link.send(bytes([CMD_END]))
    link.expect_ok(CMD_END, "END")
    return chunks, crc


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("png", nargs="?", help="image file to upload (any size; fitted to 48x48)")
    ap.add_argument("--test-pattern", action="store_true", help="send a built-in color/orientation test icon")
    ap.add_argument("--clear", action="store_true", help="remove the icon from the device")
    ap.add_argument("--list", action="store_true", help="list matching vendor HID interfaces")
    ap.add_argument("--dry-run", action="store_true", help="convert only, don't touch USB")
    ap.add_argument("--preview", metavar="OUT.png", help="save the converted icon (as the panel will show it, 4x)")
    args = ap.parse_args()

    if args.list:
        devs = find_devices()
        for d in devs:
            print("%04x:%04x  %s  %s" % (d["vendor_id"], d["product_id"], d.get("product_string"), d["path"]))
        if not devs:
            print("no Quadra vendor HID interface found")
        return 0 if devs else 1

    data = None
    if not args.clear:
        if args.test_pattern:
            img = test_pattern()
        elif args.png:
            img = fit_icon(Image.open(args.png))
        else:
            ap.error("give a PNG, --test-pattern, --clear or --list")
        data = to_rgb565_be(img)
        n_chunks = -(-len(data) // DATA_MAX)
        print("icon: %dx%d RGB565, %d bytes, %d chunks, crc32 %08x"
              % (ICON_W, ICON_H, len(data), n_chunks, zlib.crc32(data) & 0xFFFFFFFF))
        if args.preview:
            rgb565_be_to_image(data).resize((ICON_W * 4, ICON_H * 4), Image.NEAREST).save(args.preview)
            print("preview written to", args.preview)

    if args.dry_run:
        return 0

    devs = find_devices()
    if not devs:
        print("error: no Quadra vendor HID interface found (is the new firmware flashed "
              "and the board in HID USB mode?)", file=sys.stderr)
        return 1
    link = Link(devs[0])
    try:
        if args.clear:
            link.drain()
            link.send(bytes([CMD_CLEAR]))
            link.expect_ok(CMD_CLEAR, "CLEAR")
            print("icon cleared")
        else:
            chunks, crc = upload(link, data)
            print("uploaded %d chunks, device verified crc32 %08x" % (chunks, crc))
    except IOError as e:
        print("error:", e, file=sys.stderr)
        return 1
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
