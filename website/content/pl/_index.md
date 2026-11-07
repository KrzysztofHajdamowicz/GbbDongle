---
title: GbbDongle
description: Gotowy most RS485 między falownikiem a GbbOptimizerem. Bez komputera, bez Dockera, 0,5 W.
hero:
  eyebrow: Dla użytkowników GbbOptimizera
  title: Falownik w GbbOptimizerze, **bez komputera** w tle
  lead: GbbDongle to mała płytka na szynę DIN, która łączy port RS485 Twojego falownika prosto z chmurą GbbOptimizera. Zastępuje GbbConnect2 na Windowsie, w Dockerze czy dodatek do Home Assistanta.
  note: Gotowe, wgrane i przetestowane urządzenie albo zrób to sam z płytki z listy.
how:
  title: Jak to działa
  aria: Falownik połączony przez RS485 z GbbDongle, który łączy się przez internet z GbbOptimizerem
  nodes:
    - title: Falownik
      text: port Modbus RS485
    - title: GbbDongle
      text: ESP32 na szynie DIN
      accent: true
    - title: GbbOptimizer
      text: chmura, MQTT po TLS
  links:
    - kabel RJ45
    - WiFi lub Ethernet
  text: GbbOptimizer wysyła ramki Modbus, dongle wykonuje je na magistrali falownika i odsyła odpowiedzi. Cała logika zostaje w chmurze, więc ten sam firmware działa z każdym falownikiem, który obsługuje GbbOptimizer.
features:
  title: Dlaczego GbbDongle
  items:
    - icon: "⚡"
      title: 0,5 W i zero komputera
      text: Nie potrzebujesz serwera, Raspberry Pi ani włączonego 24/7 PC. Dongle pracuje sam, wpięty w rozdzielnicę.
    - icon: "🔄"
      title: Aktualizuje się sam
      text: Co 6 godzin sprawdza nowe wydanie i instaluje je z losowym opóźnieniem, żeby cała flota nie restartowała się naraz.
    - icon: "🛟"
      title: Polecenia awaryjne
      text: Gdy internet padnie, dongle sam wykona na falowniku zestaw poleceń awaryjnych przygotowany przez GbbOptimizer.
    - icon: "🔓"
      title: Otwarte źródła
      text: Firmware oparty o ESPHome, kod na GitHubie, podpisane wydania z SBOM. Możesz też zbudować własny.
    - icon: "🏠"
      title: Home Assistant, jeśli chcesz
      text: Druga wersja firmware zgłasza się do adopcji w ESPHome. Domyślna wersja działa bez Home Assistanta.
    - icon: "🔌"
      title: WiFi albo Ethernet z PoE
      text: Słabe WiFi przy falowniku? Wersja z Ethernetem dostaje zasilanie tym samym kablem.
devices:
  title: Wybierz urządzenie
  lead: Wszystkie modele dostają ten sam firmware. Różnią się łącznością i tym, ile miejsca zajmują w rozdzielnicy.
  more: Porównaj urządzenia
  items:
    - image: img/waveshare-esp32-s3-rs485-can.jpg
      name: Waveshare ESP32-S3-RS485-CAN
      variant: antena zewnętrzna
      pill: W gotowym GbbDongle
      text: Najlepszy zasięg WiFi. Wymaga rozdzielnicy dwurzędowej.
    - image: img/waveshare-esp32-s3-rs485-can.jpg
      name: Waveshare ESP32-S3-RS485-CAN
      variant: antena wbudowana
      text: Mieści się w każdej rozdzielnicy. Do miejsc z dobrym WiFi.
    - image: img/waveshare-esp32-s3-poe-eth-8di-8do.jpg
      name: Waveshare ESP32-S3-POE-ETH-8DI-8DO
      variant: Ethernet / PoE
      text: Sieć i zasilanie jednym kablem. Tam, gdzie WiFi nie dociera.
steps:
  title: Od pudełka do działania w 4 krokach
  items:
    - title: Wgraj firmware
      text: Przez USB, prosto z przeglądarki Chrome lub Edge. Gotowe urządzenie ma go już wgranego.
    - title: Podłącz do sieci
      text: WiFi skonfigurujesz z instalatora albo przez punkt dostępowy *GbbDongle Setup*. Ethernet działa od razu.
    - title: Wpisz dane z GbbOptimizera
      text: Serwer MQTT, identyfikator instalacji i token w panelu WWW dongla.
    - title: Wepnij w falownik
      text: Kabel RJ45 do portu Modbus. Zobacz pinouty dla swojego falownika.
cta:
  title: Gotowy GbbDongle za 399 zł brutto
  text: Złożony, wgrany i przetestowany, z zewnętrzną anteną WiFi. Faktura VAT.
---
