#!/usr/bin/env python3
"""Stand-in for a GbbDongle's web_server, for working on firmware/webui/.

Serves the panel from the working tree the way the firmware does (an index
page that loads /0.css and /0.js) and fakes the web_server API it talks to:
the /events stream (ping, state and log events, same JSON as ESPHome
2026.10) and the REST calls (POST /<domain>/<name>/<action>). Saving a cloud
setting raises Restart Required; pressing Apply Settings (Restart) drops the
stream for a few seconds and comes back with the cloud connected, like a
reboot. Edit panel.js/panel.css and reload the browser, no flashing needed.

    python3 tools/webui/mock_device.py                # factory line, WiFi
    python3 tools/webui/mock_device.py --ethernet --line ha --unconfigured

Standard library only. Not a test of the firmware: it mirrors the entities in
firmware/common/*.yaml by hand.
"""

import argparse
import json
import queue
import random
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlsplit

WEBUI = Path(__file__).resolve().parents[2] / "firmware" / "webui"
INDEX = (
    "<!DOCTYPE html><html><head><meta charset=UTF-8><link rel=icon href=data:>"
    "<link rel=stylesheet href=/0.css></head><body>"
    "<script type=module src=/0.js></script><esp-app></esp-app></body></html>"
)
CLOUD = {"text/MQTT Server", "text/Plant Id", "text/Plant Token", "number/MQTT Port",
         "switch/Cloud Connection", "switch/TLS", "switch/TLS Skip CN Check"}
ESC = "\x1b"
LOG_COLOURS = {"E": "1;31", "W": "0;33", "I": "0;32", "D": "0;36", "V": "0;37", "C": "0;35"}


