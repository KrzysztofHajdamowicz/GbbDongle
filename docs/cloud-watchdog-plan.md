# Cloud watchdog — plan i handoff

Stan na 2026-10-03. Dokument roboczy — kierunek nie jest jeszcze
potwierdzony, patrz „Handoff” niżej.

## Handoff — gdzie jesteśmy

- **Zrobione:** krok 0 (refaktor neutralny) — maszyna stanów awaryjnych
  wydzielona z `gbb_dongle.cpp` do `components/gbb_dongle/emergency_manager.{h,cpp}`.
  Zachowanie bez zmian, YAML-owe API bez zmian. Metody `is_pending()`
  potrzebnej watchdogowi **celowo nie dodano** — czeka na decyzję kierunkową.
- **Wstrzymane:** kroki 1–3 (klasa `CloudWatchdog`, encje, dokumentacja).
  Właściciel chce się namyśleć, czy kierunek (restart odraczany przez zestaw
  awaryjny, bez handoveru NVS, bez backoffu) jest właściwy.
- **Do rozważenia przy powrocie:**
  - czy zamiast odraczania nie lepszy jest jednak handover setów RAM-only
    przez NVS tuż przed restartem (jednorazowy zapis, zero utraty „last will”);
  - czy przy Persist OFF restart co 30 min przy błędnych danych logowania /
    długiej awarii chmury jest akceptowalny bez backoffu;
  - czy nie zmienić domyślnej wartości „Persist Emergency Commands” na ON
    (wtedy watchdog jest bezstratny niezależnie od wariantu).

## Kontekst

User story: po restarcie falownika (niskie SOC) zresetował się router i
Waveshare 8DI-8DO (Ethernet). Połączenie z chmurą „już nie wstało” i
urządzenie wymagało ręcznego odłączenia zasilania.

Co pokazało badanie kodu (esphome 2026.9.0):

- **Firmware nie ma dziś żadnego watchdoga.** Wszystkie wbudowane timery są
  wyzerowane: `api: reboot_timeout: 0s` (`base.yaml:44`),
  `mqtt: reboot_timeout: 0s` (`base.yaml:79`), `wifi: reboot_timeout: 0s`
  (`wifi.yaml:6`). Komponent `ethernet:` **w ogóle nie ma** `reboot_timeout`
  — na link-down tylko próbuje reconnectu co 15 s, w nieskończoność.
- **Stockowy `mqtt: reboot_timeout` i tak by nie pomógł.** `GbbDongle::loop()`
  woła `mqtt_->enable()` dopiero gdy `network::is_connected()`; a
  `MQTTClientComponent::loop()` w stanie DISABLED robi `return` przed
  sprawdzeniem timera (`mqtt_client.cpp:380-381`). Jeśli sieć nigdy nie
  wstanie, stockowy watchdog nigdy się nie uzbroi. Nasz musi liczyć od startu
  urządzenia.
- **Jedynym pewnym lekarstwem jest restart.** Backend esp-mqtt inicjalizuje
  klienta raz (`MQTTBackendESP32::connect()` → `if (!is_initalized_)`,
  `mqtt_backend_esp32.h:177-183`); późniejsze reconnecty to wyłącznie
  wewnętrzny auto-reconnect esp-mqtt. Maszyna stanów ESPHome jest tylko
  obserwatorem. `disable()` + `enable()` **nie** jest „miękkim restartem”:
  `disable()` robi `esp_mqtt_client_disconnect`, a ponowne `connect()` jest
  no-opem — połączenie zostałoby zerwane na stałe. Nie używać w runtime.
- **Zestawy awaryjne a restart.** Przy Persist OFF (domyślnie) sety żyją
  tylko w RAM i przepadają przy każdym restarcie. Przy Persist ON wracają z
  NVS, a deadline godzinowy liczy się od pierwszej synchronizacji zegara
  (`boot_loaded_awaiting_time_`). Po **soft-resecie** (`esp_restart`) ESP-IDF
  zachowuje czas systemowy w rejestrach RTC (domyślne
  `LIBC_TIME_SYSCALL_USE_RTC_HRT`), więc po restarcie watchdoga zegar jest
  poprawny od razu (do potwierdzenia na benchu).
- Czas realizacji zestawu: per linia `read_gap` 100 ms lub `write_gap` 3 s +
  do 1 s `response_timeout` — pojedynczy cykl to sekundy. Długie czekanie to
  stan **ARMED** (deadline do ~71 min od utraty chmury) i **BACKOFF** (retry
  1→2→4→8→15→15… min bez końca, gdy falownik nie odpowiada).

## Decyzje robocze (do potwierdzenia)

| Temat | Decyzja |
|---|---|
| Zestaw awaryjny oczekujący, sety RAM-only | Odraczaj restart, bez handoveru przez NVS; po twardym limicie restart mimo wszystko (sety RAM-only przepadają, jak w GbbConnect2 po restarcie PC). |
| Twardy limit odraczania | 2 h od momentu, gdy restart stał się należny (knob YAML `cloud_watchdog_max_defer`). |
| Timeout domyślny | 30 min, domyślnie ON na całej flocie (po następnym OTA). |
| Konfiguracja | Encja Number „Cloud Watchdog” (minuty, 0 = wyłączony, zmiana na żywo). |
| Backoff kolejnych restartów | Nie — stały odstęp. |
| Struktura kodu | `EmergencyManager` (zrobione) + nowa klasa `CloudWatchdog`, osobne pliki wg wzorca `ModbusExecutor`/`EmergencyStore`. Jeden komponent ESPHome. |

