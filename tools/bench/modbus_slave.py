#!/usr/bin/env python3
"""Stand-in inverter for bench tests: a Modbus RTU slave on a USB-RS485 adapter.

Answers every holding-register read/write for one slave address (default 1)
from a flat 1024-register table, so any frame GbbOptimizer (or
cloud_roundtrip.py) sends gets a valid response and the dongle sees the
"delivered" path instead of "Response timeout". Register 588 (Deye SOC) is
pre-set to 42 so a read returns something recognisable. Every frame on the
bus is printed as hex.

    uv run python tools/bench/modbus_slave.py --port /dev/cu.usbserial-XXXX

Match --baud/--parity to the dongle's RS485 Baud Rate / Parity entities
(defaults 9600 8N1).
"""

import argparse
import logging
import sys

from pymodbus import FramerType
from pymodbus.datastore import (
    ModbusDeviceContext,
    ModbusSequentialDataBlock,
    ModbusServerContext,
)
from pymodbus.server import StartSerialServer

REGISTER_COUNT = 1024
SOC_REGISTER = 588


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", required=True, help="serial device of the USB-RS485 adapter")
    parser.add_argument("--baud", type=int, default=9600)
    parser.add_argument("--parity", choices=["N", "E", "O"], default="N")
    parser.add_argument("--unit", type=int, default=1, help="Modbus slave address to answer as")
    parser.add_argument("--quiet", action="store_true", help="do not print bus frames")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(message)s")

    registers = [0] * REGISTER_COUNT
    registers[SOC_REGISTER] = 42
    # ModbusSequentialDataBlock uses 1-based addressing internally: starting
    # at 1 with REGISTER_COUNT values covers protocol addresses 0..1023.
    block = ModbusSequentialDataBlock(1, registers)
    device = ModbusDeviceContext(hr=block, ir=ModbusSequentialDataBlock(1, list(registers)))
    context = ModbusServerContext(devices={args.unit: device})

    def trace_packet(sending: bool, data: bytes) -> bytes:
        if not args.quiet:
            print(f"{'<- slave' if sending else '-> slave'} {data.hex().upper()}", flush=True)
        return data

    print(f"Modbus RTU slave {args.unit} on {args.port} @ {args.baud} 8{args.parity}1 (Ctrl-C to stop)", flush=True)
    StartSerialServer(
        context=context,
        framer=FramerType.RTU,
        port=args.port,
        baudrate=args.baud,
        parity=args.parity,
        stopbits=1,
        bytesize=8,
        trace_packet=trace_packet,
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