class Device:
    def __init__(self, args):
        self.args = args
        self.lock = threading.Lock()
        self.clients = []
        self.boot = time.time()
        self.down_until = 0.0
        self.ents = {}
        configured = not args.unconfigured

        def add(domain, name, cat, value, state=None, **extra):
            e = {"id": f"{domain}/{name}", "domain": domain, "name": name, "icon": "",
                 "entity_category": cat}
            if value is not None:
                e["value"] = value
                e["state"] = state if state is not None else str(value)
            e.update(extra)
            self.ents[e["id"]] = e

        add("binary_sensor", "Cloud Connected", 0, configured, "ON" if configured else "OFF")
        add("binary_sensor", "Restart Required", 2, False, "OFF")
        add("button", "Apply Settings (Restart)", 0, None)
        add("text_sensor", "Uptime", 2, "")
        add("switch", "Cloud Connection", 1, True, "ON", assumed_state=False)
        add("text", "Plant Id", 1, "48213" if configured else "", min_length=0, max_length=64, pattern="", mode=0)
        add("text", "Plant Token", 1, "", "********", min_length=0, max_length=64, pattern="", mode=1)
        add("text", "MQTT Server", 1, "gbboptimizer7-mqtt.gbbsoft.pl" if configured else "",
            min_length=0, max_length=128, pattern="", mode=0)
        add("number", "MQTT Port", 1, "8883", min_value="1", max_value="65535", step="1", mode=1)
        add("switch", "TLS", 1, True, "ON", assumed_state=False)
        add("switch", "TLS Skip CN Check", 1, False, "OFF", assumed_state=False)
        add("switch", "Persist Emergency Commands", 1, True, "ON", assumed_state=False)
        add("sensor", "Emergency Sets Stored", 2, 2, "2")
        add("select", "RS485 Baud Rate", 1, "9600",
            option=["2400", "4800", "9600", "19200", "38400", "57600", "115200"])
        add("select", "RS485 Parity", 1, "None", option=["None", "Even", "Odd"])
        add("sensor", "Requests Handled", 2, 1284, "1284")
        add("sensor", "Modbus Errors", 2, 3, "3")
        add("text_sensor", "IP Address", 2, "192.168.2.82")
        if not args.ethernet:
            add("sensor", "WiFi Signal dB", 2, -61, "-61 dBm", uom="dBm")
            add("sensor", "WiFi Signal Percent", 2, 78, "78 Signal %", uom="Signal %")
        add("text_sensor", "Firmware Version", 2, args.version)
        add("button", "Factory Reset", 2, None)
        if args.line in ("factory", "ha"):
            add("update", "Firmware", 1, args.latest, "UPDATE AVAILABLE" if args.latest != args.version else "NO UPDATE",
                current_version=args.version, title="GbbDongle", summary="", release_url="")
        if args.line == "factory":
            add("text", "Admin Password", 1, "", "********", min_length=0, max_length=64, pattern="", mode=1)

    # --- event stream ---
    def send(self, event, data):
        with self.lock:
            for q in list(self.clients):
                q.put((event, data))

    def publish(self, eid):
        e = self.ents[eid]
        self.send("state", json.dumps({k: e[k] for k in ("id", "value", "state") if k in e}))

    def log(self, level, tag, msg):
        line = f"[{level}][{tag}]: {msg}"
        self.send("log", f"{ESC}[{LOG_COLOURS.get(level, '0')}m{line}{ESC}[0m")

    def set(self, eid, value, state=None):
        e = self.ents[eid]
        e["value"] = value
        e["state"] = state if state is not None else str(value)
        self.publish(eid)
        if eid in CLOUD:
            self.ents["binary_sensor/Restart Required"].update(value=True, state="ON")
            self.publish("binary_sensor/Restart Required")
            self.log("I", "gbb_dongle:155", "Cloud settings changed; restart the device to apply them")

    def reboot(self, seconds=6.0):
        self.log("I", "app:100", "Rebooting safely...")
        self.down_until = time.time() + seconds
        with self.lock:
            for q in self.clients:
                q.put(None)
        self.boot = time.time() + seconds
        self.ents["binary_sensor/Restart Required"].update(value=False, state="OFF")
        ok = bool(self.ents["text/Plant Id"]["value"]) and self.ents["switch/Cloud Connection"]["value"]
        self.ents["binary_sensor/Cloud Connected"].update(value=ok, state="ON" if ok else "OFF")

    def uptime(self):
        s = int(max(0, time.time() - self.boot)) + (0 if self.args.unconfigured else 273000)
        d, s = divmod(s, 86400)
        hh, s = divmod(s, 3600)
        m, s = divmod(s, 60)
        parts = [f"{d}d"] * bool(d) + [f"{hh}h"] * bool(d or hh) + [f"{m}min"] * bool(d or hh or m) + [f"{s}s"]
        return " ".join(parts)

    def ticker(self):
        n = 0
        while True:
            time.sleep(1.5)
            if time.time() < self.down_until:
                continue
            n += 1
            self.ents["text_sensor/Uptime"].update(value=self.uptime(), state=self.uptime())
            if n % 4 == 0:
                self.publish("text_sensor/Uptime")
                self.send("ping", json.dumps({"uptime": int(time.time() - self.boot)}))
            if not self.ents["binary_sensor/Cloud Connected"]["value"]:
                if n % 5 == 0:
                    self.log("W", "mqtt:374", "Disconnected: TCP disconnected")
                continue
            r = random.random()
            if r < 0.5:
                req = self.ents["sensor/Requests Handled"]
                req["value"] += 1
                req["state"] = str(req["value"])
                self.log("D", "gbb_dongle:212", f"Request {90000 + n}: 4 lines from cloud")
                self.log("V", "modbus_executor:88", "TX 01 03 02 4C 00 01 84 65")
                self.publish("sensor/Requests Handled")
            elif r < 0.55:
                self.log("W", "modbus_executor:140", "Line 3: no response within 1000 ms (slave 1)")
            else:
                self.log("D", "gbb_dongle:301", "Keepalive published to 48213/keepalive")


