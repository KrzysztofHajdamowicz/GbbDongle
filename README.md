# GbbDongle

ESPHome-based firmware that connects a photovoltaic inverter (Deye and
friends) directly to the [GbbOptimizer](https://gbboptimizer.gbbsoft.pl/)
cloud — a drop-in hardware replacement for the GbbConnect2 Windows/Docker
application. Runs on off-the-shelf ESP32 RS485 boards (see
[Supported boards](#supported-boards)).

The dongle plugs into the inverter's RS485 Modbus port and acts as a
stateless proxy: GbbOptimizer sends batches of raw Modbus RTU frames over
MQTT, the dongle executes them on the bus and sends the responses back. See
[docs/protocol.md](docs/protocol.md) for the protocol details.

## Supported boards

| Board | Chip | RS485 wiring | Notes |
|---|---|---|---|
| [Waveshare ESP32-S3-RS485-CAN](https://www.waveshare.com/esp32-s3-rs485-can.htm) | ESP32-S3, 16 MB flash, 8 MB PSRAM | TX=GPIO17, RX=GPIO18, direction=GPIO21 | Full feature set incl. BLE provisioning |
| [LilyGo T-CAN485](https://github.com/Xinyuan-LilyGO/T-CAN485) | ESP32, 4 MB flash, no PSRAM | TX=GPIO22, RX=GPIO21, auto-direction | 8 KB log buffer, no BLE provisioning (WiFi setup via USB/AP) |
| [Kamami KAmod ESP32 ETH+PoE](https://wiki.kamamilabs.com/index.php?title=KAmod_ESP32_ETH_POE) + [KAmodRPi UART RS485 ISO](https://wiki.kamamilabs.com/index.php/KAmodRPi_UART_RS485_ISO_(PL)) HAT | ESP32, 4 MB flash, no PSRAM | TX=GPIO1, RX=GPIO3 (UART0), auto-direction (MAX13487, isolated) | Ethernet-only (LAN8742, PoE); no WiFi/AP/BLE; serial logging disabled |
| [Waveshare ESP32-S3-POE-ETH-8DI-8DO](https://www.waveshare.com/esp32-s3-poe-eth-8di-8do.htm) | ESP32-S3, 16 MB flash, 8 MB PSRAM | TX=GPIO17, RX=GPIO18, direction control=GPIO21 (isolated transceiver, manual DE/RE) | Two firmware variants: WiFi (BLE provisioning; needs the external SMA antenna) or Ethernet-only (W5500, PoE); onboard DI/DO not used |

On the T-CAN485 the RS485 transceiver is auto-direction; the firmware drives
its enable pins (GPIO16 5 V booster, GPIO17 auto-direction, GPIO19 enable)
high at boot.

The Kamami board carries the RS485 interface on a Raspberry Pi-compatible
2×20 header (KAmodRPi UART RS485 ISO HAT). Its caveats:

- The HAT sits on **UART0 (GPIO1/GPIO3) — the same pins as the CH340 USB
  converter**. Never use USB and the HAT at the same time (electrical
  conflict); flash over USB with the HAT removed, then unplug USB before
  mounting the HAT.
- The ESP32 ROM bootloader prints on GPIO1 at every boot, so devices on the
  bus see a short burst of garbage (invalid CRC — harmless, Modbus slaves
  ignore it). Serial logging is disabled in the firmware; logs are available
  via the web UI and the native API.
- Ethernet-only: `wifi:` (and the setup AP / improv provisioning) is not
  compiled in. The device gets an address via DHCP and is reachable as
  `gbbdongle-kamami.local`.

The Waveshare ESP32-S3-POE-ETH-8DI-8DO has both WiFi and Ethernet, which are
mutually exclusive in ESPHome — pick one of the two firmware variants at
flash time:

- **WiFi variant** (`gbbdongle-8di8do-wifi`): BLE + serial provisioning. The
  WROOM-1U module has **no onboard antenna** — attach the external SMA
  antenna or WiFi won't work.
- **Ethernet variant** (`gbbdongle-8di8do-eth`): W5500 over SPI, powered via
  PoE (802.3af), USB or the 7–36 V terminal; DHCP, reachable as
  `gbbdongle-8di8do-eth.local`. No WiFi/AP/BLE compiled in.
- The board's 8 digital inputs, 8 digital outputs, RGB LED, buzzer, RTC and
  TF slot are deliberately not exposed — this is dongle-only firmware.

Board specifics live in `firmware/boards/*.yaml`, WiFi connectivity in
`firmware/common/wifi.yaml` — adding another board means writing one board
file plus a device yaml combining it with `firmware/common/base.yaml` (and
`common/wifi.yaml` for WiFi boards).

## Installation

Every board ships in two firmware flavours, selectable in the web installer:

- **Factory Image** (default) — the standalone GbbDongle product. No Home
  Assistant API; the web dashboard and firmware uploads are protected by a
  password (default `admin` / `admin`) which you can change via the
  **Admin Password** entity on the device's web page. **Factory Reset**
  restores the defaults. Firmware auto-updates through the `Firmware` update
  entity (manifest hosted on GitHub Pages).
- **Home Assistant-compatible** — exposes the ESPHome native API and
  broadcasts an adoption offer to the ESPHome dashboard. The API encryption
  key is generated and persisted on the first connection (until then the
  API accepts unprovisioned connections — standard ESPHome factory-image
  behaviour). After adoption your ESPHome dashboard compiles the
  configuration from this repo (`firmware/<config>.yaml`) with a fresh
  encryption key, and further updates are yours to build — bring your own
  support. No web/OTA passwords: add your own in the adopted config if you
  want them.

1. Open the **[web installer](https://krzysztofhajdamowicz.github.io/GbbDongle/)**
   in Chrome/Edge, pick your board and flavour, connect it over USB and
   click Install.
2. Get the device online:
   - WiFi boards (Waveshare boards — WiFi variant, LilyGo): In web installer choose an option to configure Wi-Fi or join the
     `GbbDongle Setup` WiFi AP and configure your WiFi.
   - Kamami: unplug USB, mount the RS485 HAT, plug in Ethernet (PoE or 5 V) —
     the device gets an address via DHCP (`gbbdongle-kamami.local`).
   - Waveshare 8DI-8DO (Ethernet variant): plug in Ethernet (PoE, USB or
     7–36 V power) — the device gets an address via DHCP
     (`gbbdongle-8di8do-eth.local`).
3. Open the device's web UI (Factory Image: log in with `admin` / `admin`
   and set your own Admin Password) and fill in the GbbOptimizer settings
   (MQTT Server, Plant Id, Plant Token — from your GbbOptimizer plant page),
   then press **Apply Settings (Restart)**. Settings are stored in flash;
   changes to cloud settings take effect after a restart.
4. Wire RS485 A/B to the inverter (default 9600 baud, 8N1 — configurable
   live, no restart needed).

With the Home Assistant-compatible image the device appears as *Discovered*
in the ESPHome dashboard (click **Adopt**) and can also be added to Home
Assistant directly. If you changed nothing else, adoption works
out-of-the-box; the first upload is unauthenticated by design (the factory
state has no OTA password).

## Configuration entities

| Entity | Meaning |
|---|---|
| MQTT Server / MQTT Port | e.g. `gbboptimizer1-mqtt.gbbsoft.pl` : `8883` |
| Plant Id / Plant Token | from GbbOptimizer |
| Cloud Connection | master enable switch |
| TLS / TLS Skip CN Check | TLS is on by default (Certum Trusted Network CA + ISRG Root X1 compiled in) |
| RS485 Baud Rate / Parity | serial parameters, applied live |
| Admin Password | Factory Image only: changes the web-dashboard and OTA-upload password (login stays `admin`) |

Note: the Plant Token is masked in the web UI but — on the Home
Assistant-compatible image — visible to the native API, like any ESPHome
text entity.

## Development

Requires [uv](https://docs.astral.sh/uv/) (`brew install uv`).

```sh
uv sync
uv run esphome config firmware/gbbdongle.yaml              # validate (Waveshare, import target)
uv run esphome compile firmware/gbbdongle-factory.yaml     # Factory Image, Waveshare
uv run esphome compile firmware/gbbdongle-ha.yaml          # HA-compatible image, Waveshare
uv run esphome compile firmware/gbbdongle-tcan485-factory.yaml   # T-CAN485 (…-ha.yaml likewise)
uv run esphome compile firmware/gbbdongle-kamami-factory.yaml    # Kamami
uv run esphome compile firmware/gbbdongle-8di8do-wifi-factory.yaml  # 8DI-8DO WiFi variant
uv run esphome compile firmware/gbbdongle-8di8do-eth-factory.yaml   # 8DI-8DO Ethernet variant
uv run esphome run firmware/gbbdongle-dev.yaml             # dev build, flash & logs
uv run esphome run firmware/gbbdongle-tcan485-dev.yaml     # dev build for T-CAN485
uv run esphome run firmware/gbbdongle-kamami-dev.yaml      # dev build for Kamami (no secrets needed)
uv run esphome run firmware/gbbdongle-8di8do-wifi-dev.yaml # dev build for 8DI-8DO WiFi
uv run esphome run firmware/gbbdongle-8di8do-eth-dev.yaml  # dev build for 8DI-8DO Ethernet (no secrets needed)
```

Firmware configs are layered via ESPHome packages. `firmware/<config>.yaml`
is the board's core config and the dashboard-adoption **import target**; the
published images wrap it: `<config>-factory.yaml` (adds
`firmware/common/factory.yaml`: strips `api:`, adds web auth + the Admin
Password entity + self-updates) and `<config>-ha.yaml` (adds
`firmware/common/ha.yaml`: `dashboard_import` + self-updates). Shared
packages: `firmware/common/base.yaml` (board- and transport-agnostic,
includes keyless `api: encryption:`), `firmware/common/updates.yaml`
(update entity, published images only), `firmware/common/wifi.yaml` (WiFi
connectivity — left out on Ethernet boards), `firmware/boards/*.yaml`
(hardware specifics) and `firmware/common/dev.yaml` (verbose-logging dev
overlay). The WiFi dev variants additionally include `common/wifi-dev.yaml`,
which joins your WiFi from `firmware/secrets.yaml` (see
`secrets.yaml.example`).

`base.yaml` pulls the `gbb_dongle` component from
`github://KrzysztofHajdamowicz/GbbDongle@main` by default so that adopted
configs (which only fetch the yaml, not the repo) resolve it. To build the
import targets or the `-factory`/`-ha` images against your working tree,
override the source:

```sh
uv run esphome -s external_components_source ../components compile firmware/gbbdongle-ha.yaml
```

The `-dev` (and bench) variants set this override in `common/dev.yaml`, so
they always compile the local component without extra flags.

### Using the component in your own ESPHome config

```yaml
external_components:
  - source: github://KrzysztofHajdamowicz/GbbDongle
    components: [gbb_dongle]
```

See `firmware/common/base.yaml` for the required `mqtt:` settings
(placeholder broker, `enable_on_boot: false`, disabled birth/will/log
messages) and the entity wiring.

### Testing without the cloud / inverter

Run a local mosquitto (`brew install mosquitto`), point the dongle at it with
TLS off, and publish a captured `toDevice` request; a Modbus slave simulator
(e.g. `pymodbus`) on a USB-RS485 adapter stands in for the inverter.

## Releases

Tagging `v*` builds both firmware lines for every supported board (matrix
build), attaches the binaries to the GitHub Release and deploys the web
installer plus the manifests to GitHub Pages. Factory Image binaries keep
the legacy basenames (`gbbdongle.*.bin`, `gbbdongle-tcan485.*.bin`, …) so
update manifests already in the field keep resolving — **devices installed
before the factory/HA split therefore auto-update onto the Factory line**
(no native API, web UI and OTA behind `admin`/`admin`). If such a device is
used with Home Assistant, reflash it once with the Home Assistant-compatible
image (`<config>-ha.*.bin`) from the web installer.

Manifests are per-board and per-line — `manifest-<board>[-ha].json` (ESP Web
Tools) and `update-manifest-<board>[-ha].json` (OTA update entity) — because
chip family alone cannot distinguish the two plain-ESP32 boards (T-CAN485
and Kamami), nor the three ESP32-S3 images (RS485-CAN and the two 8DI-8DO
variants): each install button and each variant's `update_manifest_url`
points at its own manifest. The legacy shared
`manifest.json`/`update-manifest.json` (one entry per chip family:
Waveshare + T-CAN485, Factory line) are still generated so devices flashed
before the split keep seeing updates and migrate to their per-board manifest
with their next OTA.
