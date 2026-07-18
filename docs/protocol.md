# GbbOptimizer cloud protocol (as implemented by GbbDongle)

Reverse-engineered from GbbConnect2 (`GbbEngine2/Server/JobManager-mqtt.cs`,
`GbbConnect2Protocol/Protocol.cs`) and documented in depth in the
[gbbconnect-go](https://github.com/KrzysztofHajdamowicz/gbbconnect-go) design
docs. GbbDongle is a faithful re-implementation of the *device* side on an
ESP32-S3 with a direct RS485 connection to the inverter.

## MQTT session

| Parameter | Value |
|---|---|
| Broker | `gbboptimizerX-mqtt.gbbsoft.pl`, port 8883, TLS |
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

JSON `Header` object, PascalCase keys, null fields omitted:

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
`fromDevice`, adding `GbbVersion` and `GbbEnvironment` (and `LastLog` when
`SendLastLog != 0`).

Error semantics (mirrors GbbConnect2):

- Per-line failure (timeout, bad CRC, bad hex): `Line.Error` is set, `Modbus`
  of that line **and every subsequent line** is removed, processing stops.
- The response is still published; the response itself is the acknowledgment.

Timing between commands: ≥100 ms after a read, ≥3000 ms after a write
(write = Modbus function ≥ 5 and ≠ 23).

## GbbDongle-specific behavior

- `SubInverterSN` is accepted but not used for routing: there is a single
  physical RS485 bus and the slave address inside each RTU frame already
  selects the target inverter. (GbbConnect2 used it to pick a different
  Solarman TCP dongle.)
- `LastLog` is served from a 64 KB ring buffer of recent ESPHome log lines
  held in PSRAM (incremental: each request returns what was logged since the
  previous one, capped at 8 KB).
- `LogLevel` (`OnlyErrors`/`Min`/`Max`) gates what gets recorded into that
  ring buffer and is persisted across reboots. It does not change the global
  ESPHome logger level.
