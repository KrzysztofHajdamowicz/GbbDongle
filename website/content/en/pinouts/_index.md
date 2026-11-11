---
title: "Inverter RS485 pinouts"
translationKey: pinouts
weight: 40
description: "Which port, which pins and how not to read the plug the wrong way round: Deye, Solis, Hoymiles HiT, Felicity T-REX."
menu:
  main:
    name: Pinouts
    weight: 30
---

GbbDongle talks to the inverter over two RS485 wires (**A** and **B**), optionally a third
(**GND**). This page shows which inverter port to use and which pins carry those signals.
Every RJ45 plug on this site is drawn **in one orientation**, whatever the manufacturer's
drawing looks like.

## How to read the drawings

![RJ45 plug: contacts facing you, latch at the back, pins on the left, cable to the right](img/pinouts/convention-en.svg)

Hold the plug like this:

1. **contacts (gold blades) facing you**,
2. **latch at the back**, away from you,
3. **contact end to the left**, **cable leaving to the right**.

In this position **pin 1 is at the bottom and pin 8 at the top**. Wire colours in the
drawings follow T568B, the scheme most ready-made network cables use.

## Rules that always apply

- **A to A, B to B.** The pin the manufacturer calls A (RS485 A, 485_A, RS485+) goes to the
  dongle's **A** terminal, B to **B**. GND is optional, but worth connecting on long cables
  and with several inverters.
- **Count pins, not colours.** A cable may be crimped as T568A, which swaps the orange and
  green pairs. Check the cable with a tester or a multimeter.
- **No answer? Swap A and B.** Inverter and adapter makers disagree on which line is "A".
  Swapping the wires does no harm.
- **One polling device per bus.** The dongle cannot share the same wires with
  SolarAssistant, a Wi-Fi logger or another Modbus integration (see the [FAQ]({{< relref "/faq" >}})).
- **Inverter settings:** Modbus address **1**, **9600** baud, 8N1, unless you changed it in
  the dongle's web panel.
- **Never connect supply pins** (+5 V, +12 V, VCC) to the dongle.

## Traps in manufacturer drawings

Manufacturers draw the RJ45 connector in several ways and do not always say from which
side. Before you copy a pin number from a manual onto a cable, work out what is drawn.

| What the drawing shows | Where pin 1 is |
|---|---|
| Socket from the front, latch slot **at the bottom** | on the **left** |
| Socket from the front, latch slot **at the top** (socket mounted upside down) | on the **right** |
| Plug from the contact side, tip up, cable down | on the **left** |
| Plug end-on (looking into the tip), latch at the bottom | on the **right** |

![RJ45 socket from the front, latch slot at the bottom: pin 1 on the left](img/pinouts/socket-latch-down.svg) ![RJ45 socket from the front, latch slot at the top: pin 1 on the right](img/pinouts/socket-latch-up.svg)

Upside-down sockets do exist: on **Hoymiles HiT** inverters the top row of the COM1 block
(including **Meter_2**) is mounted the other way up from the bottom row. Felicity draws its
socket latch-up, numbered 8…1 from the left. Each brand page says how its source is drawn.

## Summary

| Inverter | Port | A | B | GND | Source |
|---|---|---|---|---|---|
| [Deye]({{< relref "deye" >}}) three-phase SG04LP3 / SG05LP3 | Modbus (RJ45) | 2 | 1 | 3 or 6 | ✅ Deye manual |
| [Deye]({{< relref "deye" >}}) three-phase SG04LP3 / SG05LP3 | Meter-485 (RJ45) | 2 | 1 | 3 | ✅ Deye manual |
| [Deye]({{< relref "deye" >}}) single-phase SG0xLP1 | RS485 (RJ45) | 2 | 1 | – | 🟡 Solar Assistant |
| [Solis]({{< relref "solis" >}}) S6-EH3P (three-phase 8–20 kW) | RS485 (RJ45) | 1 | 2 | – | ✅ Solis manual |
| [Solis]({{< relref "solis" >}}) S6-EH1P (single-phase) | RS485 (RJ45) | 5 | 4 | – | ✅ Solis manual |
| [Hoymiles HiT]({{< relref "hoymiles" >}}) HIT-(5-20)L-G3 | Meter_2 (RJ45, top row, upside down) | 4 | 5 | 6 | ✅ Hoymiles manual |
| [Felicity]({{< relref "felicity" >}}) T-REX-10KLP3G01 / 15KLP3G01 | COM (terminal block) | 4 | 3 | – | 🟡 German 10K manual only |
| [Felicity]({{< relref "felicity" >}}) T-REX HP (e.g. 50KHP3G01) | RS485 (RJ45) | 3 | 4 | – | 🟡 manufacturer table via Solar Assistant |

Legend: ✅ confirmed in the manufacturer's manual, 🟡 indirect or incomplete source,
⚠️ not verified. None of these replaces a look at the port labels on your own unit:
manufacturers change boards between revisions.

Your model is missing or something does not match? Ask on the GbbDongle Discord channel,
ideally with a photo of the port and the manual page.
