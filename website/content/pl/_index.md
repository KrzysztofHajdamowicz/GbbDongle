---
title: GbbDongle
description: Gotowy most RS485 między falownikiem a GbbOptimizerem. Bez komputera, bez Dockera, 0,5 W.
hero:
  eyebrow: Dla użytkowników GbbOptimizera
  title: Falownik podłączony do GbbOptimizera, **bez komputera** w tle
  lead: "Pomiń zawodną chmurę producenta! GbbDongle łączy GbbOptimizer bezpośrednio z portem RS485 falownika, więc sterowanie działa szybko i niezależnie od cudzych serwerów. Bez komputera, Dockera czy Raspberry Pi włączonego 24/7: wystarczy mała płytka w rozdzielnicy."
  note: Gotowe, wgrane i przetestowane urządzenie albo zrób to sam używając jednego z wspieranych modułów.
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
  links:
    - kabel RJ45
    - WiFi lub Ethernet
  text: GbbOptimizer wysyła ramki Modbus, dongle wykonuje je na magistrali falownika i odsyła odpowiedzi. Cała logika zostaje w chmurze, więc ten sam firmware działa z każdym falownikiem, który obsługuje GbbOptimizer.
features:
  title: Dlaczego GbbDongle
  items:
    - icon: "⚡"
      title: Energooszczędność
      text: Nie potrzebujesz serwera, Raspberry Pi czy PC włączonego 24/7. Dongle pracuje sam, zasilany przewodem USB.
    - icon: "🔄"
      title: Aktualizuje się sam
      text: Wystarczy jedynie wstępna konfiguracja i urządzenie będzie się aktualizować automatycznie.
    - icon: "🛟"
      title: Polecenia awaryjne
      text: Co godzinę wysyłany jest zestaw poleceń zapasowych, dzięki czemu Twój falownik będzie realizował bezpieczny program w razie niedostępności chmury.
    - icon: "🔓"
      title: Otwarte i audytowalne źródła
      text: Oprogramowanie zawiera najnowsze wytyczne z zakresu cyberbezpieczeństwa.
    - icon: "🏠"
      title: Home Assistant, jeśli chcesz
      text: Nie odwracamy się od zapaleńców. Twój GbbDongle może działać w Home Assistant.
    - icon: "🔌"
      title: WiFi albo Ethernet z PoE
      text: Słabe WiFi przy falowniku? W ofercie jest również wersja Ethernet. Dane lub Dane+Zasilanie płynące jednym kablem.
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
  title: Od pudełka do działania w 3 krokach
  items:
    # - title: Wgraj firmware
    #   text: Przez USB, prosto z przeglądarki Chrome, Edge albo Firefox. Gotowe urządzenie ma go już wgranego.
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
