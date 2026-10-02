#!/usr/bin/env python3
"""quadra -- host tool for this fork's firmware extensions (src/ext_proto.h).

    quadra.py hello                   firmware and extension versions
    quadra.py profile [name]          list app profiles, or switch to one (saved)
    quadra.py reboot [--serial]       restart; --serial = one boot as USB-Serial-JTAG (flashing)
    quadra.py flash [firmware.bin]    serial reboot -> flash the app -> back to HID, no buttons

Talks to the vendor HID interface (no macOS Input Monitoring permission needed).
Setup: python3 -m pip install hidapi esptool
"""
import argparse
import os
import subprocess
import sys
import time

import hid

REPORT_SIZE = 64
VENDOR_USAGE_PAGE, VENDOR_USAGE = 0xFF00, 0x01
PRODUCT = "Quadra"
USJ_VID, USJ_PID = 0x303A, 0x1001  # the chip's own USB-Serial-JTAG (serial boot, ROM loader)

# src/host_proto.h
CMD_HELLO, TAG_HELLO = 0x10, 0xB0
# src/ext_proto.h
EXT_HELLO, EXT_REBOOT = 0x20, 0x21
EXT_TAG_HELLO, EXT_TAG_ACK = 0xC0, 0xC1
EXT_REBOOT_NORMAL, EXT_REBOOT_SERIAL = 0, 1
EXT_ST = {0: "OK", 1: "BAD_PARAM", 2: "UNKNOWN"}

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_FW = os.path.join(HERE, "..", ".pio", "build", "esp32-s3-devkitm-1", "firmware.bin")
APP_OFFSET = "0x10000"  # boards/nano_partitions.csv app0


class Quadra:
    def __init__(self):
        found = [d for d in hid.enumerate()
                 if d.get("usage_page") == VENDOR_USAGE_PAGE and d.get("usage") == VENDOR_USAGE
                 and d.get("product_string") == PRODUCT]
        if not found:
            raise SystemExit("no Quadra found (HID mode, vendor interface)")
        self.dev = hid.device()
        self.dev.open_path(found[0]["path"])

    def close(self):
        self.dev.close()

    def send(self, report: bytes):
        report = report.ljust(REPORT_SIZE, b"\0")
        if self.dev.write(b"\0" + report) < 0:
            raise IOError("HID write failed")

    def request(self, report: bytes, tags, timeout_s=1.0):
        """Send one command, return the first reply whose tag is in `tags` (or None)."""
        self.send(report)
        end = time.monotonic() + timeout_s
        while time.monotonic() < end:
            data = self.dev.read(REPORT_SIZE, 100)
            if data and data[0] in tags:
                return bytes(data)
        return None


def cstr(b: bytes) -> str:
    return b.split(b"\0", 1)[0].decode(errors="replace")


def cmd_hello(_args):
    q = Quadra()
    try:
        h = q.request(bytes([CMD_HELLO]), {TAG_HELLO})
        x = q.request(bytes([EXT_HELLO]), {EXT_TAG_HELLO, EXT_TAG_ACK, 0xA0}, 0.5)
    finally:
        q.close()
    if h:
        print(f"firmware {cstr(h[4:36])} ({cstr(h[36:52])}), protocol {h[1]}, {h[2]} profiles")
    print(f"extensions v{x[1]}" if x and x[0] == EXT_TAG_HELLO else "extensions: none (stock firmware)")


CMD_SET, CMD_SAVE, CMD_PROFILE = 0x12, 0x13, 0x16
TAG_SETTINGS, TAG_PROFILE, TAG_ERROR = 0xB1, 0xB2, 0xBF
SET_HID_TYPE, SET_PROFILE, HID_APP = 7, 9, 3


def profiles(q):
    out, i, count = [], 0, 1
    while i < count:
        r = q.request(bytes([CMD_PROFILE, i]), {TAG_PROFILE, TAG_ERROR})
        if not r or r[0] != TAG_PROFILE:
            break
        count = r[2]
        out.append((cstr(r[4:16]), cstr(r[16:32])))
        i += 1
    return out


