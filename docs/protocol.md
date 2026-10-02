# GbbOptimizer cloud protocol (as implemented by GbbDongle)

Reverse-engineered from GbbConnect2 (`GbbEngine2/Server/JobManager-mqtt.cs`,
`GbbConnect2Protocol/Protocol.cs`) and documented in depth in the
[gbbconnect-go](https://github.com/KrzysztofHajdamowicz/gbbconnect-go) design
docs. GbbDongle is a faithful re-implementation of the *device* side on an
ESP32/ESP32-S3 board with a direct RS485 connection to the inverter.

## MQTT session

| Parameter | Value |
|---|---|
| Broker | `gbboptimizerX-mqtt.gbbsoft.pl`, port 8883, TLS (host, port, TLS and TLS Skip CN Check are runtime settings; trust anchors compiled in via `certificate_authority` in `base.yaml`) |
| Client ID | `GbbConnect2_{PlantId}` |
| Username | `{PlantId}` |
| Password | `{PlantToken}` |
| LWT / retain | none |

Topics:

| Topic | Direction | QoS |
|---|---|---|
| `{PlantId}/ModbusInMqtt/toDevice` | cloud → device | 1 (subscribe) |
| `{PlantId}/ModbusInMqtt/fromDevice` | device → cloud | 2 (publish) |
| `{PlantId}/keepalive` | device → cloud, empty payload, every 60 s | 1 |

## Payload

JSON `Header` object, PascalCase keys, null fields omitted (an optional
per-line `Tag` string is passed through unchanged):

```json
{
  "OrderId": "YQd/lxzmEqbTnlA=",
  "SendLastLog": 1,
  "Lines": [
    { "LineNo": 0, "Timestamp": 1784392208, "Modbus": "01030204000345B2" }
  ]
}
```

`Modbus` is an uppercase-hex Modbus RTU frame **including CRC**. The device
executes each line on the RS485 bus in order and overwrites `Modbus` with the
raw response frame, then publishes the whole mutated header back on
`fromDevice`, adding the client-identity fields (and `LastLog` when
`SendLastLog != 0`):

| Field | Value |
|---|---|
| `ProtocolVersion` | `2` (Header.CURR_PROTOCOL_VERSION, protocol v2 as of GbbConnect2 2.0.0) |
| `ClientVersion` | firmware version, e.g. `0.1.1` |
| `ClientEnvironment` | `GbbDongle/<device_name>`, e.g. `GbbDongle/gbbdongle-kamami` |
| `ClientName` | `GbbDongle` |
| `ClientInfo` | live diagnostics, e.g. `Wi-Fi 65% (-78dBm), IP 192.168.1.23, uptime 3d 23h 48min 56s` (`Ethernet, IP …, uptime …` on Ethernet boards); omitted segments simply drop out |
| `GbbVersion`, `GbbEnvironment` | legacy names for version/environment, kept until the cloud fully migrates to the `Client*` fields |

Error semantics (mirrors GbbConnect2):

- Per-line failure: `Line.Error` is set, `Modbus` of that line **and every
  subsequent line** is removed, processing stops. `Error` strings:
  `Response timeout`, `Invalid CRC in response`, `Response too short` (reply
  under 4 bytes) and `Invalid Modbus hex string` (unparseable hex or a
  request frame under 4 bytes).
- Lines without a `Modbus` value are skipped (left untouched).
- The response is still published; the response itself is the acknowledgment.
- Cases that get **no** response: a malformed/unparseable `toDevice` payload is ignored; the
  request queue holds one pending request, so a newer request replaces a
  queued one that has not started yet; a response that exceeds the JSON
  size cap even without `LastLog` is dropped.

Timing: a line fails with `Response timeout` after 1000 ms without a
complete reply (`response_timeout`). Between commands the device waits
≥100 ms after a read and ≥3000 ms after a write (`read_gap`/`write_gap`;
write = Modbus function ≥ 5 and ≠ 23), measured from the end of the previous
command's reply or timeout — including across request boundaries, so the
first frame of the next request honours the gap left by the last one.

## Emergency command set ("last will", protocol v2)

Port of GbbConnect2 2.0.0 (commit c2d1f00). Two additional toDevice Header
fields:

| Field | Meaning |
|---|---|
| `IsInvSetup` | int; non-zero marks a message carrying inverter setup data. GbbOptimizer sends one during the first 10 minutes of every hour. |
| `LinesOnNoInvSetup` | `Line[]`; emergency Modbus commands to run if the cloud goes silent. Stored (replacing the previous set) keyed by `SubInverterSN` (trimmed like .NET `Trim()`; absent/empty = master); an **empty array clears** the stored set for that key. A set counts as unchanged when every line's `LineNo` and `Modbus` match — `Tag`/`Timestamp` are ignored, so the hourly re-send with fresh timestamps is not a change. |

Trigger (mirrors GbbConnect2): past minute 10 of the hour
(`emergency_minute_threshold`; minutes are taken from the SNTP clock's
UTC epoch — `ts % 3600` — which matches local time in whole-hour time zones;
the check is inert until the clock syncs), if an `IsInvSetup` message has been seen before but
none arrived since the top of the current hour, the device executes every
stored set on the RS485 bus. An `IsInvSetup` that arrives **before** the
first clock sync (MQTT can beat NTP after a power cycle) still arms the
check: its receive time is approximated with the sync moment, which can only
delay the trigger, never fire it early. Results are **only logged** (they land in
`LastLog`), never published to `fromDevice`, and normal cloud requests take
priority on the bus.

GbbDongle deviations from the original:

- **Delivery confirmation + retry**: a set only counts as delivered (and is
  then cleared — "send once") when the inverter answered at least one line.
  Otherwise the set is kept and retried with exponential backoff
  (`emergency_retry_initial` 60 s doubling up to `emergency_retry_max`
  15 min) until it succeeds or a fresh `IsInvSetup` arrives, which cancels
  pending sends (the cloud is back in charge) — including an in-flight
  batch, which is aborted at the next safe Modbus line boundary (never
  mid-frame; the current response/timeout completes first). If the set is
  replaced while it is being executed, a successful run does not clear the
  replacement — it stays stored and goes through the normal retry. The
  original sent blindly once per hour-miss.
- **Optional persistence**: the "Persist Emergency Commands" switch (default
  off) keeps the sets in NVS across reboots (written only when the content
  actually changes, to limit flash wear). Turning the switch off erases the
  NVS copy. Sets whose JSON would reach the 5120 B `build_json()` cap are
  not persisted (the previous NVS copy stays, a warning is logged; the RAM
  copy is complete and still used). After a reboot restores sets, the
  hourly deadline counts from the first clock sync. The original kept the
  set in RAM only.
- `LinesOnNoInvSetup` is not echoed back in responses (GbbConnect2 echoes it
  only as a serializer side effect; the cloud ignores it).

## GbbDongle-specific behavior

- `SubInverterSN` is accepted but not used for routing: there is a single
  physical RS485 bus and the slave address inside each RTU frame already
  selects the target inverter. (GbbConnect2 used it to pick a different
  Solarman TCP dongle.)
- `LastLog` is served from a ring buffer of recent ESPHome log lines
  (`log_buffer_size`: 64 KB in PSRAM by default, 8 KB in internal RAM on
  the no-PSRAM boards T-CAN485 and Kamami). It is incremental: each request
  returns what was logged since the previous one, capped at 3 KiB — when
  more is pending, only the newest 3 KiB is sent and the older lines are
  dropped. The cap is budgeted so the escaped log plus the rest of the
  response stays under esphome `json::build_json()`'s 5120 B truncation
  cap; an oversized response is re-sent without `LastLog` rather than
  published as truncated, invalid JSON. A single log message longer than a
  quarter of the buffer is truncated.
- `LogLevel` gates what gets recorded into that ring buffer: `OnlyErrors` =
  warnings and errors, `Min` = up to DEBUG (also the default until the cloud
  sets a level), `Max` = everything the ESPHome logger emits. The level is persisted across reboots
  and does not change the global ESPHome logger level.
