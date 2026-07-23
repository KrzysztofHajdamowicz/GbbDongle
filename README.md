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

On the T-CAN485 the RS485 transceiver is auto-direction; the firmware drives
its enable pins (GPIO16 5 V booster, GPIO17 auto-direction, GPIO19 enable)
high at boot. Board specifics live in `firmware/boards/*.yaml` — adding
another board means writing one such file plus a device yaml combining it
with `firmware/common/base.yaml`.

## Installation

1. Open the **[web installer](https://krzysztofhajdamowicz.github.io/GbbDongle/)**
   in Chrome/Edge, connect the board over USB, click Install (the installer
   detects the chip and picks the right firmware variant).
2. Join the `GbbDongle Setup` WiFi AP and configure your WiFi.
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
uv run esphome run firmware/gbbdongle-dev.yaml           # dev build, flash & logs
uv run esphome run firmware/gbbdongle-tcan485-dev.yaml   # dev build for T-CAN485
```

Firmware configs are layered via ESPHome packages: `firmware/common/base.yaml`
(board-agnostic), `firmware/boards/*.yaml` (hardware specifics) and
`firmware/common/dev.yaml` (dev overlay). The dev variants join your WiFi from
`firmware/secrets.yaml` (see `secrets.yaml.example`) and log verbosely.

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
`gbbdongle-tcan485.*.bin` for the T-CAN485) to the GitHub Release and deploys
the web installer plus both manifests (ESP Web Tools + OTA update) to GitHub
Pages. Each manifest carries one entry per chip family, so both the web
installer and the on-device update entity pick the right binary
automatically.
