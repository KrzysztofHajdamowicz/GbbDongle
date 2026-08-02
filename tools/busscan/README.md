# busscan — skaner urządzeń Modbus na szynie za GbbDonglem

Jednorazowe narzędzie diagnostyczne: sprawdza, które adresy Modbus (falowniki
Deye) są widoczne na szynie RS485 podłączonej do GbbDongle'a. Uruchamia lokalny
broker MQTT (port 1883, przyjmuje dowolne dane logowania), a po podłączeniu się
dongla wysyła przez niego ramki "odczytaj rejestry 0–7" (typ urządzenia, adres,
numer seryjny) na kolejne adresy slave i drukuje tabelę wyników.

Niczego nie zapisuje do falowników — wykonywane są wyłącznie odczyty (funkcja
Modbus 0x03).

## Uruchomienie (tryb ręczny)

1. Uruchom binarkę dla swojego systemu (macOS: najpierw
   `xattr -d com.apple.quarantine busscan-darwin-*`, bo binarka nie jest
   podpisana; Windows: zezwól w SmartScreen).
2. Narzędzie wypisze listę adresów IP komputera. W web UI dongla
   (`http://gbbdongle.local`, `http://gbbdongle-tcan485.local`,
   `http://gbbdongle-kamami.local`, `http://gbbdongle-8di8do-wifi.local` albo
   `http://gbbdongle-8di8do-eth.local`) **zanotuj obecne wartości**, po czym
   ustaw:
   - `MQTT Server` = IP komputera z listy (ta sama sieć co dongle),
   - `MQTT Port` = `1883`,
   - `TLS` = off,
   - `Cloud Connection` = on,

   i wciśnij `Apply Settings (Restart)`.
3. Po restarcie dongle podłączy się do narzędzia i skan ruszy automatycznie.
4. **Po teście przywróć zanotowane ustawienia** w web UI i ponownie wciśnij
   `Apply Settings (Restart)`.

## Tryb automatyczny

Jeśli komputer widzi web UI dongla, całą podmianę i przywrócenie konfiguracji
narzędzie zrobi samo (także po przerwaniu Ctrl-C):

```bash
./busscan --dongle gbbdongle.local
```

Zamiast nazwy mDNS można podać adres IP dongla.

## Opcje

| Flaga | Znaczenie |
|---|---|
| `--dongle host` | automatyczna rekonfiguracja + przywrócenie ustawień |
| `--full` | pełny skan adresów 1–247 (domyślnie: stop po pierwszym niemym adresie) |
| `--soc` | dodatkowo odczytaj % naładowania baterii (rejestr 588) |
| `--ip a.b.c.d` | wymuś IP ogłaszane donglowi (domyślnie: autodetekcja) |
| `--port n` | port lokalnego brokera (domyślnie 1883) |
| `--timeout d` | timeout na odpowiedź MQTT od dongla (domyślnie 5s) |
| `--wait d` | ile czekać na podłączenie dongla (domyślnie 120s) |
| `-v` | szczegółowe logi (payloady MQTT) |

Kod wyjścia: `0` = znaleziono ≥1 urządzenie, `1` = nic nie znaleziono,
`2` = błąd (broker, REST, przerwanie).

Przykładowy wynik:

```
ADDR  SERIAL      TYPE                        PROTO   NOTE
1     2301234501  LV 3-phase hybrid (0x0500)  0x0102
2     2301234502  LV 3-phase hybrid (0x0500)  0x0102

2 device(s) found.
```

Wiersz z notą `Modbus exception ...` oznacza, że pod adresem coś odpowiada,
ale odrzuciło odczyt — urządzenie **jest** na szynie.

## Budowanie

```bash
./build.sh
```

Binarki lądują w `dist/` (windows/amd64, linux/amd64+arm64, darwin/amd64+arm64).
Na macOS powstaje dodatkowo `busscan-darwin-universal` (fat binary sklejona
przez `lipo`) — jeden plik działający na Intelu i Apple Silicon; testerowi
z Makiem wysyłaj ten.

## Test bez sprzętu

```bash
go test ./...
./busscan &
uv run python tools/busscan/fake_dongle.py          # z katalogu głównego repo
```

`fake_dongle.py` symuluje dongla z dwoma falownikami (adresy 1 i 2); z flagą
`--rest-stub 6052` serwuje też endpointy REST web_servera, więc tryb
`--dongle localhost:6052 --ip 127.0.0.1` można przećwiczyć na sucho.
