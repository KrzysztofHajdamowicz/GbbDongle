#!/usr/bin/env python3
"""Simulate a GbbDongle (plus fake Deye inverters) against busscan.

Mirror image of tools/cloud_roundtrip.py: connects to the busscan broker,
subscribes to {PlantId}/ModbusInMqtt/toDevice and answers each Modbus frame as
if inverters were present on the configured slave addresses. Other addresses
get {"Error": "Response timeout"} with the Modbus field stripped, matching the
firmware's error semantics (docs/protocol.md).

    ./busscan                                # terminal 1
    uv run python tools/busscan/fake_dongle.py   # terminal 2

With --rest-stub it also serves the ESPHome web_server v3 endpoints busscan's
--dongle mode uses, so the save/apply/restore flow can be dry-run:

    uv run python tools/busscan/fake_dongle.py --rest-stub 6052 &
    ./busscan --dongle localhost:6052 --ip 127.0.0.1
"""

import argparse
import json
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, unquote, urlparse

import paho.mqtt.client as mqtt

DEVICES = {
    1: {"sn": "2301234501", "type": 0x0500, "proto": 0x0102, "soc": 85},
    2: {"sn": "2301234502", "type": 0x0500, "proto": 0x0102, "soc": 42},
}


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def with_crc(frame: bytes) -> bytes:
    c = crc16(frame)
    return frame + bytes([c & 0xFF, c >> 8])


def answer_frame(frame: bytes) -> bytes | None:
    """Return the response frame, or None for a bus timeout."""
    addr, fn = frame[0], frame[1]
    dev = DEVICES.get(addr)
    if dev is None:
        return None
    if fn != 0x03:
        return with_crc(bytes([addr, fn | 0x80, 0x01]))  # illegal function
    start = int.from_bytes(frame[2:4], "big")
    count = int.from_bytes(frame[4:6], "big")
    regs = []
    for r in range(start, start + count):
        if r == 0:
            regs.append(dev["type"])
        elif r == 1:
            regs.append(addr)
        elif r == 2:
            regs.append(dev["proto"])
        elif 3 <= r <= 7:
            sn = dev["sn"].encode()
            i = (r - 3) * 2
            regs.append(sn[i] << 8 | sn[i + 1])
        elif r == 588:
            regs.append(dev["soc"])
        else:
            regs.append(0)
    body = bytes([addr, 0x03, count * 2]) + b"".join(
        r.to_bytes(2, "big") for r in regs
    )
    return with_crc(body)


def handle_request(client: mqtt.Client, plant_id: str, payload: bytes) -> None:
    header = json.loads(payload)
    for line in header.get("Lines", []):
        frame = bytes.fromhex(line["Modbus"])
        print(f"   frame -> addr {frame[0]:3d} fn {frame[1]:#04x}", end="")
        response = answer_frame(frame)
        if response is None:
            print("  (timeout)")
            line["Error"] = "Response timeout"
            line.pop("Modbus", None)
            break  # firmware aborts the batch on the first error
        print(f"  -> {response.hex().upper()}")
        line["Modbus"] = response.hex().upper()
    header["ClientName"] = "GbbDongle"
    header["ClientVersion"] = "fake"
    client.publish(f"{plant_id}/ModbusInMqtt/fromDevice", json.dumps(header), qos=2)


# --- Optional ESPHome web_server v3 REST stub --------------------------------
# Mirrors the real firmware: URLs match the entity *name* (URL-escaped), and
# the number entity serializes its value as a JSON string.

REST_STATE = {
    ("text", "MQTT Server"): "gbboptimizer2-mqtt.gbbsoft.pl",
    ("number", "MQTT Port"): "8883",
    ("switch", "TLS"): True,
    ("switch", "Cloud Connection"): True,
}


class RestStub(BaseHTTPRequestHandler):
    def _reply(self, code: int, body: dict | None = None) -> None:
        self.send_response(code)
        self.end_headers()
        if body is not None:
            self.wfile.write(json.dumps(body).encode())

    def do_GET(self):  # noqa: N802
        parts = [unquote(p) for p in urlparse(self.path).path.strip("/").split("/")]
        if len(parts) == 2 and (parts[0], parts[1]) in REST_STATE:
            value = REST_STATE[(parts[0], parts[1])]
            self._reply(200, {"name_id": f"{parts[0]}/{parts[1]}", "value": value})
        else:
            self._reply(404)

    def do_POST(self):  # noqa: N802
        url = urlparse(self.path)
        parts = [unquote(p) for p in url.path.strip("/").split("/")]
        if parts == ["button", "Apply Settings (Restart)", "press"]:
            print("REST: apply/restart pressed; state:", dict(REST_STATE))
            self._reply(200)
            return
        if len(parts) == 3:
            key, action = (parts[0], parts[1]), parts[2]
            if key in REST_STATE:
                if action == "set":
                    value = parse_qs(url.query)["value"][0]
                    REST_STATE[key] = value
                elif action in ("turn_on", "turn_off"):
                    REST_STATE[key] = action == "turn_on"
                else:
                    self._reply(404)
                    return
                print(f"REST: {parts[0]}/{parts[1]} = {REST_STATE[key]}")
                self._reply(200)
                return
        self._reply(404)

    def log_message(self, *args):  # silence default request logging
        pass


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--broker", default="localhost")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--plant-id", default="TEST1")
    parser.add_argument("--rest-stub", type=int, metavar="HTTP_PORT",
                        help="also serve the ESPHome REST endpoints on this port")
    args = parser.parse_args()

    if args.rest_stub:
        server = ThreadingHTTPServer(("", args.rest_stub), RestStub)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        print(f"REST stub on http://localhost:{args.rest_stub}")

    client = mqtt.Client(client_id=f"GbbConnect2_{args.plant_id}")
    client.username_pw_set(args.plant_id, "fake-token")

    def on_message(_client, _userdata, msg):
        print(f"<- {msg.topic}")
        handle_request(client, args.plant_id, msg.payload)

    def on_connect(_client, _userdata, _flags, rc):
        print(f"fake dongle connected to {args.broker}:{args.port} as Plant Id {args.plant_id}; "
              f"inverters on addresses {sorted(DEVICES)}")
        client.subscribe(f"{args.plant_id}/ModbusInMqtt/toDevice", qos=1)

    client.on_message = on_message
    client.on_connect = on_connect
    # connect_async + loop_start keeps retrying until the busscan broker is up.
    client.connect_async(args.broker, args.port)
    client.loop_start()

    try:
        while True:
            time.sleep(60)
            client.publish(f"{args.plant_id}/keepalive", b"", qos=1)
    except KeyboardInterrupt:
        return 0


if __name__ == "__main__":
    sys.exit(main())
