---
title: "Felicity T-REX"
translationKey: pinouts-felicity
weight: 40
description: "Port COM w Felicity T-REX-10K/15K (RS485-3: pozycja 4 A, 3 B) i gniazdo RS485 w T-REX HP."
---

> **Status:** ten pinout jest potwierdzony słabiej niż dla Deye, Solisa i Hoymilesa. Dla
> T-REX-10KLP3G01 mamy tylko niemiecką instrukcję, a dla 15 kW żadnej. Przed podłączeniem
> porównaj opis z naklejką lub nadrukiem przy złączu w swoim falowniku.

## T-REX-10KLP3G01 i T-REX-15KLP3G01 (trójfazowe, bateria 48 V)

### Którego złącza użyć

Złącze **COM** na spodzie falownika (nr 11 na rysunku przeglądowym w instrukcji). Według
instrukcji to **listwa sześciu zacisków**, nie gniazdo RJ45. Interfejs **RS485-3**
(instrukcja: „zum Debuggen”, do monitoringu) jest przeznaczony dla urządzeń zewnętrznych,
a **RS485-2** obsługuje licznik energii.

![Felicity T-REX-10KLP3G01, złącze COM: pozycja 3 485 B3, pozycja 4 485 A3](img/pinouts/felicity-trex-lp3-com.svg)

| Pozycja | Sygnał (Felicity) | Opis producenta | Zacisk dongla |
|---|---|---|---|
| 1 | RY1_4 | styk bezpotencjałowy | – |
| 2 | RY1_5 | styk bezpotencjałowy | – |
| 3 | RS485_B3 | RS485-3 | **B** |
| 4 | RS485_A3 | RS485-3 | **A** |
| 5 | RS485_A2 | RS485-2, licznik | – |
| 6 | RS485_B2 | RS485-2, licznik | – |

🟡 Źródło: *Benutzerhandbuch T-REX-10KLP3G01* (358-S010004-01), tabela 4.4-1. Niepewne:

- rodzaj złącza i kolejność pozycji na listwie odczytaliśmy z rysunku, nie z opisu;
- angielska instrukcja ze strony Felicity była dla nas niedostępna;
- dla **T-REX-15KLP3G01** nie znaleźliśmy instrukcji, zakładamy ten sam układ co w 10K.

Pozycje 5 i 6 zajmuje licznik dostarczany z falownikiem: nie podłączaj tam dongla.

## T-REX HP (wysokonapięciowe, np. T-REX-50KHP3G01)

Gniazdo RJ45 **RS485** na spodzie falownika.

![Felicity T-REX HP, gniazdo RS485: pin 3 485 A3, pin 4 485 B3](img/pinouts/felicity-trex-hp-rs485.svg)

| Pin | Sygnał (Felicity) | Opis producenta | Zacisk dongla |
|---|---|---|---|
| 1 | 485_A2 | RS485-2, licznik | – |
| 2 | 485_B2 | RS485-2, licznik | – |
| 3 | 485_A3 | RS485-3, zdalny monitoring | **A** |
| 4 | 485_B3 | RS485-3, zdalny monitoring | **B** |
| 5 | 485_B3 | RS485-3 (powtórzony) | – |
| 6 | 485_A3 | RS485-3 (powtórzony) | – |
| 7 | RY_4 | styk bezpotencjałowy | – |
| 8 | RY_5 | styk bezpotencjałowy | – |

🟡 Źródło: tabela producenta zamieszczona na stronach Solar Assistant (T-REX-50KHP3G01,
IVGM). Uwaga: tekst na stronie Solar Assistant podaje „pin 5 RS485A, pin 6 RS485B”, a tabela
producenta na tej samej stronie odwrotnie (5 = B3, 6 = A3). Używaj pinów **3 i 4**, które są
zgodne w obu miejscach. Czy T-REX-10KHP3G01 ma ten sam układ: ⚠️ niezweryfikowane.

### Jak Felicity rysuje złącze

Felicity rysuje gniazdo **zatrzaskiem do góry**, z numeracją **8…1 od lewej**, a wtyczkę w
perspektywie z numerami 8…1. Geometrycznie to poprawne (pin 1 po prawej, gdy zatrzask jest
na górze), ale łatwo pomylić się z rysunkami Deye czy Solisa, gdzie zatrzask jest na dole i
pin 1 po lewej. Nasz rysunek wtyczki przelicza to na wspólną konwencję strony.

## Źródła

- Felicity, *Benutzerhandbuch Hybrid-Solarwechselrichter T-REX-10KLP3G01* (358-S010004-01): [PDF (solarscouts.de)](https://solarscouts.de/mediafiles/Sonstiges/Benutzerhandbuch%20-%20Felicity%20T-REX-10KLP3G01.pdf)
- Solar Assistant: [Felicity T-REX 50kW HP RS485](https://solar-assistant.io/help/inverters/felicity/T-REX-50KLP1G01/rs485), [IVGM RS485](https://solar-assistant.io/help/inverters/felicity/IVGM/rs485)
