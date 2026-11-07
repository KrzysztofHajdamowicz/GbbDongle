---
title: Urządzenia
description: Jedno oprogramowanie, kilka płytek. Wybierz według tego, jak dobrze WiFi dociera do falownika i ile masz miejsca w rozdzielnicy.
menu:
  main:
    name: Urządzenia
    weight: 10
---

{{< product image="img/waveshare-esp32-s3-rs485-can.jpg" name="Waveshare ESP32-S3-RS485-CAN" variant="antena zewnętrzna" pill="Polecane" buy="true" >}}
**Wersja z gotowego GbbDongle.** Zewnętrzna antena WiFi daje wyraźnie lepszy zasięg niż antena wbudowana, więc to dobry wybór, gdy falownik stoi daleko od routera albo za ścianą.

- ESP32-S3, WiFi, konfiguracja WiFi przez Bluetooth lub USB
- izolowany port RS485, montaż na szynie DIN
- **wymaga rozdzielnicy co najmniej dwurzędowej**: antena wystaje nad moduł i zajmuje miejsce w rzędzie powyżej
{{< /product >}}

{{< product image="img/waveshare-esp32-s3-rs485-can.jpg" name="Waveshare ESP32-S3-RS485-CAN" variant="antena wbudowana" >}}
**Ta sama płytka, mieści się w każdej rozdzielnicy.** Taką wersję zwykle znajdziesz w polskich sklepach (np. na Allegro). Antena wbudowana jest słabsza: jeśli obecny logger falownika już ma kłopoty z WiFi, ta wersja tego nie naprawi.

- ESP32-S3, WiFi, konfiguracja WiFi przez Bluetooth lub USB
- izolowany port RS485, montaż na szynie DIN
- pasuje do rozdzielnicy jednorzędowej
{{< /product >}}

{{< product image="img/waveshare-esp32-s3-poe-eth-8di-8do.jpg" name="Waveshare ESP32-S3-POE-ETH-8DI-8DO" variant="Ethernet / PoE" >}}
**Gdy WiFi przy falowniku nie działa** (klasyka: metalowy garaż albo kotłownia). Jeden kabel Ethernet daje sieć i zasilanie (PoE 802.3af). Można też zasilić z USB albo z zacisków 7–36 V.

- ESP32-S3, Ethernet (W5500) z PoE; alternatywnie WiFi z anteną SMA
- izolowany port RS485, montaż na szynie DIN
- zajmuje **8 modułów** na szynie
{{< /product >}}

## Porównanie

| | RS485-CAN, antena zewn. | RS485-CAN, antena wbud. | 8DI-8DO |
|---|---|---|---|
| Łączność | WiFi | WiFi | Ethernet + PoE lub WiFi |
| Zasięg WiFi | dobry | słabszy | (Ethernet) |
| Rozdzielnica | dwurzędowa | jednorzędowa | 8 modułów |
| Zasilanie | 7–36 V lub USB-C | 7–36 V lub USB-C | PoE, 7–36 V lub USB-C |
| Firmware | ten sam | ten sam | wersja Ethernet albo WiFi |

## Inne obsługiwane płytki

Dla majsterkowiczów: **LilyGo T-CAN485** (goła płytka bez obudowy, WiFi) i **Kamami KAmod ESP32 ETH+PoE** z nakładką KAmodRPi UART RS485 ISO (Ethernet). Działają z tym samym firmware i znajdziesz je w [instalatorze]({{< relref "instalacja" >}}).

## Zrób to sam czy kup gotowe?

Wszystko jest publiczne: kup płytkę z listy (skopiuj jej nazwę do wyszukiwarki sklepu), wgraj firmware z [instalatora]({{< relref "instalacja" >}}) i zrób kabel z przewodu Ethernet. Gotowy GbbDongle przychodzi złożony, wgrany i sprawdzony z falownikiem.
