---
name: protocol-change
description: Use when changing how GbbDongle talks to the GbbOptimizer cloud or the inverter — gbb_protocol (toDevice/fromDevice JSON), modbus_executor, emergency_store/emergency_manager, cloud_pairing, bus arbitration in gbb_dongle.cpp, or docs/protocol.md.
---

# Changing protocol code

The dongle is a stateless proxy: GbbOptimizer sends raw Modbus RTU frames
over MQTT, the dongle runs them on RS485 and answers. The cloud decides
everything else, so the dongle must behave exactly like GbbConnect2, the
application it replaces.

## Before writing code

1. Read the matching section of `docs/protocol.md` (MQTT session, Payload,
   Emergency command set, Pairing, GbbDongle-specific behavior).
2. Read the GbbConnect2 code it was derived from:
   [gbbsoft/GbbConnect2](https://github.com/gbbsoft/GbbConnect2)
   (Krzysztof's checkout: `~/dev/gbbsoft/GbbConnect2`), mainly
   `GbbEngine2/Server/JobManager-mqtt.cs` and
   `GbbConnect2Protocol/Protocol.cs`. Error semantics mirror GbbConnect2
   exactly; don't "improve" them. If the two disagree, say so and ask.
3. Two decisions were confirmed by the GbbOptimizer author; re-check with
   Krzysztof before changing either:
   - the cloud sends one `toDevice` telegram at a time and waits for the
     answer, so a single pending-request slot is enough (no FIFO);
   - once an emergency set starts, it runs to the end before the next cloud
     request takes the bus (`EmergencyManager::wants_bus()` first).

## Rules for the code

- Everything from the broker is attacker-controlled. Check sizes **before**
  allocating or `reserve()`-ing: see `MAX_LINES_PER_ARRAY` in
  `gbb_protocol.h` and `MAX_TODEVICE_PAYLOAD`. With exceptions disabled, a
  failed allocation aborts and reboot-loops the device.
- Never block the ESPHome main loop: the executor is a state machine
  (IDLE → GAP → TRANSMIT → RX_WAIT → DONE); pairing's HTTP runs on its own
  FreeRTOS task.
- Never log the Plant Token, a pairing code or the answer's token.
- Payload keys are PascalCase (ArduinoJson), as GbbConnect2 sends them.
- Update `docs/protocol.md` in the same change. It is the spec other agents
  and the support bot read.

## Verification

```sh
uv run esphome -s external_components_source ../components compile firmware/gbbdongle-factory.yaml
tests/host/run.sh                       # needs one compile first (ArduinoJson); see tests/host/README.md
```

`tests/host/` covers `cloud_pairing` today; add a `test_<unit>.cpp` there when
a change is testable on the host.

Runtime behaviour needs the bench (`tools/bench/README.md`): a local broker,
a build without TLS, `tools/bench/modbus_slave.py` on the USB-RS485 adapter
standing in for the inverter, and

```sh
uv run python tools/cloud_roundtrip.py --broker <mac ip> --plant-id TEST1 --request-file <request.json>
```

Ask Krzysztof before flashing the bench unit, and report what ran on
hardware and what only compiled. Pairing must never be tested with a real
GbbOptimizer code unasked: redeeming one rotates that plant's token.
