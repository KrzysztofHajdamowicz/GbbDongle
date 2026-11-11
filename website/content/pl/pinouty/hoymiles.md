---
title: "Hoymiles HiT"
translationKey: pinouts-hoymiles
weight: 30
description: "Gniazdo Meter_2 w hybrydach Hoymiles HIT-G3: pin 4 (A), pin 5 (B), pin 6 (GND). Uwaga na odwrócony górny rząd gniazd."
---

Rysunki wtyczek są w [konwencji strony]({{< relref "/pinouty" >}}): styki do siebie, zatrzask z tyłu, piny po
lewej, kabel w prawo, pin 1 na dole.

Dotyczy trójfazowych hybryd **HIT-5L-G3 … HIT-20L-G3** (wersje globalne i AU).

## Którego gniazda użyć

Dongle podłączamy do gniazda **Meter_2** w bloku **COM1**. Gniazdo **Meter_1** zajmuje
licznik energii dostarczany z falownikiem. Nie używaj gniazd **Parallel**, **BMS** ani
**DRM**.

## Gniazdo Meter_2

![Hoymiles HiT, gniazdo Meter_2: pin 4 485 A, pin 5 485 B, pin 6 GND](img/pinouts/hoymiles-hit-meter2.svg)

| Pin | Sygnał (Hoymiles) | Zacisk dongla | Kolor T568B |
|---|---|---|---|
| 1, 2, 3 | NC | – | |
| 4 | 485A | **A** | niebieski |
| 5 | 485B | **B** | biało-niebieski |
| 6 | GND | GND (opcjonalnie) | zielony |
| 7, 8 | NC | – | |

✅ Potwierdzone w instrukcji *Three-phase Hybrid Inverter HIT-(5-20)L-G3* (rozdz. 7.7 COM1
Connection). Ten sam układ pinów ma gniazdo Meter_1.

## Pułapka: górny rząd gniazd jest odwrócony

COM1 to blok 2×3 gniazd RJ45. Według rysunku w instrukcji **górny rząd (Parallel_2,
Meter_2, BMS) jest zamontowany do góry nogami**: wycięcie na zatrzask jest na górze, a
**pin 1 po prawej**, gdy patrzysz w gniazdo. Dolny rząd (Parallel_1, Meter_1, DRM) ma
zatrzask na dole i pin 1 po lewej.

![Blok COM1 Hoymiles HiT od frontu: górny rząd odwrócony, pin 1 po prawej](img/pinouts/hoymiles-hit-com1.svg)

W praktyce: wtyczkę do Meter_2 wkładasz zatrzaskiem do góry. Numeracja pinów w samej
wtyczce się nie zmienia, więc kabel zarobiony według tabeli powyżej jest poprawny. Zmienia
się tylko to, co widzisz, licząc piny w gnieździe.

Instrukcja globalna (2025) rysuje w tabeli pinów wszystkie gniazda tak samo: zatrzaskiem
w dół, piny 1…8 od lewej, kolory T568B. To widok dolnego rzędu. Nowsza instrukcja AU
(REV0.5, 2026) rysuje już gniazda górnego rzędu, w tym Meter_2, zatrzaskiem do góry, z
numeracją 8…1 od lewej, co potwierdza odwrócenie.

## Alternatywa: zaciski 485_2 w COM2

Hoymiles przeznacza na sterowanie przez urządzenia zewnętrzne i VPP zaciski **485_2** w
listwie **COM2** (przewód 0,5–0,8 mm²). Nie testowaliśmy z nimi GbbDongle.
⚠️ Niezweryfikowane.

## Status obsługi

Czy falownik działa z GbbDongle, zależy od obsługi w GbbOptimizerze. Zapytaj na kanale
GbbDongle na Discordzie, zanim kupisz sprzęt pod Hoymiles.

## Źródła

- Hoymiles, *Three-phase Hybrid Inverter HIT-(5-20)L-G3 User Manual*, rozdz. 7.7 i 7.8: [PDF (hoymiles.com)](https://www.hoymiles.com/uploadfile/1/202506/ec8b1b6a09.pdf)
- Hoymiles, *Three-phase Hybrid Inverter HIT-(5-20)L-G3-AU User Manual* REV0.5, rozdz. 7.7 (tabela wprost dla Meter_2): [PDF (hoymiles.com)](https://www.hoymiles.com/uploadfile/1/202607/c1506d349b.pdf)
