---
title: "Solis"
translationKey: pinouts-solis
weight: 20
description: "Gniazdo RS485 w hybrydach Solis S6: trójfazowe piny 1 (A) i 2 (B), jednofazowe piny 5 (A) i 4 (B)."
---

Rysunki wtyczek są w [konwencji strony]({{< relref "/pinouty" >}}): styki do siebie, zatrzask z tyłu, piny po
lewej, kabel w prawo, pin 1 na dole.

> **Uwaga:** Solis używa różnych pinów w różnych seriach. Trójfazowe S6-EH3P mają RS485
> na pinach 1/2, a jednofazowe S6-EH1P na pinach 4/5. Sprawdź dokładne oznaczenie modelu
> na tabliczce znamionowej, zanim zarobisz kabel.

W każdym przypadku używaj gniazda opisanego **RS485** („third-party external devices”).
Gniazdo **Meter** obsługuje licznik energii, **BMS** baterię, a **PAR-A/PAR-B** pracę
równoległą: nie podłączaj tam dongla.

## S6-EH3P: trójfazowe hybrydy 8–20 kW

Dotyczy m.in. S6-EH3P12K/15K/20K-ND-H, S6-EH3P8K/10K/12K-LV-ND-H oraz starszych
S6-EH3P(12-20)K-H.

![Solis S6-EH3P, gniazdo RS485: pin 1 RS485 A, pin 2 RS485 B](img/pinouts/solis-s6-eh3p.svg)

| Pin | Sygnał (Solis) | Zacisk dongla | Kolor T568B |
|---|---|---|---|
| 1 | RS485A | **A** | biało-pomarańczowy |
| 2 | RS485B | **B** | pomarańczowy |
| 3–8 | – | – | |

Instrukcja nie podaje pinu GND na tym gnieździe.

✅ Potwierdzone w instrukcjach Solis: *S6 Series Hybrid Inverter, three phase* (V1.2, 07.2025,
rozdz. 4.8.6) i *S6-EH3P(12-20)K-H* (rozdz. 4.8.6).

## S6-EH1P: jednofazowe hybrydy

Dotyczy m.in. S6-EH1P(3-8)K-L-PRO i S6-EH1P(3-6)K-L-AU. Solar Assistant podaje ten sam
układ dla S6-EH1P6K-L-PLUS.

![Solis S6-EH1P, gniazdo RS485: pin 4 RS485 B, pin 5 RS485 A](img/pinouts/solis-s6-eh1p.svg)

| Pin | Sygnał (Solis) | Zacisk dongla | Kolor T568B |
|---|---|---|---|
| 4 | RS485B | **B** | niebieski |
| 5 | RS485A | **A** | biało-niebieski |
| 1–3, 6–8 | – | – | |

✅ Potwierdzone w instrukcjach Solis *S6-EH1P(3-8)K-L-PRO* (V1.1, 03.2024, rozdz. 3.10.6)
i *S6-EH1P(3-6)K-L-AU* (V1.1, 11.2024, rozdz. 4.7.6). Wersja L-PLUS: 🟡 tylko Solar Assistant.

## Pozostałe serie Solis

| Seria | Gniazdo | A | B | Status |
|---|---|---|---|---|
| S6-EH3P30K/50K-H | RS485 (RJ45) | 5 | 4 | 🟡 Solar Assistant |
| S6-EH3P 60–125K (Solarator) | RS485 (RJ45) | 5 | 4 | 🟡 Solar Assistant |
| RHI-3P5K-HVES-5G-DC | RS485 (RJ45) | 5 | 4 | 🟡 Solar Assistant |
| S5-EH1P(5-6)K-L | COM (gniazdo loggera Wi-Fi, nie RJ45) | 2 | 3 | 🟡 Solar Assistant |

W S5-EH1P-L to gniazdo, z którego czyta logger Wi-Fi: dongle i logger nie mogą pracować
jednocześnie. Na pinie 1 tego gniazda jest **+5 V**, nie podłączaj go do dongla.

## Jak Solis rysuje złącze

Solis pokazuje **gniazdo od frontu, wycięcie na zatrzask na dole**, piny 1…8 od lewej, z
kolorami T568B. To zgadza się z widokiem gniazda w falowniku. Kolory dotyczą kabla T568B:
w kablu T568A pary pomarańczowa i zielona są zamienione, więc licz piny.

## Źródła

- Solis, *User Manual for S6 Series Hybrid Inverter, three phase* (S6-EH3P…-ND-H), V1.2 07.2025: [PDF](https://kesko-onninen-pim-resources-production.s3-eu-west-1.amazonaws.com/pimdocuments/25939540.pdf)
- Solis, *S6-EH3P(12-20)K-H User Manual*: [PDF](https://myshop.s3-external-3.amazonaws.com/shop4072000.images.Solis_S6-EH3P12-20_Manual.pdf)
- Solis, *User Manual for S6 Series Hybrid Inverter* S6-EH1P(3-8)K-L-PRO, V1.1 03.2024: [PDF (upwatt.com)](https://upwatt.com/mpattachment/file/download/id/626)
- Solis, *User Manual for S6 Series Hybrid Inverter* S6-EH1P(3-6)K-L-AU, V1.1 11.2024: [PDF](https://downloads.osw.energy/au/downloads/2162/docs/Solis_Manual_RHI3-6K_S6_AUS_V1%201202309013.pdf)
- Solar Assistant: [lista modeli Solis](https://solar-assistant.io/help/inverters/solis), [S6 Hybrid RS485](https://solar-assistant.io/help/inverters/solis/S6/rs485), [S5-EH1P-L RS485](https://solar-assistant.io/help/inverters/solis/S5-EH1P6K-L/rs485)
