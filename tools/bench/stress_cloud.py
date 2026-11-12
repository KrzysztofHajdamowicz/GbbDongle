#!/usr/bin/env python3
"""Keep the dongle's RS485 bus busy with back-to-back cloud requests.

Bench load generator for firmware/gbbdongle-profile.yaml: plays the cloud
against a dongle on a local broker (like cloud_roundtrip.py), but sends the
next toDevice request as soon as the previous fromDevice arrives, for
--count rounds. Each request carries --lines holding-register reads (FC03,
--regs registers each, unit --unit) spread over the slave's 1024-register
table. Prints the round-trip time of each request and a summary, which
gives the bus time per line at the current baud rate and gaps.

    uv run python tools/bench/stress_cloud.py --broker 127.0.0.1 --plant-id TEST1 \\
        --count 100 --lines 10 --regs 60

Keep --regs at 60 or below with the FTDI adapter behind the Dell dock (it
stalls on ~255 B replies, see tools/bench/README.md).
"""

import argparse
import json
import statistics
import sys
import threading
import time

import paho.mqtt.client as mqtt


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def read_frame(unit: int, address: int, count: int) -> str:
    pdu = bytes([unit, 0x03, address >> 8, address & 0xFF, count >> 8, count & 0xFF])
    crc = crc16(pdu)
    return (pdu + bytes([crc & 0xFF, crc >> 8])).hex().upper()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--broker", default="localhost")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--plant-id", default="TEST1")
    parser.add_argument("--count", type=int, default=50, help="number of requests")
    parser.add_argument("--lines", type=int, default=10, help="Modbus lines per request")
    parser.add_argument("--regs", type=int, default=60, help="registers per read (1-125)")
    parser.add_argument("--unit", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=60.0, help="per-request timeout in seconds")
    args = parser.parse_args()
    if not 1 <= args.regs <= 125:
        parser.error("--regs must be 1..125")

    topic_to = f"{args.plant_id}/ModbusInMqtt/toDevice"
    topic_from = f"{args.plant_id}/ModbusInMqtt/fromDevice"
    arrived = threading.Event()
    last: dict = {}

    def on_message(_client, _userdata, msg):
        if msg.topic != topic_from:
            return
        try:
            last["body"] = json.loads(msg.payload)
        except ValueError:
            last["body"] = None
        arrived.set()

    client = mqtt.Client(client_id="gbb-cloud-stress")
    client.on_message = on_message
    client.connect(args.broker, args.port)
    client.subscribe(topic_from, qos=2)
    client.loop_start()
    time.sleep(0.5)

    span = max(1, 1024 - args.regs)
    rtts: list[float] = []
    failed_lines = 0
    for i in range(args.count):
        lines = [
            {"LineNo": n, "Timestamp": int(time.time()), "Modbus": read_frame(args.unit, (n * 97) % span, args.regs)}
            for n in range(args.lines)
        ]
        request = {"OrderId": f"stress-{i}", "Lines": lines}
        arrived.clear()
        start = time.monotonic()
        client.publish(topic_to, json.dumps(request), qos=1)
        if not arrived.wait(args.timeout):
            print(f"#{i}: no fromDevice within {args.timeout}s", file=sys.stderr)
            continue
        rtt = time.monotonic() - start
        body = last.get("body") or {}
        errors = sum(1 for line in body.get("Lines", []) if line.get("Error"))
        failed_lines += errors
        rtts.append(rtt)
        print(f"#{i}: {rtt * 1000:.0f} ms for {args.lines} line(s){f', {errors} failed' if errors else ''}", flush=True)
    client.loop_stop()

    if not rtts:
        print("No responses.", file=sys.stderr)
        return 1
    per_line = statistics.median(rtts) / args.lines * 1000
    print(
        f"\n{len(rtts)}/{args.count} responses, round trip median {statistics.median(rtts) * 1000:.0f} ms, "
        f"max {max(rtts) * 1000:.0f} ms; ~{per_line:.0f} ms of bus time per {args.regs}-register read; "
        f"{failed_lines} failed line(s)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
