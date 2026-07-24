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

Board specifics live in `firmware/boards/*.yaml`, WiFi connectivity in
`firmware/common/wifi.yaml` — adding another board means writing one board
file plus a device yaml combining it with `firmware/common/base.yaml` (and
`common/wifi.yaml` for WiFi boards).

## Installation

1. Open the **[web installer](https://krzysztofhajdamowicz.github.io/GbbDongle/)**
   in Chrome/Edge, pick your board, connect it over USB and click Install.
2. Get the device online:
   - WiFi boards (Waveshare, LilyGo): join the `GbbDongle Setup` WiFi AP and
     configure your WiFi.
   - Kamami: unplug USB, mount the RS485 HAT, plug in Ethernet (PoE or 5 V) —
     the device gets an address via DHCP (`gbbdongle-kamami.local`).
3. Open the device's web UI and fill in the GbbOptimizer settings
   (MQTT Server, Plant Id, Plant Token — from your GbbOptimizer plant page),
   then press **Apply Settings (Restart)**. Settings are stored in flash;
   changes to cloud settings take effect after a restart.
4. Wire RS485 A/B to the inverter (default 9600 baud, 8N1 — configurable
   live, no restart needed).

The device also exposes the ESPHome native API, so it can be adopted into
Home Assistant; firmware auto-update is available through the `Firmware`
update entity (manifest hosted on GitHub Pages).

## Configuration entities

| Entity | Meaning |
|---|---|
| MQTT Server / MQTT Port | e.g. `gbboptimizer1-mqtt.gbbsoft.pl` : `8883` |
| Plant Id / Plant Token | from GbbOptimizer |
| Cloud Connection | master enable switch |
| TLS / TLS Skip CN Check | TLS is on by default (Certum Trusted Network CA + ISRG Root X1 compiled in) |
| RS485 Baud Rate / Parity | serial parameters, applied live |

Note: the Plant Token is masked in the web UI but visible to the Home
Assistant API, like any ESPHome text entity.

## Development

Requires [uv](https://docs.astral.sh/uv/) (`brew install uv`).

```sh
uv sync
uv run esphome config firmware/gbbdongle.yaml            # validate (Waveshare)
uv run esphome compile firmware/gbbdongle.yaml           # release image, Waveshare
uv run esphome compile firmware/gbbdongle-tcan485.yaml   # release image, T-CAN485
uv run esphome compile firmware/gbbdongle-kamami.yaml    # release image, Kamami
uv run esphome run firmware/gbbdongle-dev.yaml           # dev build, flash & logs
uv run esphome run firmware/gbbdongle-tcan485-dev.yaml   # dev build for T-CAN485
uv run esphome run firmware/gbbdongle-kamami-dev.yaml    # dev build for Kamami (no secrets needed)
```

Firmware configs are layered via ESPHome packages: `firmware/common/base.yaml`
(board- and transport-agnostic), `firmware/common/wifi.yaml` (WiFi
connectivity — left out on Ethernet boards), `firmware/boards/*.yaml`
(hardware specifics) and `firmware/common/dev.yaml` (verbose-logging dev
overlay). The WiFi dev variants additionally include `common/wifi-dev.yaml`,
which joins your WiFi from `firmware/secrets.yaml` (see
`secrets.yaml.example`).

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

Tagging `v*` builds the firmware for every supported board (matrix build),
attaches the binaries (`gbbdongle.*.bin` for the Waveshare board,
`gbbdongle-tcan485.*.bin` for the T-CAN485, `gbbdongle-kamami.*.bin` for the
Kamami) to the GitHub Release and deploys the web installer plus the
manifests to GitHub Pages.

Manifests are per-board — `manifest-<board>.json` (ESP Web Tools) and
`update-manifest-<board>.json` (OTA update entity) — because chip family
alone cannot distinguish the two plain-ESP32 boards (T-CAN485 and Kamami):
each install button and each device's `update_manifest_url` points at its
own manifest. The legacy shared `manifest.json`/`update-manifest.json`
(one entry per chip family: Waveshare + T-CAN485) are still generated so
devices flashed before the split keep seeing updates and migrate to their
per-board manifest with their next OTA.
