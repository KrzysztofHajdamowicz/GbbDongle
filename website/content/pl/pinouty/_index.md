---
title: "Pinouty RS485 falowników"
translationKey: pinouts
weight: 40
description: "Które gniazdo, które piny i jak nie pomylić strony wtyczki: Deye, Solis, Hoymiles HiT, Felicity T-REX."
menu:
  main:
    name: Pinouty
    weight: 30
---

GbbDongle łączy się z falownikiem dwoma przewodami RS485 (**A** i **B**), opcjonalnie
trzecim (**GND**). Na tej stronie znajdziesz, które gniazdo falownika wybrać i na których
pinach są te sygnały. Wszystkie rysunki wtyczek RJ45 na stronie są narysowane **w jednej
orientacji**, niezależnie od tego, jak rysuje je producent.

## Jak czytać rysunki

![Wtyczka RJ45: piny do siebie, zatrzask z tyłu, piny po lewej, kabel w prawo](img/pinouts/convention-pl.svg)

Weź wtyczkę do ręki tak:

1. **styki (złote blaszki) skierowane do Ciebie**,
2. **zatrzask (języczek blokady) z tyłu**, od Ciebie,
3. **końcówka ze stykami w lewo**, **kabel wychodzi w prawo**.

W tej pozycji **pin 1 jest na dole, pin 8 na górze**. Kolory przewodów na rysunkach to
standard T568B, w którym zarobiona jest większość gotowych kabli sieciowych.

## Zasady, które obowiązują zawsze

- **A do A, B do B.** Pin opisany przez producenta jako A (RS485 A, 485_A, RS485+) idzie na
  zacisk **A** dongla, pin B na zacisk **B**. GND jest opcjonalne, ale przy dłuższych kablach
  i kilku falownikach warto go podłączyć.
- **Licz piny, nie kolory.** Kabel może być zarobiony w T568A, wtedy pary pomarańczowa i
  zielona zamieniają się miejscami. Sprawdź kabel testerem albo multimetrem.
- **Brak odpowiedzi? Zamień A z B.** Producenci i producenci adapterów nie są zgodni, która
  linia to „A”. Zamiana przewodów niczego nie uszkodzi.
- **Jedno urządzenie pytające na magistrali.** Dongle nie może dzielić tych samych przewodów
  z SolarAssistantem, loggerem Wi-Fi czy inną integracją Modbus (szczegóły w [FAQ]({{< relref "/faq" >}})).
- **Ustawienia po stronie falownika:** adres Modbus **1**, **9600** bodów, 8N1, chyba że
  ustawiłeś inaczej w panelu dongla.
- **Nigdy nie podłączaj pinów zasilania** (+5 V, +12 V, VCC) do dongla.

## Pułapki w rysunkach producentów

Producenci rysują złącze RJ45 na kilka sposobów i nie zawsze piszą, z której strony.
Zanim przeniesiesz numer pinu z instrukcji na kabel, ustal, co jest narysowane.

| Co jest na rysunku | Gdzie jest pin 1 |
|---|---|
| Gniazdo od frontu, wycięcie na zatrzask **na dole** | po **lewej** |
| Gniazdo od frontu, wycięcie na zatrzask **na górze** (gniazdo zamontowane „do góry nogami”) | po **prawej** |
| Wtyczka od strony styków, końcówka do góry, kabel w dół | po **lewej** |
| Wtyczka od czoła (patrzysz w końcówkę), zatrzask na dole | po **prawej** |

![Gniazdo RJ45 od frontu, zatrzask na dole: pin 1 po lewej](img/pinouts/socket-latch-down.svg) ![Gniazdo RJ45 od frontu, zatrzask na górze: pin 1 po prawej](img/pinouts/socket-latch-up.svg)

Gniazda montowane do góry nogami naprawdę się zdarzają: w falownikach **Hoymiles HiT**
górny rząd gniazd COM1 (w tym **Meter_2**) jest odwrócony względem dolnego. Felicity rysuje
swoje gniazdo zatrzaskiem do góry, z numeracją 8…1 od lewej. Na podstronach każdej marki
zaznaczamy, jak narysowane jest źródło.

## Podsumowanie

| Falownik | Gniazdo | A | B | GND | Źródło |
|---|---|---|---|---|---|
| [Deye]({{< relref "deye" >}}) trójfazowe SG04LP3 / SG05LP3 | Modbus (RJ45) | 2 | 1 | 3 lub 6 | ✅ instrukcja Deye |
| [Deye]({{< relref "deye" >}}) trójfazowe SG04LP3 / SG05LP3 | Meter-485 (RJ45) | 2 | 1 | 3 | ✅ instrukcja Deye |
| [Deye]({{< relref "deye" >}}) jednofazowe SG0xLP1 | RS485 (RJ45) | 2 | 1 | – | 🟡 Solar Assistant |
| [Solis]({{< relref "solis" >}}) S6-EH3P (trójfazowe 8–20 kW) | RS485 (RJ45) | 1 | 2 | – | ✅ instrukcja Solis |
| [Solis]({{< relref "solis" >}}) S6-EH1P (jednofazowe) | RS485 (RJ45) | 5 | 4 | – | ✅ instrukcja Solis |
| [Hoymiles HiT]({{< relref "hoymiles" >}}) HIT-(5-20)L-G3 | Meter_2 (RJ45, górny rząd, odwrócony) | 4 | 5 | 6 | ✅ instrukcja Hoymiles |
| [Felicity]({{< relref "felicity" >}}) T-REX-10KLP3G01 / 15KLP3G01 | COM (listwa zaciskowa) | 4 | 3 | – | 🟡 tylko instrukcja DE dla 10K |
| [Felicity]({{< relref "felicity" >}}) T-REX HP (np. 50KHP3G01) | RS485 (RJ45) | 3 | 4 | – | 🟡 tabela producenta przez Solar Assistant |

Legenda: ✅ potwierdzone w instrukcji producenta, 🟡 źródło pośrednie albo niepełne,
⚠️ niezweryfikowane. Żaden z tych pinoutów nie zastępuje spojrzenia na opis gniazda w
Twoim egzemplarzu: producenci zmieniają płyty między rewizjami.

Brakuje Twojego modelu albo coś się nie zgadza? Napisz na kanale GbbDongle na Discordzie,
najlepiej ze zdjęciem gniazda i stroną instrukcji.
