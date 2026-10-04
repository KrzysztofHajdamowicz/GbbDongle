# Bench testing without cloud or inverter

Three processes on the Mac stand in for GbbOptimizer and the inverter:

| Role | Tool |
|---|---|
| Cloud broker | local `mosquitto` (plain MQTT, anonymous) |
| Cloud | `tools/cloud_roundtrip.py` — publishes one `toDevice` request, prints the `fromDevice` response |
| Inverter | `tools/bench/modbus_slave.py` — pymodbus RTU slave on a USB-RS485 adapter wired A/B to the dongle |

Plus the dongle itself, flashed with `firmware/gbbdongle-bench.yaml`
(gbbdongle-dev + `emergency_minute_threshold: 1`, so the hourly emergency
trigger fires at minute 2 of the next hour instead of minute 11).

## Setup

1. Broker — mosquitto 2.x binds to localhost only by default, so give it a
   config that listens on the LAN and allows anonymous clients:

   ```sh
   printf 'listener 1883 0.0.0.0\nallow_anonymous true\n' > /tmp/mosq-bench.conf
   mosquitto -c /tmp/mosq-bench.conf
   ```

   The dongle must be able to reach the Mac's LAN IP on 1883 (mind VLAN
   isolation between an IoT subnet and the Mac, and the macOS application
   firewall).

2. Dongle — flash and keep the serial log open:

   ```sh
   uv run esphome run firmware/gbbdongle-bench.yaml --device /dev/cu.usbmodemXXXX
   ```

   Point it at the broker through its web UI or the REST API (entity
   *names*, URL-encoded; POSTs need an empty body):

   ```sh
   D=http://<dongle-ip>
   curl -X POST -d '' "$D/text/MQTT%20Server/set?value=<mac-lan-ip>"
   curl -X POST -d '' "$D/number/MQTT%20Port/set?value=1883"
   curl -X POST -d '' "$D/text/Plant%20Id/set?value=TEST1"
   curl -X POST -d '' "$D/text/Plant%20Token/set?value=TEST1"
   curl -X POST -d '' "$D/switch/TLS/turn_off"
   curl -X POST -d '' "$D/button/Apply%20Settings%20(Restart)/press"
   ```

3. Inverter — start the slave (defaults: unit 1, 9600 8N1, prints every
   frame):

   ```sh
   uv run python tools/bench/modbus_slave.py --port /dev/cu.usbserial-XXXX
   ```

## Scenarios

- **Cloud round trip** — the captured request, five register reads answered
  by the slave:

  ```sh
  uv run python tools/cloud_roundtrip.py --broker 127.0.0.1 --plant-id TEST1
  ```

- **Emergency set** — `IsInvSetup` plus a two-line `LinesOnNoInvSetup`
  (two FC06 writes to unit 1) and one SOC read (register 588, the slave
  answers 42):

  ```sh
  uv run python tools/cloud_roundtrip.py --broker 127.0.0.1 --plant-id TEST1 \
      --request-file tools/bench/emergency_request.json
  ```

  Expected log: `Stored emergency command set for master (2 line(s))`; the
  *Emergency Sets Stored* sensor goes to 1 within a minute.

- **Hourly trigger** — with no `IsInvSetup` since the top of the hour, the
  set goes out at minute 2 (bench threshold): `No InvSetup from
  GbbOptimizer this hour; sending the emergency command set(s)`, then
  `delivered; cleared` and `All emergency command sets delivered`. Stop the
  slave beforehand to exercise the no-response path instead: `got no
  response from the inverter`, `retrying in 60 s` (BACKOFF, doubling up to
  15 min); start the slave again and the retry delivers.

- **Persistence** — turn *Persist Emergency Commands* on (`Emergency sets
  persisted to NVS`), press restart: `Restored 1 emergency set(s) … from
  NVS` at boot and `Clock synced; hourly emergency check armed (sets
  restored from NVS)` on the first check. A software restart keeps the
  system clock (ESP-IDF stores it in RTC registers), so the check is live
  immediately; only a power cycle waits for SNTP.

- **Cancel** — send the emergency request again while a send is pending or
  in BACKOFF: `InvSetup received; cancelling the pending emergency send`.

Register frames carry a Modbus CRC; to craft new ones:

```sh
python3 -c 'import sys;b=bytes.fromhex(sys.argv[1]);c=0xFFFF
for x in b:
    c^=x
    for _ in range(8): c=(c>>1)^0xA001 if c&1 else c>>1
print((b+bytes([c&255,c>>8])).hex().upper())' 010600F80001
```
