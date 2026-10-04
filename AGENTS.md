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
| `firmware/common/base.yaml` | Board- and transport-agnostic package: api (keyless encryption), web_server, esphome + web_server OTA, uart skeleton, mqtt placeholder, SNTP time, the `gbb_dongle:` block, all config entities, compiled-in CA certs, `esphome.min_version` |
| `firmware/common/wifi.yaml` | WiFi connectivity package: `wifi:` + setup AP, `captive_portal:`, `improv_serial:`, WiFi Signal / IP Address entities. Ethernet boards simply don't include it (`ethernet:` conflicts with `wifi:` in ESPHome) |
| `firmware/common/factory.yaml` | Factory-line overlay: `api: !remove`, digest web auth, the Admin Password entity (rotates web + esphome-OTA password), includes `updates.yaml` |
| `firmware/common/ha.yaml` | HA-compatible-line overlay: `dashboard_import` pointing at the import target, includes `updates.yaml` |
| `firmware/common/updates.yaml` | Self-update for the published images only: `http_request` OTA + update entity + the spread auto-install script |
| `firmware/common/dev.yaml` | Dev overlay: verbose logging + local component source |
| `firmware/common/wifi-dev.yaml` | Dev overlay for WiFi boards: joins WiFi from `firmware/secrets.yaml` (gitignored; copy the repo-root `secrets.yaml.example`) |
| `firmware/boards/*.yaml` | One package per board (core file + transport overlays for the 8DI-8DO): chip/flash/psram, UART pins, RS485 direction control, ethernet, BLE provisioning |
| `firmware/gbbdongle*.yaml` | Device entrypoints combining the packages above via `packages:` — per board an import target (`<config>.yaml`), the two published images (`-factory`, `-ha`) and a `-dev` variant; `gbbdongle-bench.yaml` is a bench-only overlay |
| `static/` | GitHub Pages web installer (`index.html` + `img/` board photos), deployed by the release workflow |
| `docs/protocol.md` | **Authoritative** cloud-protocol description — read it before touching protocol code |
| `tools/` | `cloud_roundtrip.py` (plays the cloud against a dongle on a local broker), `bench/` (pymodbus RTU slave standing in for the inverter on a USB-RS485 adapter, an emergency-set test request and the bench procedure README) and `busscan/` (one-shot Go diagnostic that scans Modbus addresses through a dongle, Polish README, no CI) |
| `.github/workflows/` | `ci.yaml` (import-target validation + compile matrix), `release.yaml` (build, manifests, SBOM, attestations, Pages deploy), `dependabot-auto-merge.yml` |

### Package layering

A device yaml is just a `packages:` list; later packages override earlier
ones, list items concatenate:

| Entrypoint | Packages |
|---|---|
| `gbbdongle.yaml` (Waveshare), `gbbdongle-tcan485.yaml` | base + wifi + board |
| `gbbdongle-kamami.yaml` | base + board (Ethernet lives in the board file) |
| `gbbdongle-8di8do-wifi.yaml` | base + wifi + board core + wifi overlay (`boards/waveshare-esp32-s3-poe-eth-8di-8do{,-wifi}.yaml`) |
| `gbbdongle-8di8do-eth.yaml` | base + board core + eth overlay (`…-eth.yaml`, W5500 Ethernet) |
| `<config>-factory.yaml` | `<config>.yaml` + `common/factory.yaml`; sets `admin_password` and `update_manifest_url` |
| `<config>-ha.yaml` | `<config>.yaml` + `common/ha.yaml`; sets `update_manifest_url` |
| `<config>-dev.yaml` | the same packages as `<config>.yaml` + dev (+ wifi-dev on WiFi boards) |
| `gbbdongle-bench.yaml` | `gbbdongle-dev.yaml` + `emergency_minute_threshold: 1` (bench-test the emergency trigger minutes after the hour; not shipped, not in CI) |

`firmware/<config>.yaml` is the **import target**: the ESPHome dashboard
fetches it via `github://…@main` when adopting a device running the HA
image, so it must stay self-contained (no `updates.yaml`, no local paths).
The published images are always built from the `-factory`/`-ha` wrappers.

Adding a board = one `firmware/boards/<board>.yaml` + the import target,
`-factory`, `-ha` and `-dev` entrypoints (each wrapper with its own
`update_manifest_url`) + the CI `validate-import-targets` loop and both
build matrices + a row in `release.yaml`'s `BOARDS` table + a card in
`static/index.html` + a row in `README.md`.

