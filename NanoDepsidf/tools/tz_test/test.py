"""tzrule.c against the system's zoneinfo: every zone with a POSIX footer, around each switch, from today
for ten years (a footer is the rule after a zone's last listed change: right for now and later).
Run: python3 tools/tz_test/test.py"""
import os, subprocess, sys, tempfile
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo, available_timezones

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = "/usr/share/zoneinfo"

def footer(name):
    with open(os.path.join(ROOT, name), "rb") as f:
        d = f.read()
    return d.rstrip(b"\n").rsplit(b"\n", 1)[-1].decode() if d[:4] == b"TZif" and d[4:5] >= b"2" else None

# Morocco switches around Ramadan, which no POSIX rule can say: tzdata lists those switches
# one by one through 2087 and its footer only holds after that. The host sends such zones as
# their usual offset instead (quadrad / the companion check a footer against the coming year).
NOT_A_RULE = {"Africa/Casablanca", "Africa/El_Aaiun"}

cases, zones = [], 0
for name in sorted(available_timezones() - NOT_A_RULE):
    rule = footer(name) if os.path.exists(os.path.join(ROOT, name)) else None
    if not rule:
        continue
    zones += 1
    z = ZoneInfo(name)
    t = datetime.now(timezone.utc).replace(minute=0, second=0, microsecond=0)
    prev = z.utcoffset(t.astimezone(z).replace(tzinfo=None))
    # hourly through 2026-2035: every switch shows up, and each one's neighbours get checked
    while t.year < datetime.now().year + 10:
        off = t.astimezone(z).utcoffset()
        if off != prev or t.hour == 12 and t.day in (1, 15):
            for dt in (-3601, -1, 0, 1, 3600):
                u = t + timedelta(seconds=dt)
                cases.append(f"{rule}\t{int(u.timestamp())}\t{int(u.astimezone(z).utcoffset().total_seconds())}")
            prev = off
        t += timedelta(hours=1)

exe = os.path.join(tempfile.mkdtemp(), "tz_test")
subprocess.check_call(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-o", exe, os.path.join(HERE, "main.c"), os.path.join(HERE, "../../src/tzrule.c")])
r = subprocess.run([exe], input="\n".join(cases) + "\n", text=True, capture_output=True)
print(f"{zones} zones, " + r.stdout.strip())
sys.exit(r.returncode)
