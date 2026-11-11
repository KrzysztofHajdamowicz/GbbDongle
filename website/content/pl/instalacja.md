---
title: Instalacja firmware
description: Wybierz urządzenie, wersję firmware i wgraj je przez USB prosto z przeglądarki. Gotowy GbbDongle ma firmware już wgrany, zacznij wtedy od kroku „Podłącz do sieci”.
layout: installer
menu:
  main:
    name: Instalacja
    weight: 20
---

{{< installer >}}

## Instrukcja krok po kroku {#guide-title}

<p id="guide-hint" class="hint">Wybierz urządzenie i wersję firmware powyżej, a instrukcja dopasuje się do Twojego wyboru.</p>

<div id="guide" class="guide" hidden>

{{< step title="Przygotuj komputer i kabel" >}}
- Komputer z przeglądarką obsługującą Web Serial: **Chrome**, **Edge**, **Opera** albo **Firefox** od wersji 151. Safari tego nie potrafi; instalację należy wykonać z komputera, nie z telefonu.
- Kabel **USB-C z transmisją danych**. Kabel tylko do ładowania nie pokaże portu.
- Do wgrania wystarczy zasilanie z USB, także w płytkach z Ethernetem.
{{< only device="kamami" >}}
**Kamami:** zdejmij nakładkę RS485 przed podłączeniem USB. Konwerter USB i nakładka używają tych samych pinów, nie łącz ich jednocześnie.
{{< /only >}}
{{< /step >}}

{{< step title="Wgraj firmware" >}}
1. Podłącz płytkę kablem USB.
2. Kliknij **Zainstaluj firmware** w kroku 3 kreatora powyżej.
3. Wybierz z listy port, który pojawił się po podłączeniu płytki (np. `COM8` w Windows, `cu.usbmodem…` w macOS).
4. Przy pierwszej instalacji zaznacz **wyczyszczenie urządzenia** (Erase device).
5. Poczekaj, aż pasek postępu dojdzie do końca. Trwa to około minuty.
{{< /step >}}

{{< step title="Podłącz do WiFi" net="wifi" >}}
Najprościej od razu w instalatorze: po wgraniu wybierz **Configure Wi-Fi**, wskaż swoją sieć i wpisz hasło.

Możesz też zrobić to później: dongle bez skonfigurowanego WiFi wystawia punkt dostępowy **GbbDongle Setup**. Połącz się z nim telefonem, a strona konfiguracji otworzy się sama.
{{< only device="8di8do-wifi" >}}
**8DI-8DO:** moduł nie ma anteny na płytce. Bez przykręconej anteny SMA WiFi nie zadziała.
{{< /only >}}
{{< /step >}}

{{< step title="Podłącz Ethernet" net="eth" >}}
{{< only device="kamami" >}}
Odłącz USB, załóż nakładkę RS485, a dopiero potem podłącz zasilanie.
{{< /only >}}
{{< only device="8di8do-eth" >}}
Wepnij kabel sieciowy. Zasilanie dostarczy switch z **PoE** (802.3af); możesz też zasilić płytkę z USB albo z zacisków 7–36 V.
{{< /only >}}
{{< only device="kamami" >}}
Wepnij kabel sieciowy. Zasilanie dostarczy switch z **PoE** albo zasilacz 5 V.
{{< /only >}}
Adres IP dongle dostanie z routera (DHCP).
{{< /step >}}

{{< step title="Otwórz panel WWW dongla" >}}
Wpisz w przeglądarce {{< hostname >}}.

Jeśli nazwy `.local` nie działają w Twojej sieci, znajdź adres IP na liście urządzeń DHCP w routerze. Możesz też podłączyć dongle przez USB, otworzyć ten instalator, wybrać **Logs & Console** i odczytać adres IP z logu.
{{< only line="factory" >}}
Zaloguj się jako `admin` / `admin` i od razu ustaw własne hasło w polu **Admin Password**. To samo hasło chroni wgrywanie aktualizacji. Puste pole przywraca hasło `admin`, a **Factory Reset** wszystkie ustawienia domyślne.
{{< /only >}}
{{< /step >}}

{{< step title="Połącz z GbbOptimizerem" >}}
1. W GbbOptimizerze otwórz **Instalacje**, przy swojej instalacji kliknij **Popraw** i na górze zmień typ instalacji na **GbbDongle**. Dopóki tego nie zrobisz, GbbOptimizer rozmawia z falownikiem starą drogą (np. przez Deye Cloud) i dongle nic nie dostaje.
2. Przepisz z instalacji do panelu dongla: **MQTT Server**, **Plant Id** i **Plant Token**. Tokenu nigdy nie publikuj.
3. Kliknij **Apply Settings (Restart)**.
{{< /step >}}

{{< step title="Dodaj do Home Assistanta (opcjonalnie)" line="ha" >}}
Dongle pojawi się jako *Discovered* w panelu ESPHome obok Home Assistanta. Kliknij **Adopt** albo dodaj go bezpośrednio w Home Assistancie; klucz szyfrowania API powstanie przy pierwszym połączeniu.

Po adopcji firmware kompiluje Twój panel ESPHome i to Ty odpowiadasz za aktualizacje. Wersję Factory możesz w każdej chwili przywrócić z tego instalatora.
{{< /step >}}

{{< step title="Podłącz do falownika" >}}
Zrób kabel z gotowego kabla Ethernet z jedną wtyczką uciętą: fabrycznie zaciśnięta wtyczka idzie do falownika, a przewody RS485 przykręcasz do zacisków dongla **A**, **B** i opcjonalnie **GND**. Licz numery pinów, nie sugeruj się kolorami żył.

Który port i które piny, sprawdzisz w dziale [Pinouty]({{< relref "pinouty" >}}). Domyślne ustawienia portu to 9600 bodów, 8N1. Zmienisz je w panelu bez restartu.
{{< /step >}}

{{< step title="Sprawdź, czy działa" >}}
W GbbOptimizerze, w **Prognoza baterii**, wybierz **Operacje → Pobierz wszystkie dane z instalacji** i obserwuj log dongla. Powinny pojawić się zapytania z GbbOptimizera i odpowiedzi falownika; linia zaczynająca się od `01 03` to odczyt z falownika o adresie Modbus 1.

Zapytania bez odpowiedzi to prawie zawsze okablowanie: sprawdź kabel testerem i spróbuj zamienić A z B. Więcej w [FAQ]({{< relref "faq" >}}).
{{< /step >}}

</div>
