---
name: add-board
description: Use when adding support for a new ESP32 board (or a new network variant of an existing board) to GbbDongle — firmware packages, entrypoints, CI, release manifests, the website's device cards and install guide, and docs.
---

# Adding a board

A board touches firmware, CI, the release manifests and the website. Missing
one of them breaks either the installer or OTA updates for that board, and
nothing fails loudly. Read "Supported boards" in `AGENTS.md` first: it lists
the gotchas of the existing boards (RS485 direction, UART0 sharing, Ethernet
vs WiFi).

## Names to choose first

- `<config>`: the device name and file basename, `gbbdongle-<board>`. It is
  stable API from the first release on (artifact names, manifest URLs, HA
  entity ids). `gbbdongle` alone is the Waveshare ESP32-S3-RS485-CAN.
- `<suffix>`: the manifest suffix (`tcan485`, `8di8do-eth`, …).
- One board with WiFi and Ethernet = two configs (`wifi:` and `ethernet:`
  are mutually exclusive): a core board file plus `-wifi`/`-eth` overlays,
  like `firmware/boards/waveshare-esp32-s3-poe-eth-8di-8do*.yaml`.

## Checklist

1. `firmware/boards/<board>.yaml`: chip, flash, PSRAM, UART pins, RS485
   direction (`flow_control_pin` only for manual-direction transceivers),
   Ethernet, BLE provisioning, and on Ethernet boards the IP sensor
   (`ip_address_id`). No PSRAM ⇒ `log_buffer_size: 8192`. Every named entity
   needs an empty `state_topic:` and a `web_server:` sorting block (see the
   `config-entity` skill).
2. Entrypoints in `firmware/`, copied from an existing board of the same
   transport:
   - `<config>.yaml`: the import target, self-contained (no `updates.yaml`, no
     local paths), WiFi boards include `common/wifi.yaml`;
   - `<config>-factory.yaml` and `<config>-ha.yaml`, each with its own
     `update_manifest_url`
     (`https://gbbdongle.hajdamowicz.info/firmware/update-manifest-<suffix>.json`,
     `…-<suffix>-ha.json` for the HA line);
   - `<config>-dev.yaml` (+ `common/wifi-dev.yaml` on WiFi boards).
3. `.github/workflows/ci.yaml`: add `<config>` to the `validate-import-targets`
   loop and both `-factory`/`-ha` to the `build` matrix. The paths filter
   already covers `firmware/**`.
4. `.github/workflows/release.yaml`: both images in the `build` matrix.
5. `tools/site/assemble-manifests.sh`: a row in `BOARDS`
   (`<config> <chipFamily> <suffix> <display name>`). **Never** add the board
   to the legacy shared manifests: the update entity takes the first
   chipFamily match, and the ESP32 and ESP32-S3 slots are taken.
6. Website, PL and EN together:
   - `website/data/devices.yaml`: an entry (`id`, `manifest: <suffix>`,
     `hostname: <config>`, `net: wifi|eth`, `image`, `name`, taglines; `group:
     other` unless it is a main product);
   - the photo in `website/static/img/`;
   - a `{{< product >}}` card in `website/content/pl/urzadzenia.md` and
     `website/content/en/urzadzenia.md` (hand-written, not data-driven);
   - `{{< only device="<id>" >}}` blocks in `content/{pl,en}/instalacja.md`
     where the board needs its own steps (antenna, USB vs UART0, PoE…).
7. Docs: a row in the boards table of `README.md` and a "Supported boards"
   entry with its gotchas in `AGENTS.md`.

## Verification

```sh
uv run esphome -s external_components_source ../components config firmware/<config>.yaml
uv run python tools/check_mqtt_exposure.py firmware/<config>-factory.yaml firmware/<config>-ha.yaml
uv run esphome -s external_components_source ../components compile firmware/<config>-factory.yaml
uv run esphome -s external_components_source ../components compile firmware/<config>-ha.yaml
```

Dry-run the manifests: create dummy `<dir>/<cfg>{,-ha}.{factory,ota}.bin` for
**every** board in `BOARDS`, run
`tools/site/assemble-manifests.sh <dir> 9.9.9 https://example.invalid/rel`
and `jq .` each JSON. Then `hugo --source website` (or
`hugo server --source website` after `tools/site/fetch-firmware.sh`; the new
board has no binaries until the next release).

A compile proves nothing about the RS485 wiring or direction control: say so,
and ask Krzysztof for a bench round trip on the real board
(`tools/bench/README.md`) before the board is announced.