class Handler(BaseHTTPRequestHandler):
    dev: Device = None
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *a):
        pass

    def reply(self, code, body=b"", ctype="text/plain"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def down(self):
        if time.time() < self.dev.down_until:
            self.close_connection = True
            self.reply(503)
            return True
        return False

    def do_GET(self):
        path = urlsplit(self.path).path
        if path == "/":
            return self.reply(200, INDEX.encode(), "text/html")
        if path in ("/0.js", "/0.css"):
            f = WEBUI / ("panel.js" if path.endswith("js") else "panel.css")
            return self.reply(200, f.read_bytes(), "text/javascript" if path.endswith("js") else "text/css")
        if path == "/events":
            if self.down():
                return
            return self.events()
        self.reply(404)

    def events(self):
        dev = self.dev
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        self.close_connection = True
        q = queue.Queue()

        def emit(event, data):
            self.wfile.write(f"event: {event}\ndata: {data}\n\n".encode())
            self.wfile.flush()

        try:
            self.wfile.write(b"retry: 3000\n\n")
            emit("ping", json.dumps({"title": dev.args.title, "comment": "", "ota": True, "log": True,
                                     "lang": "en", "uptime": int(time.time() - dev.boot)}))
            for e in dev.ents.values():
                emit("state", json.dumps(e))
            with dev.lock:
                dev.clients.append(q)
            while True:
                item = q.get()
                if item is None:
                    break
                emit(*item)
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            with dev.lock:
                if q in dev.clients:
                    dev.clients.remove(q)

    def do_POST(self):
        dev = self.dev
        length = int(self.headers.get("Content-Length") or 0)
        upload = self.rfile.read(length) if length else b""
        if self.down():
            return
        u = urlsplit(self.path)
        if u.path == "/update":
            dev.log("I", "web_server.ota:120", f"OTA upload finished ({len(upload)} B)")
            self.reply(200, b"Update Successful!")
            threading.Timer(0.5, dev.reboot, (10.0,)).start()
            return
        parts = u.path.strip("/").split("/")
        if len(parts) != 3:
            return self.reply(404)
        domain, name, action = parts[0], unquote(parts[1]), parts[2]
        eid = f"{domain}/{name}"
        e = dev.ents.get(eid)
        if e is None:
            return self.reply(404)
        args = {k: v[0] for k, v in parse_qs(u.query, keep_blank_values=True).items()}
        if domain == "switch" and action in ("turn_on", "turn_off", "toggle"):
            v = (not e["value"]) if action == "toggle" else action == "turn_on"
            dev.set(eid, v, "ON" if v else "OFF")
        elif domain == "text" and action == "set":
            v = args.get("value", "")
            if e.get("mode") == 1:
                dev.set(eid, "", "********")
            else:
                dev.set(eid, v)
        elif domain == "number" and action == "set":
            dev.set(eid, args.get("value", ""))
        elif domain == "select" and action == "set" and args.get("option") in e["option"]:
            dev.set(eid, args["option"])
            dev.log("I", "gbb_dongle:147", "RS485: %s baud, parity %s" % (
                dev.ents["select/RS485 Baud Rate"]["value"], dev.ents["select/RS485 Parity"]["value"]))
        elif domain == "button" and action == "press":
            self.reply(200)
            dev.reboot()
            return
        elif domain == "update" and action == "install":
            dev.set(eid, e["value"], "INSTALLING")
            self.reply(200)
            threading.Timer(3, dev.reboot, (10.0,)).start()
            return
        else:
            return self.reply(404)
        self.reply(200)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("--line", choices=["factory", "ha", "dev"], default="factory")
    ap.add_argument("--ethernet", action="store_true", help="no WiFi sensors (Kamami, 8DI-8DO ETH)")
    ap.add_argument("--unconfigured", action="store_true", help="fresh device: no Plant Id or server")
    ap.add_argument("--version", default="2.1.0")
    ap.add_argument("--latest", default="2.1.0", help="version the update entity reports")
    ap.add_argument("--title", default="GbbDongle")
    args = ap.parse_args()
    Handler.dev = Device(args)
    threading.Thread(target=Handler.dev.ticker, daemon=True).start()
    srv = ThreadingHTTPServer((args.bind, args.port), Handler)
    srv.daemon_threads = True
    print(f"Mock GbbDongle on http://{args.bind}:{args.port}/ (serving {WEBUI})")
    srv.serve_forever()


if __name__ == "__main__":
    main()