### Firmware lines

Every board ships two images, selectable in the web installer:

- **Factory** (`-factory`, the default product): no native API
  (`api: !remove`), web UI behind digest auth `admin` / `${admin_password}`
  (default `admin`). The Admin Password text entity rotates both the web and
  the esphome-OTA password at runtime; an empty value restores the compiled
  default (`web_server_base` keeps a raw pointer, see the lambda comments in
  `common/factory.yaml`). Artifacts keep the legacy unsuffixed basenames.
- **HA-compatible** (`-ha`): keyless `api: encryption:` (key provisioned by
  the first client), `dashboard_import` for adoption, no passwords. After
  adoption the user's dashboard builds the import target — bring your own
  support, and no more self-updates from our manifests.

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
- **Waveshare ESP32-S3-POE-ETH-8DI-8DO** (`gbbdongle-8di8do-wifi.*` /
  `gbbdongle-8di8do-eth.*`): ESP32-S3, PSRAM, isolated RS485 with **manual**
  direction (TX=GPIO17, RX=GPIO18, `flow_control_pin` GPIO21 — without it
  the driver stays enabled and the receiver off; bench-verified). One board,
  two firmware variants (`wifi:` and `ethernet:` are mutually exclusive): the board
  package is split into a core file plus a `-wifi` overlay (esp32_improv) and
  an `-eth` overlay (W5500 SPI Ethernet — the ethernet component owns spi2,
  never add an `spi:` block on it). WROOM-1U module: WiFi needs the external
  SMA antenna. Both variants are ESP32-S3 ⇒ keep them out of the legacy
  manifests (that slot belongs to `gbbdongle`). The onboard 8×DI/8×DO
  (TCA9554), RGB LED, buzzer, RTC and TF card are deliberately not exposed.

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
- `emergency_store.{h,cpp}` — the emergency ("last will") command sets
  received via `LinesOnNoInvSetup`, keyed by `SubInverterSN`, optionally
  persisted to NVS as one JSON blob (raw `nvs_set_blob`, hash-guarded to
  limit flash wear). Storage only.
- `emergency_manager.{h,cpp}` — the emergency state machine (EMPTY → ARMED
  → QUEUED → EXECUTING → BACKOFF): hourly InvSetup deadline, send cycle over
  the stored sets, delivery confirmation and retry backoff. Owns the
  `EmergencyStore`; `gbb_dongle.cpp` only arbitrates the bus (a running
  emergency cycle first via `wants_bus()` / `start_next_set()`, then the
  pending cloud request — a cycle is never interrupted) and routes executor
  results with `header.emergency` back to it. See docs/protocol.md.
- `modbus_executor.{h,cpp}` — non-blocking state machine (IDLE → GAP →
  TRANSMIT → RX_WAIT → DONE) that executes one request's `Lines` on the bus,
  one frame at a time. Never block the ESPHome main loop here.
- `log_ring_buffer.{h,cpp}` — ring buffer (PSRAM when available, 64 KB
  default, 8 KB on no-PSRAM boards) fed by a logger hook; serves the
  incremental `LastLog` protocol feature.
- `__init__.py` — config schema. Key knobs: `flow_control_pin` (manual RS485
  direction; omit for auto-direction transceivers), `response_timeout`
  (1000 ms), `read_gap` (100 ms), `write_gap` (3000 ms), `log_buffer_size`,
  `time_id` (SNTP clock for the emergency check), `emergency_persist_id`,
  `emergency_minute_threshold` (10) + `emergency_retry_initial`/`_max`
  (60 s / 15 min; `gbbdongle-bench.yaml` lowers the threshold to bench-test
  the trigger without waiting an hour),
  `certificate_authority`, and the `*_id` wiring of the config entities
  declared in `base.yaml`. `version:` defaults to `auto` → resolved from
  `git describe`; release builds stamp it via `esphome -s version X.Y.Z`.
  `client_environment:` (fed `GbbDongle/${device_name}` in `base.yaml`) plus
  the optional `wifi_signal_db_id`/`wifi_signal_percent_id`/`ip_address_id`/
  `uptime_text_id` entities feed the `Client*` fields of fromDevice
  responses (WiFi ids wired in `common/wifi.yaml`, IP on Ethernet boards in
  the board file; missing WiFi sensors ⇒ ClientInfo says "Ethernet").

## MQTT protocol

