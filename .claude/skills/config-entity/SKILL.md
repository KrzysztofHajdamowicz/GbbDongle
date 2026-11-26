---
name: config-entity
description: Use when adding, renaming or changing an ESPHome entity in GbbDongle (sensor, text, number, select, switch, button, text_sensor, update) — in firmware/common/*.yaml, a board file, or wired into the gbb_dongle component.
---

# Adding or changing an entity

An entity shows up in five places: the device's web panel, the stock ESPHome
UI, Home Assistant (HA line), the web_server REST API, and — unless
suppressed — the GbbOptimizer cloud MQTT link. Its **name and id are stable
API**: renaming one churns Home Assistant entities, breaks the panel and every
REST script. Rename only with Krzysztof's explicit OK.

## Checklist

1. Every named entity carries an empty `state_topic:`. Without it ESPHome
   mirrors the entity to the cloud broker (the Plant Token included) and obeys
   its `/command` topic. `tools/check_mqtt_exposure.py` fails CI otherwise.
2. Every named entity carries a `web_server:` block with `sorting_group_id`
   (one of `grp_status`, `grp_cloud`, `grp_inverter`, `grp_network`,
   `grp_system`, declared under `web_server: sorting_groups:` in
   `firmware/common/base.yaml`) and a `sorting_weight` that fits between its
   neighbours.
3. Secrets (tokens, passwords, pairing codes) are never logged and never
   published as a sensor value. Text entities holding them use
   `mode: password` where the domain supports it — follow `Plant Token` and
   `Admin Password`.
4. If the component reads or writes it: add a `*_id` key (`cv.use_id`) to
   the schema in `components/gbb_dongle/__init__.py`, the matching setter in
   `gbb_dongle.h`, and wire the id in the `gbb_dongle:` block of the YAML.
5. Web panel (`firmware/webui/panel.js`): add the entity to the `ID` table
   (`"<domain>/<display name>"` — the event-stream id), place it on a page,
   and add its labels to **both** `STR.pl` and `STR.en`. An entity missing
   from `ID` still appears under System → Other.
6. `tools/webui/mock_device.py`: add it to the hand-written entity set so the
   panel can be developed without a device.
7. `README.md` "Configuration entities": a row with the name and meaning.
   Check docs and tools that address entities over REST
   (`grep -rn "<old name>" docs tools website`).

REST addresses entities by the URL-encoded **display name**, not the
object_id (`POST /text/Plant%20Id/set?value=…`), and a POST needs a body
(`curl -d ''`).

## Verification

```sh
node --check firmware/webui/panel.js
for c in gbbdongle gbbdongle-tcan485 gbbdongle-kamami gbbdongle-8di8do-wifi gbbdongle-8di8do-eth; do
  uv run python tools/check_mqtt_exposure.py firmware/$c-factory.yaml firmware/$c-ha.yaml
done
python3 tools/webui/mock_device.py            # then open http://127.0.0.1:8080/
```

Look at the panel in both languages, light and dark, and with
`--ethernet`, `--line ha` or `--unconfigured` when the entity depends on
them. Compile at least one factory and one HA image. On hardware, check the
entity through the panel and REST, and that nothing new appears on the
broker (`mosquitto_sub -t '#' -v` on a bench broker).
