---
title: "Deye"
translationKey: pinouts-deye
weight: 10
description: "Gniazdo Modbus i Meter-485 w falownikach Deye: piny 1 (B) i 2 (A)."
---

Rysunki wtyczek są w [konwencji strony]({{< relref "/pinouty" >}}): styki do siebie, zatrzask z tyłu, piny po
lewej, kabel w prawo, pin 1 na dole.

## Trójfazowe hybrydy SG04LP3 i SG05LP3

Dotyczy m.in. SUN-5…12K-SG04LP3-EU i SUN-3…20K-SG05LP3-EU-SM2.

### Którego gniazda użyć

Gniazdo RJ45 opisane **Modbus** (w SG04LP3 i SG05LP3 drugie od prawej). Jeśli Modbus
zajmuje inne urządzenie, dongle działa też na gnieździe **Meter-485**. Nie używaj gniazd
**BMS**, **CAN** ani **Parallel**.

### Gniazdo Modbus

![Deye, gniazdo Modbus: pin 1 485 B, pin 2 485 A, piny 3 i 6 GND](img/pinouts/deye-modbus.svg)

| Pin | Sygnał (Deye) | Zacisk dongla | Kolor T568B |
|---|---|---|---|
| 1 | SUNSPE-485_B | **B** | biało-pomarańczowy |
| 2 | SUNSPE-485_A | **A** | pomarańczowy |
| 3 | GND_SUNSPE-485 | GND (opcjonalnie) | biało-zielony |
| 4, 5 | – | – | |
| 6 | GND_SUNSPE-485 | GND (opcjonalnie) | zielony |
| 7 | SUNSPE-485_A | (drugi komplet A) | biało-brązowy |
| 8 | SUNSPE-485_B | (drugi komplet B) | brązowy |

Piny 7 i 8 to ta sama magistrala co 1 i 2. Służą do łańcuchowego połączenia kolejnego
falownika w układzie master/slave: łącz zawsze A z A (pin 2 lub 7) i B z B (pin 1 lub 8).
Opis takiej instalacji jest w [FAQ]({{< relref "/faq" >}}), w sekcji „Several inverters (Deye master/slave)”.

### Gniazdo Meter-485

![Deye, gniazdo Meter-485: pin 1 485 B, pin 2 485 A, pin 3 GND](img/pinouts/deye-meter485.svg)

| Pin | Sygnał (Deye) | Zacisk dongla |
|---|---|---|
| 1 | METER-485-B | **B** |
| 2 | METER-485-A | **A** |
| 3 | COM-GND | GND (opcjonalnie) |
| 4, 5, 6 | METER-485-B, METER-485-A, COM-GND | – |
| 7, 8 | METER-485-A, METER-485-B | – |

Na Meter-485 te same sygnały powtarzają się na pinach 4/5 i 7/8, więc wystarczy para 1/2.

### Status

✅ Potwierdzone w instrukcji **SUN-(14-20)K-SG05LP3-EU-SM2** (Appendix I, s. 49 instrukcji,
s. 51 pliku PDF). Instrukcja SUN-(8-12)K-SG04LP3 (Ver 2.1, 2021-04) ma identyczny opis
Meter-485, a gniazdo Modbus opisuje jeszcze jako „Reserved”: w starszych egzemplarzach
użyj Meter-485 albo zaktualizuj firmware falownika. Solar Assistant podaje dla wszystkich
modeli Deye ten sam układ: pin 1 RS485B, pin 2 RS485A.

### Jak Deye rysuje złącze

Instrukcja Deye pokazuje **gniazdo od frontu z wycięciem na zatrzask na dole**, piny
1…8 od lewej. To zgadza się z tym, co widzisz, patrząc w gniazdo falownika. Obok jest
rysunek wtyczki w perspektywie: końcówka w lewo, kabel w prawo jak u nas, ale **zatrzask
jest na górze**, a numeracja 8…1 biegnie ukośnie. Nie przenoś tej numeracji wprost na naszą
konwencję, w której zatrzask jest z tyłu.

## Jednofazowe hybrydy (SG01LP1, SG02LP1, SG03LP1, SG04LP1, SG05LP1)

Solar Assistant podaje dla gniazda **RS485** ten sam układ co w trójfazowych: **pin 1 B,
pin 2 A**. W starszych jednostkach 5 kW Solar Assistant wskazuje gniazdo **BMS 485**, a nie
to opisane „RS 485”.

🟡 Nie sprawdzaliśmy tego w instrukcjach jednofazowych Deye.

## Uwaga o nazwach A i B

Deye nazywa pin 2 „A”, a pin 1 „B”, i tak samo opisuje go Solar Assistant. Niektóre
integracje (np. dokumentacja Eniris) prowadzą pin 1 Deye do zacisku „A” swojego
konwertera, czyli odwrotnie. Jeżeli dongle nie dostaje odpowiedzi, zamień A z B.

## Źródła

- Deye, *SUN-14K…20K-SG05LP3-EU-SM2 User Manual*, Appendix I: [kopia PDF (solartech.ro)](https://solartech.ro/wp-content/uploads/2024/08/Manual-deye-14.15.16.18.20-kw-1.pdf)
- Deye, *SUN-8K/10K/12K-SG04LP3 User Manual* Ver 2.1, Appendix II: [kopia PDF](https://cdn.shopify.com/s/files/1/2510/0896/files/deye_Manuale_utente_trifase.pdf)
- Solar Assistant: [Deye SG05LP3 RS485](https://solar-assistant.io/help/inverters/deye/SG05LP3/rs485), [SG04LP3 RS485](https://solar-assistant.io/help/inverters/deye/SG04LP3/rs485), [lista modeli Deye](https://solar-assistant.io/help/inverters/deye)
- Eniris: [Deye, okablowanie](https://docs.eniris.be/pl/Devices/PV-hybrid-and-battery-inverters/DEYE)