The dongle is an MQTT-to-Modbus gateway: the cloud publishes batches of raw
Modbus RTU frames, the device executes them on the RS485 bus and publishes
the responses back. The full wire format (session parameters, topics,
payload schema, error semantics, timing rules) is documented in
**`docs/protocol.md`** — read it before touching protocol code, and treat it
plus the GbbConnect2 sources as the spec: the error semantics mirror
GbbConnect2 exactly, don't "improve" them.

## OTA / web-installer manifests

Published images (both lines, via `common/updates.yaml`) self-update: the
`http_request` update entity polls `update_manifest_url` (substitution, set
in each `-factory`/`-ha` wrapper) every 6 h, and on each newly detected
release `on_update_available` runs the `auto_install_firmware` script, which
waits a random 0–5 h (fleet spread) and installs only if
`update.is_available` still holds (a rolled-back manifest cancels it).

Manifests are **per-board and per-line** — `manifest-<suffix>[-ha].json`
(ESP Web Tools) and `update-manifest-<suffix>[-ha].json` (OTA), `<suffix>`
from the `BOARDS` table — because chipFamily alone cannot distinguish the
two plain-ESP32 boards (T-CAN485 vs Kamami) nor the three ESP32-S3 images.
The legacy shared `manifest.json`/`update-manifest.json` (Waveshare +
T-CAN485 only, keyed by chip family, Factory line) are still generated for
devices flashed before the split; they migrate to their per-board URL with
their next OTA — which also means pre-split devices land on the Factory
line. **Never add a second `ESP32` (or `ESP32-S3`) entry to the legacy
update manifest** — the update entity takes the first chipFamily match. All
manifests are generated by a board-table loop in `release.yaml`'s "Assemble
site and manifests" step; nothing is committed to the repo.

## Development commands

```sh
uv sync
uv run esphome config firmware/<variant>.yaml     # validate
uv run esphome compile firmware/<variant>.yaml    # build
uv run esphome run firmware/<variant>-dev.yaml    # flash + logs
```

`base.yaml` defaults `external_components` to `github://…@main` (required
for dashboard adoption). Non-`-dev` builds against the working-tree
component need `-s external_components_source ../components` (CI/release
pass it); the `-dev`/bench variants get the local override from
`common/dev.yaml`.

Configs: `gbbdongle`, `gbbdongle-tcan485`, `gbbdongle-kamami`,
`gbbdongle-8di8do-wifi`, `gbbdongle-8di8do-eth` (each + `-factory`, `-ha`,
`-dev`). WiFi dev variants need `firmware/secrets.yaml` (copy from the
repo-root `secrets.yaml.example`); the Ethernet dev variants (Kamami,
8di8do-eth) do not. CI validates the five import targets standalone and
compiles all ten published images (5 boards × factory/ha); on pull requests
it also uploads the OTA images as artifacts for testers. When touching
`release.yaml`'s assemble step, dry-run it locally: extract the `run:`
block, create dummy `site/firmware/*.bin` files, set
`GITHUB_REF_NAME`/`GITHUB_REPOSITORY`, run under bash and `jq .` the
resulting JSON.

Testing without cloud/inverter: local mosquitto with TLS off +
`tools/cloud_roundtrip.py` (publishes a captured `toDevice` request, prints
the response; paho-mqtt and pymodbus come from the `dev` uv group, installed
by default) + `tools/bench/modbus_slave.py` on a USB-RS485 adapter standing
in for the inverter. Procedure and scenarios: `tools/bench/README.md`.
`tools/busscan` plays the cloud against real inverters to list the Modbus
addresses on the bus.

## Dependencies

`esphome` is pinned exactly in `pyproject.toml`; `base.yaml` carries
`esphome.min_version` for adopted configs. Dependabot opens weekly grouped
PRs (`python-toolchain` for uv, `github-actions`) with a 7-day cooldown, and
`dependabot-auto-merge.yml` squash-merges minor/patch bumps once CI is
green. Green CI only proves that everything **compiles** — an esphome bump
lands on `main` without any runtime check, so bench-test it (MQTT over TLS,
a Modbus round trip, live baud change, Factory admin-password rotation, an
update check) before tagging the next `v*` release. Tagging is the only step
that ships firmware to devices.

## Conventions

- Comments explain constraints the code can't show (why a pin must be high,
  why a setting is a placeholder), not what the next line does.
- Entity names/ids are stable API — renaming them churns Home Assistant
  entities and breaks user dashboards.
- Artifact/device names are stable API too: `gbbdongle.*` = Waveshare, keep
  it that way for fielded-device manifest URLs.
