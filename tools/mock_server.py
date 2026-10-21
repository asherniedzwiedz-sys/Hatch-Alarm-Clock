#!/usr/bin/env python3
"""Serve data/index.html against an in-memory fake of the clock's JSON API, for UI work without
hardware:  python tools/mock_server.py  ->  http://127.0.0.1:8765
Validation mirrors alarmsFromJson() in src/Logic.h; every accepted POST body is logged to stdout."""
import json
import random
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

PAGE = Path(__file__).resolve().parent.parent / "data" / "index.html"
STATE = {
    "fw": "mock", "timeValid": True, "rtc": True, "phase": "idle", "auth": False,
    "alarms": [{"hour": 6, "minute": 45, "days": 62, "enabled": True}],
    "settings": {"tz": "UTC0", "ssid": "", "sunriseMax": 100, "nightPct": 30, "nightKelvin": 2700,
                 "autoDim": False, "alarmVolume": 60, "soundVolume": 40, "clockLevel": 4, "clock24": False, "sleepTracking": True, "sleepRangeCm": 200},
    "light": {"on": False, "lux": 12.5, "clock": True},
    "audio": {"source": "off", "current": "", "volume": 40, "timer": 0, "sd": True,
              "sounds": ["white", "pink", "rain", "ocean", "fan", "my-rain.wav"]},
    "wifi": {"connected": False, "ap": True, "ssid": "", "ip": ""},
    "sleep": {"sensor": True, "present": True, "motion": 3, "distance": 140, "tracking": False},
}


def fake_nights():
    """7 plausible nights, newest first, in the same shape as nightToJson()."""
    rng, nights = random.Random(7), []
    base = int(time.mktime(time.localtime()[:3] + (0, 0, 0, 0, 0, -1))) - 90 * 60  # last night, 22:30 local
    for i in range(7):
        start = base - i * 86400 + rng.randint(-40, 40) * 60
        stages, length = "1" * rng.randint(2, 5), rng.randint(88, 104)
        while len(stages) < length:
            stages += "2" * rng.randint(10, 30) + "1" * rng.randint(1, 3) + ("0" if rng.random() < 0.2 else "")
        asleep = stages.count("2") * 5
        nights.append({"start": start, "end": start + len(stages) * 300, "inBed": len(stages) * 5, "asleep": asleep,
                       "awake": len(stages) * 5 - asleep, "latency": stages.index("2") * 5,
                       "wakeups": stages.count("12"), "stages": stages})
    return nights


NIGHTS = fake_nights()
RANGES = {"sunriseMax": (1, 100), "nightPct": (1, 100), "nightKelvin": (1700, 6500), "alarmVolume": (0, 100), "soundVolume": (0, 100),
          "clockLevel": (0, 16), "sleepRangeCm": (30, 600)}


def is_int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def valid_alarms(body):
    return isinstance(body, list) and len(body) <= 10 and all(
        isinstance(a, dict) and is_int(a.get("hour")) and 0 <= a["hour"] <= 23 and is_int(a.get("minute"))
        and 0 <= a["minute"] <= 59 and is_int(a.get("days", 0)) and 0 <= a.get("days", 0) <= 127
        and isinstance(a.get("enabled", True), bool) for a in body)


def apply_settings(body):
    for k, (lo, hi) in RANGES.items():
        if k in body and not (is_int(body[k]) and lo <= body[k] <= hi):
            return False
    for k in RANGES:
        if k in body:
            STATE["settings"][k] = body[k]
    for k in ("tz", "ssid", "autoDim", "clock24", "sleepTracking"):
        if k in body:
            STATE["settings"][k] = body[k]
    if "adminPass" in body:
        STATE["auth"] = bool(body["adminPass"])
    return True


def handle(path, body):
    if path == "/api/alarms":
        if not valid_alarms(body):
            return False
        STATE["alarms"] = [{"days": 0, "enabled": True, **a} for a in body]
        return True
    if not isinstance(body, dict):
        return False
    if path == "/api/settings":
        return apply_settings(body)
    if path == "/api/light":
        if not isinstance(body.get("on"), bool) or not apply_settings(body):
            return False
        STATE["light"]["on"] = body["on"]
        return True
    if path == "/api/sound":
        if not apply_settings(body):
            return False
        a = STATE["audio"]
        if body.get("stop"):
            a.update(source="off", current="", timer=0)
        elif "play" in body:
            if body["play"] not in a["sounds"]:
                return False
            a.update(source="sound", current=body["play"], timer=60 * body.get("timer", 0))
        return True
    if path == "/api/time":
        return is_int(body.get("epoch")) and 1704067200 <= body["epoch"] <= 2145916800
    if path == "/api/sleep":
        if body.get("clear") is not True:
            return False
        NIGHTS.clear()
        return True
    if path == "/api/alarm":
        return body.get("action") in ("snooze", "dismiss")
    return False


class Handler(BaseHTTPRequestHandler):
    def _send(self, code, data, ctype="application/json"):
        raw = data if isinstance(data, bytes) else json.dumps(data).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        if self.path.split("?")[0] in ("/", "/index.html"):
            return self._send(200, PAGE.read_bytes(), "text/html")
        if self.path == "/api/state":
            now = int(time.time())
            nxt = next((i for i, a in enumerate(STATE["alarms"]) if a["enabled"]), -1)
            return self._send(200, {**STATE, "time": now, "local": time.strftime("%a %Y-%m-%d %H:%M:%S", time.gmtime(now)),
                                    "next": {"index": nxt, "in": 8 * 3600 if nxt >= 0 else 0}})
        if self.path == "/api/sleep":
            return self._send(200, {"live": STATE["sleep"], "nights": NIGHTS})
        self._send(404, {"ok": False})

    def do_POST(self):
        try:
            body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"null")
        except ValueError:
            return self._send(400, {"ok": False})
        ok = handle(self.path, body)
        print(f"POST {self.path} {'OK ' if ok else 'BAD'} {json.dumps(body)}", flush=True)
        self._send(200 if ok else 400, {"ok": ok})

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    print("mock clock on http://127.0.0.1:8765", flush=True)
    ThreadingHTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
