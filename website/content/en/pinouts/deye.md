---
title: "Deye"
translationKey: pinouts-deye
weight: 10
description: "Modbus and Meter-485 ports on Deye inverters: pins 1 (B) and 2 (A)."
---

Plug drawings follow the [site convention]({{< relref "/pinouts" >}}): contacts facing you, latch at the back,
pins on the left, cable to the right, pin 1 at the bottom.

## Three-phase hybrids SG04LP3 and SG05LP3

Covers e.g. SUN-5…12K-SG04LP3-EU and SUN-3…20K-SG05LP3-EU-SM2.

### Which port

The RJ45 port labelled **Modbus** (on the SG04LP3 and SG05LP3, the second from the right).
If another device uses Modbus, the dongle also works on the **Meter-485** port. Do not use
the **BMS**, **CAN** or **Parallel** ports.

### Modbus port

![Deye Modbus port: pin 1 485 B, pin 2 485 A, pins 3 and 6 GND](img/pinouts/deye-modbus.svg)

| Pin | Signal (Deye) | Dongle terminal | T568B colour |
|---|---|---|---|
| 1 | SUNSPE-485_B | **B** | white-orange |
| 2 | SUNSPE-485_A | **A** | orange |
| 3 | GND_SUNSPE-485 | GND (optional) | white-green |
| 4, 5 | – | – | |
| 6 | GND_SUNSPE-485 | GND (optional) | green |
| 7 | SUNSPE-485_A | (second A) | white-brown |
| 8 | SUNSPE-485_B | (second B) | brown |

Pins 7 and 8 are the same bus as pins 1 and 2. They daisy-chain the next inverter in a
master/slave setup: always connect A to A (pin 2 or 7) and B to B (pin 1 or 8). The [FAQ]({{< relref "/faq" >}})
section "Several inverters (Deye master/slave)" describes such an installation.

### Meter-485 port

![Deye Meter-485 port: pin 1 485 B, pin 2 485 A, pin 3 GND](img/pinouts/deye-meter485.svg)

| Pin | Signal (Deye) | Dongle terminal |
|---|---|---|
| 1 | METER-485-B | **B** |
| 2 | METER-485-A | **A** |
| 3 | COM-GND | GND (optional) |
| 4, 5, 6 | METER-485-B, METER-485-A, COM-GND | – |
| 7, 8 | METER-485-A, METER-485-B | – |

Meter-485 repeats the same signals on pins 4/5 and 7/8, so the 1/2 pair is enough.

### Status

✅ Confirmed in the **SUN-(14-20)K-SG05LP3-EU-SM2** manual (Appendix I, manual page 49,
PDF page 51). The SUN-(8-12)K-SG04LP3 manual (Ver 2.1, 2021-04) gives the same Meter-485
pinout and still lists the Modbus port as "Reserved": on older units use Meter-485 or update
the inverter firmware. Solar Assistant shows the same layout for every Deye model: pin 1
RS485B, pin 2 RS485A.

### How Deye draws the connector

The Deye manual shows the **socket from the front with the latch slot at the bottom**, pins
1…8 from the left. That matches what you see looking into the inverter's port. Next to it
is a perspective drawing of the plug: tip to the left and cable to the right as here, but
**the latch is on top** and the 8…1 numbering runs diagonally. Do not carry that numbering
straight over to our convention, where the latch is at the back.

## Single-phase hybrids (SG01LP1, SG02LP1, SG03LP1, SG04LP1, SG05LP1)

Solar Assistant gives the same layout for the **RS485** port as on the three-phase models:
**pin 1 B, pin 2 A**. On older 5 kW units, Solar Assistant points to the **BMS 485** port,
not the one labelled "RS 485".

🟡 We have not checked this in Deye's single-phase manuals.

## A note on A and B naming

Deye calls pin 2 "A" and pin 1 "B", and Solar Assistant labels them the same way. Some
integrations (e.g. the Eniris documentation) wire Deye pin 1 to the "A" terminal of their
converter, the other way round. If the dongle gets no answer, swap A and B.

## Sources

- Deye, *SUN-14K…20K-SG05LP3-EU-SM2 User Manual*, Appendix I: [PDF copy (solartech.ro)](https://solartech.ro/wp-content/uploads/2024/08/Manual-deye-14.15.16.18.20-kw-1.pdf)
- Deye, *SUN-8K/10K/12K-SG04LP3 User Manual* Ver 2.1, Appendix II: [PDF copy](https://cdn.shopify.com/s/files/1/2510/0896/files/deye_Manuale_utente_trifase.pdf)
- Solar Assistant: [Deye SG05LP3 RS485](https://solar-assistant.io/help/inverters/deye/SG05LP3/rs485), [SG04LP3 RS485](https://solar-assistant.io/help/inverters/deye/SG04LP3/rs485), [Deye model list](https://solar-assistant.io/help/inverters/deye)
- Eniris: [Deye wiring](https://docs.eniris.be/Devices/PV-hybrid-and-battery-inverters/DEYE)
