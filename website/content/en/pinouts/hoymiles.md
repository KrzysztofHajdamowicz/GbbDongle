---
title: "Hoymiles HiT"
translationKey: pinouts-hoymiles
weight: 30
description: "Meter_2 port on Hoymiles HIT-G3 hybrids: pin 4 (A), pin 5 (B), pin 6 (GND). Mind the upside-down top row of ports."
---

Plug drawings follow the [site convention]({{< relref "/pinouts" >}}): contacts facing you, latch at the back,
pins on the left, cable to the right, pin 1 at the bottom.

Covers the three-phase hybrids **HIT-5L-G3 … HIT-20L-G3** (global and AU versions).

## Which port

Connect the dongle to the **Meter_2** port in the **COM1** block. **Meter_1** is taken by
the energy meter shipped with the inverter. Do not use the **Parallel**, **BMS** or **DRM**
ports.

## Meter_2 port

![Hoymiles HiT Meter_2 port: pin 4 485 A, pin 5 485 B, pin 6 GND](img/pinouts/hoymiles-hit-meter2.svg)

| Pin | Signal (Hoymiles) | Dongle terminal | T568B colour |
|---|---|---|---|
| 1, 2, 3 | NC | – | |
| 4 | 485A | **A** | blue |
| 5 | 485B | **B** | white-blue |
| 6 | GND | GND (optional) | green |
| 7, 8 | NC | – | |

✅ Confirmed in the *Three-phase Hybrid Inverter HIT-(5-20)L-G3* manual (section 7.7 COM1
Connection). Meter_1 has the same pinout.

## Trap: the top row of ports is upside down

COM1 is a 2×3 block of RJ45 sockets. According to the manual's drawing, **the top row
(Parallel_2, Meter_2, BMS) is mounted upside down**: the latch slot is at the top and
**pin 1 is on the right** as you look into the socket. The bottom row (Parallel_1,
Meter_1, DRM) has the latch slot at the bottom and pin 1 on the left.

![Hoymiles HiT COM1 block from the front: top row upside down, pin 1 on the right](img/pinouts/hoymiles-hit-com1.svg)

In practice: the plug goes into Meter_2 latch up. The pin numbering inside the plug does
not change, so a cable crimped per the table above is correct. Only what you see when
counting pins in the socket changes.

The global manual (2025) draws every socket in its pin table the same way: latch down, pins
1…8 from the left, T568B colours. That is the bottom-row view. The newer AU manual (REV0.5,
2026) draws the top-row sockets, Meter_2 included, latch up and numbered 8…1 from the left,
which confirms the inversion.

## Alternative: 485_2 terminals in COM2

Hoymiles designates the **485_2** terminals of the **COM2** terminal strip (0.5–0.8 mm²
wire) for third-party control and VPP. We have not tested GbbDongle on them.
⚠️ Not verified.

## Support status

Whether the inverter works with GbbDongle depends on support in GbbOptimizer. Ask on the
GbbDongle Discord channel before you buy hardware for a Hoymiles.

## Sources

- Hoymiles, *Three-phase Hybrid Inverter HIT-(5-20)L-G3 User Manual*, sections 7.7 and 7.8: [PDF (hoymiles.com)](https://www.hoymiles.com/uploadfile/1/202506/ec8b1b6a09.pdf)
- Hoymiles, *Three-phase Hybrid Inverter HIT-(5-20)L-G3-AU User Manual* REV0.5, section 7.7 (table names Meter_2 explicitly): [PDF (hoymiles.com)](https://www.hoymiles.com/uploadfile/1/202607/c1506d349b.pdf)
