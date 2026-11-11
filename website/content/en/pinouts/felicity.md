---
title: "Felicity T-REX"
translationKey: pinouts-felicity
weight: 40
description: "COM port on Felicity T-REX-10K/15K (RS485-3: position 4 A, 3 B) and the RS485 port on T-REX HP."
---

> **Status:** this pinout is less well confirmed than Deye, Solis and Hoymiles. For the
> T-REX-10KLP3G01 we only have the German manual, and none for the 15 kW model. Before you
> connect, compare it with the label printed next to the connector on your inverter.

## T-REX-10KLP3G01 and T-REX-15KLP3G01 (three-phase, 48 V battery)

### Which connector

The **COM** connector underneath the inverter (no. 11 in the manual's overview drawing).
According to the manual it is a **six-position terminal block**, not an RJ45 socket.
**RS485-3** (manual: "zum Debuggen", for monitoring) is meant for external devices;
**RS485-2** serves the energy meter.

![Felicity T-REX-10KLP3G01 COM connector: position 3 485 B3, position 4 485 A3](img/pinouts/felicity-trex-lp3-com.svg)

| Position | Signal (Felicity) | Manufacturer note | Dongle terminal |
|---|---|---|---|
| 1 | RY1_4 | dry contact | – |
| 2 | RY1_5 | dry contact | – |
| 3 | RS485_B3 | RS485-3 | **B** |
| 4 | RS485_A3 | RS485-3 | **A** |
| 5 | RS485_A2 | RS485-2, meter | – |
| 6 | RS485_B2 | RS485-2, meter | – |

🟡 Source: *Benutzerhandbuch T-REX-10KLP3G01* (358-S010004-01), table 4.4-1. Uncertain:

- the connector type and the order of positions on the block are read off a drawing, not
  stated in text;
- the English manual on Felicity's site was not reachable for us;
- we found no manual for the **T-REX-15KLP3G01** and assume the same layout as the 10K.

Positions 5 and 6 are used by the meter shipped with the inverter: do not connect the
dongle there.

## T-REX HP (high voltage, e.g. T-REX-50KHP3G01)

The RJ45 port labelled **RS485** underneath the inverter.

![Felicity T-REX HP RS485 port: pin 3 485 A3, pin 4 485 B3](img/pinouts/felicity-trex-hp-rs485.svg)

| Pin | Signal (Felicity) | Manufacturer note | Dongle terminal |
|---|---|---|---|
| 1 | 485_A2 | RS485-2, meter | – |
| 2 | 485_B2 | RS485-2, meter | – |
| 3 | 485_A3 | RS485-3, remote monitoring | **A** |
| 4 | 485_B3 | RS485-3, remote monitoring | **B** |
| 5 | 485_B3 | RS485-3 (repeated) | – |
| 6 | 485_A3 | RS485-3 (repeated) | – |
| 7 | RY_4 | dry contact | – |
| 8 | RY_5 | dry contact | – |

🟡 Source: the manufacturer's table reproduced on Solar Assistant's pages (T-REX-50KHP3G01,
IVGM). Note: Solar Assistant's text says "pin 5 RS485A, pin 6 RS485B", while the
manufacturer's table on the same page says the opposite (5 = B3, 6 = A3). Use **pins 3 and
4**, which agree in both places. Whether the T-REX-10KHP3G01 uses the same layout:
⚠️ not verified.

### How Felicity draws the connector

Felicity draws the socket **latch up**, numbered **8…1 from the left**, and the plug in
perspective numbered 8…1. That is geometrically right (pin 1 is on the right when the latch
is on top), but easy to confuse with Deye's or Solis's drawings, where the latch is at the
bottom and pin 1 on the left. Our plug drawing converts it to the common site convention.

## Sources

- Felicity, *Benutzerhandbuch Hybrid-Solarwechselrichter T-REX-10KLP3G01* (358-S010004-01): [PDF (solarscouts.de)](https://solarscouts.de/mediafiles/Sonstiges/Benutzerhandbuch%20-%20Felicity%20T-REX-10KLP3G01.pdf)
- Solar Assistant: [Felicity T-REX 50kW HP RS485](https://solar-assistant.io/help/inverters/felicity/T-REX-50KLP1G01/rs485), [IVGM RS485](https://solar-assistant.io/help/inverters/felicity/IVGM/rs485)
