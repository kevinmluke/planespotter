#!/usr/bin/env python3
"""Serves the device's setup page with a fake API, for testing it in a browser.

  python3 test/host/mock_device.py [port] [setup|settings]

Simulated networks: "Maison-5G" (password "correct"), "Guest-Office" (needs a
sign-in page), "Corp-WiFi" (enterprise: listed,
but the page refuses it).
"""
import json
import os
import re
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = open(os.path.join(ROOT, "src", "web", "pages.h"), encoding="utf-8").read()
PAGE = SRC[SRC.index('R"HTML(') + 7:SRC.index(')HTML"')]
CITIES_CPP = open(os.path.join(ROOT, "src", "generated", "cities.cpp")).read()
CITIES_GZ = bytes(int(h, 16) for h in re.findall(r"0x([0-9a-f]{2})", CITIES_CPP.split("kCitiesGz[] = {")[1].split("};")[0]))

MODE = sys.argv[2] if len(sys.argv) > 2 else "setup"
STATE = {
    "mode": MODE, "version": "1.0.0", "mac": "24:58:7C:A1:3F:2A",
    "connectedSsid": "Maison-5G" if MODE == "settings" else "",
    "hasLocation": MODE == "settings", "tz": "CET-1CEST,M3.5.0,M10.5.0/3",
    "join": {"state": "idle", "ssid": ""},
    "settings": {"lang": 1 if MODE == "settings" else 0, "theme": 1, "units": 1, "mode": 0, "range": 30,
                 "rareRange": 80, "rareLevel": 2, "issAlerts": True, "autoUpdate": True,
                 "place": "Blagnac" if MODE == "settings" else "", "lat": 43.64, "lon": 1.39},
}
NETS = [{"ssid": "Maison-5G", "rssi": -48, "secure": True, "ent": False},
        {"ssid": "Corp-WiFi", "rssi": -60, "secure": True, "ent": True},
        {"ssid": "Guest-Office", "rssi": -66, "secure": False, "ent": False},
        {"ssid": "Livebox-21A4", "rssi": -80, "secure": True, "ent": False}]


def finish_join(wifi):
    if wifi["ssid"] == "Guest-Office":
        result = "captive"
    elif wifi["ssid"] in ("Maison-5G",) and wifi.get("pass") == "correct":
        result = "ok"
        STATE["connectedSsid"] = wifi["ssid"]
    elif wifi["ssid"] in ("Maison-5G", "Livebox-21A4"):
        result = "wrong_password"
    else:
        result = "not_found"
    STATE["join"] = {"state": result, "ssid": wifi["ssid"]}
    print("join", wifi["ssid"], "->", result, flush=True)


class H(BaseHTTPRequestHandler):
    def send(self, code, body, ctype="application/json", headers=None):
        data = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/":
            return self.send(200, PAGE, "text/html; charset=utf-8")
        if path == "/cities.tsv":
            return self.send(200, CITIES_GZ, "text/tab-separated-values; charset=utf-8", {"Content-Encoding": "gzip"})
        if path == "/scan":
            return self.send(200, json.dumps(NETS))
        if path == "/api/state":
            return self.send(200, json.dumps(STATE))
        self.send(302, "", headers={"Location": "/"})

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length") or 0)).decode()
        if self.path == "/api/save":
            req = json.loads(body)
            print("save", json.dumps(req), flush=True)
            s = STATE["settings"]
            for k in ("lang", "theme", "units", "mode", "range", "rareRange", "rareLevel", "issAlerts", "autoUpdate"):
                s[k] = req[k]
            if "location" in req:
                s.update(place=req["location"]["place"], lat=req["location"]["lat"], lon=req["location"]["lon"])
                STATE["tz"] = req["location"]["tz"]
                STATE["hasLocation"] = True
            if "wifi" in req:
                STATE["join"] = {"state": "connecting", "ssid": req["wifi"]["ssid"]}
                threading.Timer(2.5, finish_join, [req["wifi"]]).start()
        self.send(200, '{"ok":true}')

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    print(f"mock device ({MODE}) on http://localhost:{port}/", flush=True)
    ThreadingHTTPServer(("127.0.0.1", port), H).serve_forever()
