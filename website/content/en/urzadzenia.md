---
title: Devices
slug: devices
description: One firmware, several boards. Choose by how well WiFi reaches your inverter and how much room you have in the distribution board.
menu:
  main:
    name: Devices
    weight: 10
---

{{< product image="img/waveshare-esp32-s3-rs485-can.jpg" name="Waveshare ESP32-S3-RS485-CAN" variant="external antenna" pill="Recommended" buy="true" >}}
**The version in the ready-made GbbDongle.** The external WiFi antenna has a clearly better range than the internal one, so it is a good choice when the inverter is far from the router or behind a wall.

- ESP32-S3, WiFi, WiFi setup over Bluetooth or USB
- isolated RS485 port, DIN rail mount
- **needs a distribution board with at least two rows**: the antenna sticks out above the module and takes room in the row above
{{< /product >}}

{{< product image="img/waveshare-esp32-s3-rs485-can.jpg" name="Waveshare ESP32-S3-RS485-CAN" variant="internal antenna" >}}
**The same board, fits any distribution board.** This is the version Polish shops usually sell (e.g. on Allegro). The internal antenna is weaker: if your current inverter logger already struggles with WiFi, this version will not fix that.

- ESP32-S3, WiFi, WiFi setup over Bluetooth or USB
- isolated RS485 port, DIN rail mount
- fits a single-row distribution board
{{< /product >}}

{{< product image="img/waveshare-esp32-s3-poe-eth-8di-8do.jpg" name="Waveshare ESP32-S3-POE-ETH-8DI-8DO" variant="Ethernet / PoE" >}}
**When WiFi at the inverter does not work** (for example in a metal garage or a boiler room). One Ethernet cable carries network and power (PoE 802.3af). It can also be powered from USB or the 7–36 V terminals.

- ESP32-S3, Ethernet (W5500) with PoE; WiFi with the SMA antenna as an alternative
- isolated RS485 port, DIN rail mount
- takes **8 modules** on the rail
{{< /product >}}

## Comparison

| | RS485-CAN, ext. antenna | RS485-CAN, int. antenna | 8DI-8DO |
|---|---|---|---|
| Connectivity | WiFi | WiFi | Ethernet + PoE or WiFi |
| WiFi range | good | weaker | (Ethernet) |
| Distribution board | two rows | single row | 8 modules |
| Power | 7–36 V or USB-C | 7–36 V or USB-C | PoE, 7–36 V or USB-C |
| Firmware | the same | the same | Ethernet or WiFi variant |

## Other supported boards

For tinkerers: **LilyGo T-CAN485** (bare board without enclosure, WiFi) and **Kamami KAmod ESP32 ETH+PoE** with the KAmodRPi UART RS485 ISO HAT (Ethernet). They run the same firmware and are in the [installer]({{< relref "instalacja" >}}).

## Build it or buy it?

Everything is public: buy a board from the list (copy its name into your shop's search box), flash it from the [installer]({{< relref "instalacja" >}}) and make the cable from an Ethernet cable. The ready-made GbbDongle arrives assembled, flashed and tested with an inverter.
