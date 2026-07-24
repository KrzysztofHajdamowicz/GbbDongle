# AGENTS.md — orientation for AI agents working on this repo

## What this is

GbbDongle is ESPHome-based firmware that connects a photovoltaic inverter
(Deye and similar) directly to the [GbbOptimizer](https://gbboptimizer.gbbsoft.pl/)
cloud over MQTT — a drop-in hardware replacement for the GbbConnect2
Windows/Docker application. The dongle plugs into the inverter's RS485 Modbus
port and acts as a **stateless proxy**: the cloud sends batches of raw Modbus
RTU frames over MQTT, the device executes them on the bus and publishes the
responses back. All protocol intelligence lives in the cloud; the device only
executes frames.

Build system: **ESPHome** (not raw PlatformIO/ESP-IDF), driven via
[uv](https://docs.astral.sh/uv/). There is no `platformio.ini`; ESPHome
generates it internally.

## Repository layout

| Path | Contents |
|---|---|
| `components/gbb_dongle/` | The custom ESPHome external component: Python config schema (`__init__.py`) + C++ implementation |
| `firmware/common/base.yaml` | Board- and transport-agnostic package: api, web_server, ota + `http_request` update entity, uart skeleton, mqtt placeholder, the `gbb_dongle:` block, all config entities, compiled-in CA certs |
| `firmware/common/wifi.yaml` | WiFi connectivity package: `wifi:` + setup AP, `captive_portal:`, `improv_serial:`, WiFi Signal / IP Address entities. Ethernet boards simply don't include it (`ethernet:` conflicts with `wifi:` in ESPHome) |
| `firmware/common/dev.yaml` | Dev overlay: verbose logging only |
| `firmware/common/wifi-dev.yaml` | Dev overlay for WiFi boards: joins WiFi from `firmware/secrets.yaml` (gitignored; copy `secrets.yaml.example`) |
| `firmware/boards/*.yaml` | One package per board: chip/flash/psram, UART pins, RS485 direction control, ethernet, BLE provisioning |
| `firmware/gbbdongle*.yaml` | Device entrypoints combining the packages above via `packages:` (release + `-dev` variants) |
| `static/` | GitHub Pages web installer (`index.html` + `img/` board photos), deployed by the release workflow |
| `docs/protocol.md` | **Authoritative** cloud-protocol description — read it before touching protocol code |
| `.github/workflows/` | `ci.yaml` (config + compile matrix), `release.yaml` (build, manifests, SBOM, attestations, Pages deploy) |

### Package layering

A device yaml is just a `packages:` list; later packages override earlier
ones, list items concatenate:

| Entrypoint | Packages |
|---|---|
| `gbbdongle.yaml` (Waveshare), `gbbdongle-tcan485.yaml` | base + wifi + board |
| their `-dev` variants | base + wifi + board + dev + wifi-dev |
| `gbbdongle-kamami.yaml` | base + board (Ethernet lives in the board file) |
| `gbbdongle-kamami-dev.yaml` | base + board + dev |

Adding a board = one `firmware/boards/<board>.yaml` + entrypoint(s) + CI/release
matrix entries + a card in `static/index.html` + a row in `README.md`.

### Supported boards (gotchas included)

- **Waveshare ESP32-S3-RS485-CAN** (`gbbdongle.*` artifacts — the name is kept
  unsuffixed so update manifests already in the field keep resolving):
  ESP32-S3, PSRAM, manual RS485 direction via `flow_control_pin` GPIO21, BLE
  provisioning (`esp32_improv`).
- **LilyGo T-CAN485** (`gbbdongle-tcan485.*`): plain ESP32, no PSRAM
  (`log_buffer_size: 8192`), auto-direction transceiver but needs three GPIO
  enable switches (16/17/19) driven high.
- **Kamami KAmod ESP32 ETH+PoE + KAmodRPi UART RS485 ISO HAT**
  (`gbbdongle-kamami.*`): plain ESP32, Ethernet-only (LAN8742 via the
  `LAN8720` driver, PoE). **RS485 rides on UART0 (GPIO1/GPIO3)** — the same
  pins as the CH340 USB converter, so serial logging is disabled
  (`logger: baud_rate: 0`), there is no `improv_serial`, and USB must not be
  connected while the HAT is mounted. The ROM bootloader still prints on
  GPIO1 at boot (harmless garbage on the bus, invalid CRC).

## The gbb_dongle component

`components/gbb_dongle/` — an external component registered in `base.yaml`.
Files:

- `gbb_dongle.{h,cpp}` — orchestration. Runs at setup priority LATE.
  ESPHome's `mqtt:` block is a placeholder (`enable_on_boot: false`); the
  component injects the runtime-configured broker/credentials (from template
  text/number/switch entities persisted in NVS) into the MQTT client at
  setup, then calls `enable()` once the network is up. **Cloud settings
  changes require a restart** (esp-mqtt config is built once); RS485
  baud/parity changes apply live. Publishes a keepalive every 60 s.
- `gbb_protocol.{h,cpp}` — JSON parse/build of the `Header` payload
  (PascalCase keys, ArduinoJson).
- `modbus_executor.{h,cpp}` — non-blocking state machine (IDLE → GAP →
  TRANSMIT → RX_WAIT → DONE) that executes one request's `Lines` on the bus,
  one frame at a time. Never block the ESPHome main loop here.
- `log_ring_buffer.{h,cpp}` — ring buffer (PSRAM when available, 64 KB
  default, 8 KB on no-PSRAM boards) fed by a logger hook; serves the
  incremental `LastLog` protocol feature.
- `__init__.py` — config schema. Key knobs: `flow_control_pin` (manual RS485
  direction; omit for auto-direction transceivers), `response_timeout`
  (1000 ms), `read_gap` (100 ms), `write_gap` (3000 ms), `log_buffer_size`,
  `certificate_authority`, and the `*_id` wiring of the config entities
  declared in `base.yaml`. `version:` defaults to `auto` → resolved from
  `git describe`; release builds stamp it via `esphome -s version X.Y.Z`.

## MQTT protocol

The dongle is an MQTT-to-Modbus gateway: the cloud publishes batches of raw
Modbus RTU frames, the device executes them on the RS485 bus and publishes
the responses back. The full wire format (session parameters, topics,
payload schema, error semantics, timing rules) is documented in
**`docs/protocol.md`** — read it before touching protocol code, and treat it
plus the GbbConnect2 sources as the spec: the error semantics mirror
GbbConnect2 exactly, don't "improve" them.

## OTA / web-installer manifests

Firmware auto-updates via ESPHome's `http_request` update entity polling
`update_manifest_url` (substitution, set per entrypoint) every 6 h.
Manifests are **per-board** — `manifest-<board>.json` (ESP Web Tools) and
`update-manifest-<board>.json` (OTA) — because chipFamily alone cannot
distinguish the two plain-ESP32 boards (T-CAN485 vs Kamami). The legacy
shared `manifest.json`/`update-manifest.json` (Waveshare + T-CAN485 only,
keyed by chip family) are still generated for devices flashed before the
split; they migrate to their per-board URL with their next OTA. **Never add
a second `ESP32` entry to the legacy update manifest** — the update entity
takes the first chipFamily match. All manifests are generated by a
board-table loop in `release.yaml`'s "Assemble site and manifests" step;
nothing is committed to the repo.

## Development commands

```sh
uv sync
uv run esphome config firmware/<variant>.yaml     # validate
uv run esphome compile firmware/<variant>.yaml    # build
uv run esphome run firmware/<variant>-dev.yaml    # flash + logs
```

Variants: `gbbdongle`, `gbbdongle-tcan485`, `gbbdongle-kamami` (+ `-dev`).
WiFi dev variants need `firmware/secrets.yaml` (copy from
`secrets.yaml.example`); the Kamami dev variant does not. CI validates and
compiles all three release variants. When touching `release.yaml`'s assemble
step, dry-run it locally: extract the `run:` block, create dummy
`site/firmware/*.bin` files, set `GITHUB_REF_NAME`/`GITHUB_REPOSITORY`, run
under bash and `jq .` the resulting JSON.

Testing without cloud/inverter: local mosquitto with TLS off + a pymodbus
slave simulator on a USB-RS485 adapter (see README).

## Conventions

- Comments explain constraints the code can't show (why a pin must be high,
  why a setting is a placeholder), not what the next line does.
- Entity names/ids are stable API — renaming them churns Home Assistant
  entities and breaks user dashboards.
- Artifact/device names are stable API too: `gbbdongle.*` = Waveshare, keep
  it that way for fielded-device manifest URLs.