## Projekt watchdoga

- **Uzbrojony**, gdy `cloud_configured_` **i** Cloud Connection ON (stan czytany
  na żywo) **i** timeout > 0. Świeże urządzenie bez danych chmury nigdy nie
  restartuje.
- **Zegar offline** startuje przy pierwszym `loop()` z uzbrojonym watchdogiem
  (≈ boot, niezależnie czy sieć kiedykolwiek wstanie), odświeżany w każdej
  pętli, w której `mqtt_->is_connected()`.
- **Restart należny**, gdy offline ≥ timeout. Timeout < 5 min podnoszony w
  kodzie do 5 min (DHCP + TLS po boocie to dziesiątki sekund; ochrona przed
  boot-loopem). 0 = wyłączony.
- **Odraczanie** (po stanie „należny”), dopóki `executor_.busy()` (nigdy w
  trakcie ramki Modbus) **lub** `EmergencyManager::is_pending()` — sety
  zapisane i wysyłka może jeszcze nastąpić: `QUEUED`/`EXECUTING`/`BACKOFF`,
  albo `ARMED` z aktywnym deadline'em (`last_inv_setup_ts_ != 0` lub flaga
  `*_awaiting_time_`). Łącznie nie dłużej niż `cloud_watchdog_max_defer`.
- **Restart**: licznik do NVS (`ESPPreferenceObject`, jak `log_level_pref_`),
  `ESP_LOGE`, `App.safe_reboot()` (teardown ≤ 1 s; po starcie komponent
  ethernet resetuje W5500 przez `reset_pin` GPIO39).
- **Po restarcie**: w `setup()` log „Cloud watchdog has rebooted this device
  N time(s)” — ring buffer logów ginie, to jedyny ślad dla `LastLog`.
- `millis()` wrap-safe: `(int32_t)(now - deadline) >= 0`.
- `safe_mode:` liczy tylko boot-y padające w ciągu 1 min — restart po ≥ 5 min
  nie wpada do safe mode.

### Interfejs `CloudWatchdog` (szkic)

```cpp
class CloudWatchdog {
 public:
  void set_timeout_number(number::Number *n);   // minuty; 0 = off; nullptr = off
  void set_max_defer(uint32_t ms);              // domyślnie 2 h
  void setup();                                  // pref licznika + log „rebooted N time(s)”
  /// armed: cloud skonfigurowana && switch ON; connected: mqtt_->is_connected();
  /// defer: executor busy || emergency pending.
  void loop(bool armed, bool connected, bool defer);
  uint32_t get_reboot_count() const;
};
```

Glue w `GbbDongle::loop()` po bloku keepalive:

```cpp
const bool armed = cloud_configured_ && (cloud_enabled_ == nullptr || cloud_enabled_->state);
watchdog_.loop(armed, mqtt_->is_connected(), executor_.busy() || emergency_.is_pending());
```

### Schemat / YAML

- `__init__.py`: `cloud_watchdog_id` (`cv.use_id(number.Number)`, opcjonalne;
  brak = watchdog wyłączony), `cloud_watchdog_max_defer` (default `2h`).
- `base.yaml`: `number:` `cfg_cloud_watchdog` „Cloud Watchdog” (min, 0–1440,
  step 1, `mode: box`, `restore_value`, initial 30, `entity_category: config`);
  `gbb_dongle: cloud_watchdog_id: cfg_cloud_watchdog`; `sensor:` „Watchdog
  Reboots” (diagnostic, total_increasing, `id(gbb).get_watchdog_reboots()`).
- `gbbdongle-bench.yaml`: `cloud_watchdog_max_defer: 10min`.

### Dokumentacja

README (tabela encji), `docs/protocol.md` („GbbDongle-specific behavior”:
watchdog vs zestaw awaryjny), AGENTS.md (nowe pliki, knoby, uwaga „nigdy
`mqtt_->disable()` w runtime”). Konsekwencje do opisania: błędne dane
logowania ⇒ restart co 30 min; awaria serwera GbbOptimizer ⇒ restart co
30 min; Persist ON ⇒ watchdog bezstratny.

## Weryfikacja (bench, Waveshare dev unit, `gbbdongle-bench.yaml`)

1. Refaktor neutralny: round trip MQTT→Modbus, `LinesOnNoInvSetup` +
   `IsInvSetup`, trigger przy `emergency_minute_threshold: 1`, Emergency Sets
   Stored zlicza.
2. Watchdog podstawowy: Cloud Watchdog = 5, zatrzymać brokera → restart po
   5 min, licznik 1. To samo z siecią odłączoną od boota.
3. Odraczanie: set + InvSetup, broker stop → „deferring”, set wychodzi po
   progu, restart tuż po „All emergency command sets delivered”.
4. Limit: jak 3, bez slave'a (BACKOFF) → restart po `max_defer`.
5. Negatywne: brak danych chmury / Cloud Connection OFF / timeout 0 → brak
   restartów; zmiana Number bez „Restart Required”.
6. Zegar po soft-resecie: czy `now.is_valid()` od razu po restarcie bez sieci.
7. Przed tagiem: checklista z AGENTS.md.
