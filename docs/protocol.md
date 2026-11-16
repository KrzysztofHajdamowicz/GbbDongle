# GbbOptimizer cloud protocol (as implemented by GbbDongle)

Reverse-engineered from GbbConnect2 (`GbbEngine2/Server/JobManager-mqtt.cs`,
`GbbConnect2Protocol/Protocol.cs`) and documented in depth in the
[gbbconnect-go](https://github.com/KrzysztofHajdamowicz/gbbconnect-go) design
docs. GbbDongle is a faithful re-implementation of the *device* side on an
ESP32/ESP32-S3 board with a direct RS485 connection to the inverter.

## MQTT session

| Parameter | Value |
|---|---|
| Broker | `gbboptimizerX-mqtt.gbbsoft.pl`, port 8883, TLS with hostname verification, always on (host and port are runtime settings; trust anchors compiled in via `certificate_authority` in `base.yaml`) |
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

These are the only topics the device uses. ESPHome's own MQTT features are
switched off on this connection: no entity state/command topics, no
discovery, no `esphome/discover` / `esphome/ping`, no birth/will/log topics.
Once per boot, after connecting, the device publishes an empty retained
message to `<device_name>/text/plant_token/state` to delete the Plant Token
that firmware up to 2.0.x left there.

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
- A reply with a valid CRC but another slave address or function code than
  the request (a late answer to an earlier request, bus cross-talk) is not
  this line's result: it is logged and ignored, and the device keeps
  listening until `response_timeout`. A Modbus exception reply (function
  code | 0x80) matches and is returned unchanged, like GbbConnect2 does.
- A response that would not fit the JSON size cap (below), even without
  `LastLog`, is answered like a failing line instead of being dropped:
  walking back from the last line, results are removed and the first removed
  line gets `Error: Response too large`, until the document fits.
- Lines without a `Modbus` value are skipped (left untouched).
- The response is still published; the response itself is the acknowledgment.
- Cases that get **no** response: a malformed/unparseable `toDevice` payload,
  or valid JSON whose root is not an object (`[]`, `5`), is ignored; a
  payload over 16 KiB, or one whose `Lines` / `LinesOnNoInvSetup` array has
  more than 50 elements, is ignored the same way (the device logs a warning;
  real batches carry ~10 lines and a longer response could not fit the size
  cap below anyway); the
  request queue holds one pending request, so a newer request replaces a
  queued one that has not started yet; a response that still exceeds the
  JSON size cap with every line result removed (oversized `Tag`/`OrderId`
  echoes) is dropped.

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
| `LinesOnNoInvSetup` | `Line[]`; emergency Modbus commands to run if the cloud goes silent. Stored (replacing the previous set) keyed by `SubInverterSN` (trimmed like .NET `Trim()`; absent/empty = master); an **empty array clears** the stored set for that key, and so does an array in which no line carries a `Modbus` frame (such a set could never be executed). A set counts as unchanged when every line's `LineNo` and `Modbus` match — `Tag`/`Timestamp` are ignored, so the hourly re-send with fresh timestamps is not a change. |

Trigger (mirrors GbbConnect2): past minute 10 of the hour
(`emergency_minute_threshold`; minutes are taken from the SNTP clock's
UTC epoch — `ts % 3600` — which matches local time in whole-hour time zones;
the check is inert until the clock syncs), if an `IsInvSetup` message has been seen before but
none arrived since the top of the current hour, the device executes every
stored set on the RS485 bus. An `IsInvSetup` that arrives **before** the
first clock sync (MQTT can beat NTP after a power cycle) still arms the
check: its receive time is approximated with the sync moment, which can only
delay the trigger, never fire it early. Results are **only logged** (they land in
`LastLog`), never published to `fromDevice`. A triggered send cycle owns the
bus until every stored set has been executed in full: a cloud request that
arrives meanwhile waits in the single pending slot and runs afterwards. The
cloud's commands overwrite the emergency state, they never interleave with
it (GbbOptimizer author's intent; GbbConnect2's blocking `SendLines` gives
the same ordering).

GbbDongle deviations from the original:

- **Delivery confirmation + retry**: a set only counts as delivered (and is
  then cleared — "send once") when the inverter accepted every line that
  carries a frame: no line failed (timeout, CRC, …) and none was answered
  with a Modbus exception. Otherwise the set is kept and retried with exponential backoff
  (`emergency_retry_initial` 60 s doubling up to `emergency_retry_max`
  15 min) until it succeeds or a fresh `IsInvSetup` arrives, which cancels
  the pending retry (the cloud is back in charge). An `IsInvSetup` that
  arrives while a send cycle is running does **not** interrupt it: the sets
  still go out in full, and only an undelivered remainder is then left
  un-retried (the cloud's own setup supersedes it). If the set is replaced
  while it is being executed, a successful run does not clear the
  replacement — it stays stored and goes through the normal retry. The
  original sent blindly once per hour-miss.
- **Optional persistence**: the "Persist Emergency Commands" switch (default
  off) keeps the sets in NVS across reboots (written only when the content
  actually changes, to limit flash wear). Turning the switch off erases the
  NVS copy. Sets whose JSON would reach the 5120 B `build_json()` cap are
  not persisted: the NVS copy is erased (it no longer matches what is
  stored, and restoring it after a reboot could re-send a replaced or
  already delivered set), an error is logged, and the RAM copy stays
  complete and in use until the next reboot. A failed NVS read at boot
  keeps the stored blob for the next boot; only an unparseable one is
  erased. After a reboot restores sets, the
  hourly deadline counts from the first clock sync. The original kept the
  set in RAM only.
- `LinesOnNoInvSetup` is not echoed back in responses (GbbConnect2 echoes it
  only as a serializer side effect; the cloud ignores it).

## Pairing with GbbOptimizer

Instead of copying MQTT Server, Plant Id and Plant Token by hand, the owner
generates a one-time pairing code for the plant in GbbOptimizer (a prefix
of one or two letters and two groups of four, e.g. `J-EEH7-ABCD` or
`AB-EEH7-ABCD`) and pastes it into the dongle's web panel. The dongle
exchanges it for the plant's cloud settings (GbbOptimizer manual, MQTT
chapter, "GbbDongle Pairing"):

```
POST https://gbboptimizer.gbbsoft.pl/Callback/GbbDongleJoin
Content-Type: application/json

{"code":"J-EEH7-ABCD"}
```

| Answer | Meaning | Pairing Status |
|---|---|---|
| 200 `{"plantId":"G12345","plantName":"Dom","plantToken":"…","mqttServer":"gbboptimizer7-mqtt.gbbsoft.pl","mqttPort":8883}` | the plant's settings | `confirm` |
| 404 `{"error":"invalid_code"}` | unknown, already used or expired code | `invalid_code` |
| 429 | too many attempts | `rate_limited` |
| other status | | `server_error` |
| no answer (DNS, TCP, TLS, 15 s timeout) | | `network_error` |

How the dongle runs the exchange (`components/gbb_dongle/cloud_pairing.cpp`):

- The typed code is normalized before it is sent: whitespace (also
  no-break spaces) dropped, letters upper-cased, typographic dashes turned
  into `-`, and a code of 8 to 10 letters and digits regrouped as a prefix
  of up to two symbols and two groups of four (`jeeh7abcd` →
  `J-EEH7-ABCD`, `ab eeh7 abcd` → `AB-EEH7-ABCD`). Codes of another length
  (4 to 32 letters and digits) keep the dashes they were typed with.
  Anything else is refused locally (`invalid_format`) without a request.
- HTTPS with ESP-IDF's HTTP client on a FreeRTOS task of its own, so the
  TLS handshake never stalls the main loop or the RS485 timing. The server
  is verified against the same trust anchors as the broker
  (`certificate_authority`) with hostname verification; redirects are not
  followed. A build without `certificate_authority` reports `unavailable`
  and the panel hides pairing. The `User-Agent` is `GbbDongle/<version>`.
- Only the 200 body is read, up to 2 KiB, and it is checked before anything
  is shown: `plantId` must fit the Plant Id entity and contain only
  letters, digits, `-` and `_` (it becomes part of MQTT topics);
  `plantToken` must fit the Plant Token entity and be printable ASCII;
  `mqttServer` must be a host name under `.gbbsoft.pl` (the firmware also
  trusts ISRG Root X1, so a broker elsewhere could present a valid
  certificate and receive the token at the first login) and fit the MQTT
  Server entity; `mqttPort` must be a whole number 1–65535. `plantName` is
  display only: control and text-direction characters are dropped and it
  is cut to 64 bytes. A failed check gives `bad_response` and the reason in
  the log.
- The settings are not saved yet. Pairing Plant shows
  `<plantName> · <plantId> · <mqttServer>` and the panel asks for
  confirmation. **Pairing Confirm** writes MQTT Server, MQTT Port, Plant Id
  and Plant Token, turns Cloud Connection on, clears the stored emergency
  sets (RAM and NVS) when the Plant Id changes, and restarts the device
  once the RS485 bus is idle (after 1 s, at most 30 s). No new cloud
  request or emergency set starts meanwhile. **Pairing Cancel**, a new
  code, or 10 minutes without confirmation (`expired`) discard the result.
- The code and the token never go to the log; the Plant Id and the broker
  do.
- If GbbOptimizer issues a new Plant Token for the pairing, the token
  changes on the server whether or not the result is confirmed on the
  dongle: Cancel only keeps the dongle's old settings.

Pairing Status values: `idle`, `working`, `confirm`, `applied`,
`invalid_format`, `invalid_code`, `rate_limited`, `network_error`,
`server_error`, `bad_response`, `expired`, `unavailable`. The REST API
drives the same flow (Factory Image: with the panel login):

```
curl -X POST 'http://<dongle>/text/Pairing%20Code/set?value=J-EEH7-ABCD'
curl 'http://<dongle>/text_sensor/Pairing%20Status'
curl 'http://<dongle>/text_sensor/Pairing%20Plant'
curl -X POST 'http://<dongle>/button/Pairing%20Confirm/press'
```

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
