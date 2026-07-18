#!/usr/bin/env python3
"""Simulate the GbbOptimizer cloud against a GbbDongle on a local broker.

Publishes a captured toDevice request (from gbbconnect-go's
example-communication.txt) and prints the fromDevice response. Point the
dongle at the same broker first (TLS off, matching Plant Id/Token — the
local mosquitto does not check credentials by default).

    brew install mosquitto && brew services start mosquitto
    uv run python tools/cloud_roundtrip.py --broker 192.168.1.10 --plant-id TEST1

The dongle must be able to reach the broker, so pass your Mac's LAN IP as
--broker in the dongle's "MQTT Server" entity too.
"""

import argparse
import json
import sys
import time

import paho.mqtt.client as mqtt

CAPTURED_REQUEST = {
    "OrderId": "roundtrip-test",
    "SendLastLog": 1,
    "Lines": [
        {"LineNo": 0, "Timestamp": 1784392208, "Modbus": "01030204000345B2"},
        {"LineNo": 1, "Timestamp": 1784392208, "Modbus": "0103020A00032471"},
        {"LineNo": 2, "Timestamp": 1784392208, "Modbus": "0103020F00033470"},
        {"LineNo": 3, "Timestamp": 1784392208, "Modbus": "0103021900015475"},
        {"LineNo": 4, "Timestamp": 1784392208, "Modbus": "0103024B0002B5A5"},
    ],
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--broker", default="localhost")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--plant-id", default="TEST1")
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--request-file", help="JSON file with a custom Header to send")
    args = parser.parse_args()

    request = CAPTURED_REQUEST
    if args.request_file:
        with open(args.request_file) as f:
            request = json.load(f)

    response: list[str] = []
    client = mqtt.Client(client_id="gbb-cloud-sim")

    def on_message(_client, _userdata, msg):
        response.append(msg.payload.decode())

    client.on_message = on_message
    client.connect(args.broker, args.port)
    client.subscribe(f"{args.plant_id}/ModbusInMqtt/fromDevice", qos=2)
    client.subscribe(f"{args.plant_id}/keepalive", qos=1)
    client.loop_start()

    payload = json.dumps(request)
    print(f"-> {args.plant_id}/ModbusInMqtt/toDevice: {payload}")
    client.publish(f"{args.plant_id}/ModbusInMqtt/toDevice", payload, qos=1)

    deadline = time.time() + args.timeout
    while time.time() < deadline and not response:
        time.sleep(0.2)
    client.loop_stop()

    if not response:
        print(f"No fromDevice response within {args.timeout}s", file=sys.stderr)
        return 1

    print("<- fromDevice:")
    print(json.dumps(json.loads(response[0]), indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
