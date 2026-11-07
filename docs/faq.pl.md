# GbbDongle – najczęstsze pytania

Polskie tłumaczenie [faq.md](faq.md). Przy zmianach aktualizuj oba pliki. Kroki instalacji i lista płytek są w
[README](../README.md), a protokół chmury w [protocol.md](protocol.md).

## Zakup i wybór sprzętu

### Czy mogę kupić gotowy GbbDongle?

Tak. Autor sprzedaje złożony, wgrany i przetestowany GbbDongle za 399 zł brutto (z fakturą VAT); zapytaj
na kanale GbbDongle na Discordzie. To Waveshare ESP32-S3-RS485-CAN na szynę DIN z zewnętrzną anteną WiFi.

### Czy mogę zbudować go sam?

Tak, wszystko jest publiczne. Kup jedną z [obsługiwanych płytek](../README.md#supported-boards), wgraj
firmware z [instalatora](https://krzysztofhajdamowicz.github.io/GbbDongle/instalacja/) przez USB w
przeglądarce z Web Serial (Chrome, Edge, Opera albo Firefox 151+ na komputerze) i zrób kabel do falownika z
kabla Ethernet z jedną uciętą wtyczką (zobacz [Okablowanie](#okablowanie-deye)). Instalator podaje
dokładną nazwę każdej płytki: skopiuj ją do wyszukiwarki sklepu.

### Którą płytkę wybrać?

- **WiFi dobrze dociera do falownika:** Waveshare ESP32-S3-RS485-CAN, montowany na szynie DIN. Wersje
  sprzedawane w polskich sklepach (np. na Allegro) mają antenę wbudowaną, wyraźnie słabszą od zewnętrznej
  anteny w egzemplarzach autora. Jeśli obecny logger falownika już ma kłopoty z WiFi, płytka z anteną
  wbudowaną tego nie naprawi. Zewnętrzna antena wystaje nad moduł, więc potrzebuje rozdzielnicy co
  najmniej dwurzędowej.
- **Słabe albo żadne WiFi przy falowniku** (klasyczny przypadek to metalowy garaż): poprowadź kabel
  Ethernet i postaw tam punkt dostępowy WiFi albo użyj płytki z Ethernetem: Waveshare
  ESP32-S3-POE-ETH-8DI-8DO (firmware Ethernet, PoE) lub Kamami KAmod ESP32 ETH+PoE. 8DI-8DO zajmuje na
  szynie DIN 8 modułów.
- **LilyGo T-CAN485** działa, ale to goła płytka bez obudowy i mocowania na szynę DIN.

### Ile kosztuje utrzymanie?

Około 0,5 W. W normalnej pracy dongle nic nie zapisuje w pamięci i nie potrzebuje komputera, w
przeciwieństwie do GbbConnect2 czy jego dodatku do Home Assistanta, które wymagają maszyny włączonej
24/7.

## Wgrywanie firmware i pierwsza konfiguracja

### Czy do wgrania firmware potrzebuję dodatkowego zasilania?

Nie, wystarczy USB, także w płytkach z Ethernetem. Za pierwszym razem firmware zawsze wgrywa się przez
USB; kolejne aktualizacje przychodzą przez sieć.

### Instalator pyta, czy wyczyścić urządzenie

Przy pierwszej instalacji zaznacz tę opcję. Na liście portów wybierz port, który pojawił się po
podłączeniu płytki (np. `COM8` w Windows).

### Jak znaleźć dongle w sieci?

Płytki z WiFi: skonfiguruj WiFi z instalatora albo połącz się z punktem dostępowym `GbbDongle Setup`.
Płytki z Ethernetem dostają adres z DHCP. Potem otwórz panel WWW:

- po nazwie, np. `http://gbbdongle-8di8do-eth.local`;
- jeśli nazwy `.local` nie działają w Twojej sieci, znajdź adres IP na liście DHCP w routerze albo
  podłącz płytkę przez USB, otwórz instalator, wybierz **Logs & Console** i odczytaj adres IP z logu
  (z podłączonym kablem Ethernet).

Login do panelu WWW to `admin` / `admin` (firmware Factory). Zmienisz go w polu **Admin Password**;
zapisanie pustego hasła przywraca `admin`, a **Factory Reset** przywraca wszystkie ustawienia domyślne.

### Co ustawić w GbbOptimizerze?

1. W **Instalacje** otwórz swoją instalację przyciskiem **Popraw** i na górze zmień typ instalacji na
   **GbbDongle**. Dopóki tego nie zrobisz, GbbOptimizer rozmawia z falownikiem poprzednią drogą (np. przez
   Deye Cloud), a dongle nic nie dostaje.
2. Przepisz z instalacji do panelu WWW dongla adres serwera MQTT, identyfikator instalacji (Plant Id) i
   token dostępu, potem kliknij **Apply Settings (Restart)**. Nigdy nie publikuj tokenu.
3. Od razu użyj swojej prawdziwej, działającej instalacji. Jeśli coś pójdzie nie tak, stracisz najwyżej
   około godziny sterowania falownikiem.

### Jak sprawdzić, czy działa?

W GbbOptimizerze, w **Prognoza baterii**, uruchom **Operacje → Pobierz wszystkie dane z instalacji** i
obserwuj log dongla: powinny pojawiać się zapytania z GbbOptimizera i odpowiedzi falownika. Linia
zaczynająca się od `01 03` to odczyt z falownika o adresie Modbus 1. Zapytania bez odpowiedzi zwykle
oznaczają problem z okablowaniem (zobacz niżej).

## Okablowanie (Deye)

### Którego portu falownika użyć?

Gniazda RJ45 opisanego **Modbus** (w modelach SG04LP3/SG05LP3 drugie od prawej). Użytkownicy podłączali
też dongle do portu **Meter-485**, gdy port Modbus zajmowało inne urządzenie. Nie używaj portów **BMS**
ani **CAN**.

Poradniki SolarAssistant pokazują pinout RS485 dla każdego modelu Deye, np.
[SG04LP3](https://solar-assistant.io/help/inverters/deye/SG04LP3/rs485) i
[SG05LP3](https://solar-assistant.io/help/inverters/deye/SG05LP3/rs485).

### Jak zrobić kabel?

Weź gotowy kabel Ethernet i utnij wtyczkę z jednej strony. Fabrycznie zaciśniętą wtyczkę zostaw od strony
falownika, bo prawie zawsze jest zaciśnięta poprawnie. Odizoluj ucięty koniec i przykręć żyły RS485 do
zacisków dongla opisanych **A**, **B** i **GND**. Potrzebujesz 2 lub 3 z 8 żył (A, B i opcjonalnie GND).
Licz numery pinów, nie sugeruj się kolorami żył według standardu Ethernet. Adapter RJ45 na zaciski też
działa, ale zwykle nie pozwala zamknąć pokrywy falownika.

Jeśli dongle nie dostaje odpowiedzi z falownika, sprawdź kabel testerem i spróbuj zamienić A z B.

### Port RS485 jest współdzielony z baterią

W niektórych modelach jedno gniazdo RJ45 prowadzi zarówno CAN do baterii, jak i Modbus. Zrób własny
kabel rozdzielający: para CAN idzie do baterii, para Modbus do dongla.

### Co ustawić w falowniku?

W ustawieniach zaawansowanych falownika znajdź ustawienia Modbus: falownik musi być **master**, z adresem
Modbus **01** i prędkością **9600** bodów. Dongle domyślnie pracuje z 9600 bodów 8N1 i można to zmienić
na żywo w jego panelu WWW.

### Czy dongle może dzielić magistralę z SolarAssistantem, integracją ESPHome Modbus albo innym czytnikiem?

Nie na tych samych przewodach. Na magistrali RS485 tylko jedno urządzenie może aktywnie pytać o dane. To
nie Ethernet, gdzie każdy rozmawia z każdym. Podłącz każdy czytnik do osobnego portu (np. dongle do
Meter-485, a inne urządzenie do Modbus) albo przenieś pozostałe narzędzia na Modbus TCP.

## Kilka falowników (Deye master/slave)

### Czy jeden dongle obsłuży kilka falowników Deye?

Tak, od października 2026, tylko Deye. Wszystkie falowniki są na jednej magistrali RS485 i każdy
odpowiada pod własnym adresem Modbus.

- **Topologia:** łańcuch, nie gwiazda. Magistrala zaczyna się przy donglu, przechodzi przez każdy
  falownik i kończy terminatorem 120 Ω za ostatnim. Kolejność na magistrali nie ma znaczenia.
- **Adresy:** ustawiasz je w każdym falowniku. Master ma adres 1, slave'y 2, 3 i tak dalej.
- **Ta magistrala jest dodatkiem** do własnego połączenia równoległego falowników, nie jego zamiennikiem.
- **Okablowanie skrętką Cat5e:** piny 1 i 2 pierwszego falownika idą do dongla, a piny 7 i 8 tego
  falownika do pinów 1 i 2 następnego (instrukcja Deye SUN-14-20K-SG05LP3, strona 51). Linia A zawsze
  łączy się z A, a B z B: nic się nie krzyżuje. W pierwszej działającej instalacji niezbędna okazała się
  też wspólna masa (GND) między falownikami.
- **W GbbOptimizerze:** ustaw typ instalacji na GbbDongle. Na liście slave'ów wpisz tylko falowniki
  podrzędne (adres Modbus 2 lub wyższy), każdy z numerem seryjnym i jednostką Modbus. Nie wpisuj mastera
  jako slave'a: to podwaja produkcję i inne wartości.
- **Sprawdzenie:** log dongla powinien pokazywać odczyty z każdego adresu, np. `01 03` i `02 03`. Jeśli
  któryś adres nigdy nie odpowiada, popraw okablowanie.

## Home Assistant

### Czy GbbDongle wysyła dane z falownika do Home Assistanta?

Nie. Dongle tylko przekazuje ramki Modbus między GbbOptimizerem a falownikiem i ich nie interpretuje, więc
nie ma żadnych odczytów falownika do opublikowania.

Firmware **zgodny z Home Assistantem** udostępnia Home Assistantowi sam dongle przez natywne API ESPHome:
jego własne encje i piny GPIO płytki, z których możesz korzystać dowolnie. Po adopcji sam budujesz i
utrzymujesz ten firmware (bez wsparcia autora). Między firmware Factory a zgodnym z HA możesz przełączać
się w każdej chwili, wgrywając drugi obraz z instalatora.

### Czym to się różni od integracji ESPHome Modbus dla mojego falownika?

Obie wysyłają polecenia na magistralę Modbus falownika. Integracja ESPHome interpretuje odpowiedzi i
zamienia je na encje Home Assistanta, które potem musisz sam powiązać z GbbOptimizerem. GbbDongle nie
dotyka danych: odbiera zapytanie, wysyła je, czyta odpowiedź, odsyła ją do GbbOptimizera i na tym koniec.
Dlatego jeden firmware pasuje do każdego falownika, z którym GbbOptimizer potrafi rozmawiać.

## Chmura, łączność i bezpieczeństwo

### Czy mogę zachować Deye Cloud albo oryginalny logger WiFi?

Tak. Fabryczny logger WiFi zostaje w swoim gnieździe i dalej działa, możesz go też wyjąć. Pamiętaj, że
logger typu Solarman robi jedną rzecz naraz. Obciążanie go jednocześnie zapytaniami z Home Assistanta i
GbbOptimizera powoduje przekroczenia czasu, co często jest powodem przejścia na dongle.

### Czy bez oryginalnego loggera producent może dalej aktualizować mój falownik?

Nie. Po wyjęciu loggera jedyną drogą wysyłania poleceń do falownika jest GbbOptimizer, który wysyła je do
dongla, a ten przekazuje je falownikowi.

### Czy dostanę e-mail, gdy dongle straci połączenie?

Tak. Dongle co 60 sekund wysyła sygnał życia, a GbbOptimizer ma limity czasu na wysyłane polecenia, więc
serwer zauważy przerwę i może wysłać e-mail. Opóźnienie ustawisz w parametrach instalacji (**Wyślij email
po utracie połączenia**).

### Co się stanie, gdy padnie internet?

GbbOptimizer może razem z ustawieniami godzinowymi wysłać awaryjny zestaw poleceń („last will”). Jeśli do
10. minuty godziny nie przyjdą nowe ustawienia, dongle sam wykona ten zestaw na falowniku. O jego treści
decyduje GbbOptimizer. Domyślnie jest trzymany tylko w RAM. Włącz **Persist Emergency Commands**, jeśli ma
przetrwać zanik zasilania albo restart dongla. Szczegóły są w [protocol.md](protocol.md).

### Po awarii routera albo internetu dongle nie wrócił

Firmware do wersji 2.0.1 nie restartuje się sam po utracie sieci lub połączenia z chmurą. Odłączenie i
ponowne włączenie zasilania przywraca go do działania; inteligentne gniazdko pozwala zrobić to zdalnie.
Przed restartem możesz podłączyć się przez USB i odczytać log w instalatorze, żeby zobaczyć stan
interfejsu sieciowego.

## Falowniki i baterie

### Czy zadziała z moim falownikiem?

Sam dongle nie zależy od protokołu. To, czy falownik zadziała, zależy od GbbOptimizera: potrzebuje on
tabeli rejestrów Modbus falownika i obsługi po swojej stronie. Deye jest obsługiwany. O inne marki pytaj
na kanale; Hoymiles HIT-G3 i hybrydy FoxESS nie były obsługiwane w październiku 2026.

### Czy marka baterii ma znaczenie?

Nie. Dongle i GbbOptimizer rozmawiają z falownikiem, nie z baterią, więc falownik Deye działa niezależnie
od tego, jaka bateria za nim stoi.

## Aktualizacje

### Jak działają aktualizacje?

Od wersji 2.0.1 publikowany firmware aktualizuje się sam. Co 6 godzin sprawdza nowe wydanie i instaluje
je po losowym opóźnieniu od 0 do 5 godzin. Urządzenia z wersją 2.0.0 lub starszą trzeba raz zaktualizować
ręcznie. Urządzenia przejęte do własnego panelu ESPHome aktualizujesz sam.

### Dostałem wersję testową z GitHub Actions. Który plik wgrać?

Użyj pliku OTA, którego nazwa zawiera Twoją płytkę i wersję firmware, np.
`ota-gbbdongle-tcan485-factory` dla LilyGo T-CAN485 z firmware Factory, i wgraj go przez panel WWW
dongla. Pliki `gbbdongle` bez przyrostka są dla Waveshare ESP32-S3-RS485-CAN. Wgranie pliku zbudowanego
dla innej płytki unieruchamia urządzenie, dopóki nie wgrasz firmware ponownie przez USB.
