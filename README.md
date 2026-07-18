# GbbDongle

ESPHome-based firmware for the
[Waveshare ESP32-S3-RS485-CAN](https://www.waveshare.com/esp32-s3-rs485-can.htm)
that connects a photovoltaic inverter (Deye and friends) directly to the
[GbbOptimizer](https://gbboptimizer.gbbsoft.pl/) cloud — a drop-in hardware
replacement for the GbbConnect2 Windows/Docker application.

The dongle plugs into the inverter's RS485 Modbus port and acts as a
stateless proxy: GbbOptimizer sends batches of raw Modbus RTU frames over
MQTT, the dongle executes them on the bus and sends the responses back. See
[docs/protocol.md](docs/protocol.md) for the protocol details.

## Installation

1. Open the **[web installer](https://krzysztofhajdamowicz.github.io/GbbDongle/)**
   in Chrome/Edge, connect the board over USB, click Install.
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
| TLS / TLS Skip CN Check | TLS is on by default (ISRG Root X1 CA compiled in) |
| RS485 Baud Rate / Parity | serial parameters, applied live |

Note: the Plant Token is masked in the web UI but visible to the Home
Assistant API, like any ESPHome text entity.

## Development

Requires [uv](https://docs.astral.sh/uv/) (`brew install uv`).

```sh
uv sync
uv run esphome config firmware/gbbdongle.yaml    # validate
uv run esphome compile firmware/gbbdongle.yaml   # build release image
uv run esphome run firmware/gbbdongle-dev.yaml   # dev build, flash & logs
```

The dev variant (`firmware/gbbdongle-dev.yaml`) joins your WiFi from
`firmware/secrets.yaml` (see `secrets.yaml.example`) and logs verbosely.

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

Tagging `v*` builds the firmware, attaches `gbbdongle.factory.bin` /
`gbbdongle.ota.bin` to the GitHub Release and deploys the web installer plus
both manifests (ESP Web Tools + OTA update) to GitHub Pages.