def cmd_profile(args):
    """List profiles, or make one the active (and saved) APP profile."""
    q = Quadra()
    try:
        ps = profiles(q)
        if not args.name:
            for i, (pid, name) in enumerate(ps):
                print(f"{i}: {pid:12} {name}")
            return
        want = args.name.lower()
        idx = next((i for i, (pid, name) in enumerate(ps) if want in (pid, name.lower())), None)
        if idx is None:
            raise SystemExit(f"no profile '{args.name}' (have: {', '.join(p for p, _ in ps)})")
        for sid, val in ((SET_HID_TYPE, HID_APP), (SET_PROFILE, idx)):
            r = q.request(bytes([CMD_SET, sid, 0, 0]) + val.to_bytes(4, "little"), {TAG_SETTINGS, TAG_ERROR})
            if not r or r[0] != TAG_SETTINGS:
                raise SystemExit("setting refused")
        q.request(bytes([CMD_SAVE]), {TAG_SETTINGS})
        print(f"active profile: {ps[idx][1]} (saved)")
    finally:
        q.close()


def reboot(serial: bool) -> bool:
    """True if the device acknowledged; False if its firmware has no extensions."""
    q = Quadra()
    try:
        r = q.request(bytes([EXT_REBOOT, EXT_REBOOT_SERIAL if serial else EXT_REBOOT_NORMAL]),
                      {EXT_TAG_ACK, 0xA0}, 1.0)
    finally:
        q.close()
    if not r or r[0] != EXT_TAG_ACK or r[1] != EXT_REBOOT:
        return False
    if r[2] != 0:
        raise SystemExit(f"reboot refused: {EXT_ST.get(r[2], r[2])}")
    return True


def cmd_reboot(args):
    if not reboot(args.serial):
        raise SystemExit("this firmware has no reboot command (stock?)")
    print("restarting" + (" into serial mode" if args.serial else ""))


def usj_port():
    from serial.tools import list_ports
    for p in list_ports.comports():
        if p.vid == USJ_VID and p.pid == USJ_PID:
            return p.device
    return None


def wait_for(fn, timeout_s, what):
    end = time.monotonic() + timeout_s
    while time.monotonic() < end:
        v = fn()
        if v:
            return v
        time.sleep(0.25)
    raise SystemExit(f"timed out waiting for {what}")


def quadra_present():
    return any(d.get("usage_page") == VENDOR_USAGE_PAGE and d.get("product_string") == PRODUCT
               for d in hid.enumerate())


def cmd_flash(args):
    fw = os.path.abspath(args.firmware)
    if not os.path.isfile(fw):
        raise SystemExit(f"no firmware at {fw} (build first: pio run)")
    port = usj_port()
    if port is None:
        if quadra_present() and reboot(True):
            print("serial reboot requested")
            port = wait_for(usj_port, 20, "the serial port after the reboot "
                            "(if the device went dark: tap EN, or hold F3+F4 while plugging in)")
        else:
            print("This firmware can't reboot itself into serial mode (first flash of this fork?).\n"
                  "Hold F3+F4 and tap EN (or plug in while holding them); keep holding ~3 s.")
            port = wait_for(usj_port, 600, "the serial port (F3+F4 boot)")
    print(f"flashing {os.path.basename(fw)} via {port}")
    rc = subprocess.call([sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", port,
                          "--baud", "921600", "--before", "default-reset", "--after", "hard-reset",
                          "write-flash", APP_OFFSET, fw])
    if rc != 0:
        raise SystemExit(f"esptool failed ({rc})")
    wait_for(quadra_present, 30, "the device to come back in HID mode")
    time.sleep(0.5)
    cmd_hello(args)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("hello").set_defaults(fn=cmd_hello)
    p = sub.add_parser("profile", help="list profiles, or switch to one (saved)")
    p.add_argument("name", nargs="?")
    p.set_defaults(fn=cmd_profile)
    p = sub.add_parser("reboot")
    p.add_argument("--serial", action="store_true", help="one boot as USB-Serial-JTAG (for flashing)")
    p.set_defaults(fn=cmd_reboot)
    p = sub.add_parser("flash")
    p.add_argument("firmware", nargs="?", default=DEFAULT_FW)
    p.set_defaults(fn=cmd_flash)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
