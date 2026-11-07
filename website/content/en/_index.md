---
title: GbbDongle
description: A ready-made RS485 bridge between your inverter and GbbOptimizer. No computer, no Docker, 0.5 W.
hero:
  eyebrow: For GbbOptimizer users
  title: Your inverter in GbbOptimizer, **no computer** required
  lead: "Skip the inverter maker's unreliable cloud. GbbDongle connects GbbOptimizer straight to your inverter's RS485 port, so control is fast and independent of anyone else's servers. No computer, Docker or Raspberry Pi running 24/7: just a small board in your distribution board."
  note: Buy it assembled, flashed and tested, or build your own from a supported board.
how:
  title: How it works
  aria: Inverter connected over RS485 to GbbDongle, which connects over the internet to GbbOptimizer
  nodes:
    - title: Inverter
      text: Modbus RS485 port
    - title: GbbDongle
      text: ESP32 on a DIN rail
      accent: true
    - title: GbbOptimizer
      text: cloud, MQTT over TLS
  links:
    - RJ45 cable
    - WiFi or Ethernet
  text: GbbOptimizer sends Modbus frames, the dongle runs them on the inverter's bus and sends the answers back. All the logic stays in the cloud, so the same firmware works with every inverter GbbOptimizer supports.
features:
  title: Why GbbDongle
  items:
    - icon: "⚡"
      title: 0.5 W and no computer
      text: No server, Raspberry Pi or PC running 24/7. The dongle works on its own, mounted in your distribution board.
    - icon: "🔄"
      title: Updates itself
      text: Checks for a new release every 6 hours and installs it after a random delay, so the fleet never restarts at once.
    - icon: "🛟"
      title: Emergency commands
      text: If the internet goes down, the dongle runs the emergency command set prepared by GbbOptimizer on the inverter by itself.
    - icon: "🔓"
      title: Open source
      text: ESPHome-based firmware, code on GitHub, signed releases with an SBOM. You can build your own too.
    - icon: "🏠"
      title: Home Assistant if you want it
      text: A second firmware line offers itself for adoption in ESPHome. The default firmware works without Home Assistant.
    - icon: "🔌"
      title: WiFi or Ethernet with PoE
      text: Weak WiFi at the inverter? The Ethernet version is powered over the same cable.
devices:
  title: Pick your device
  lead: Every model runs the same firmware. They differ in connectivity and in how much room they take in the distribution board.
  more: Compare devices
  items:
    - image: img/waveshare-esp32-s3-rs485-can.jpg
      name: Waveshare ESP32-S3-RS485-CAN
      variant: external antenna
      pill: In the ready-made GbbDongle
      text: Best WiFi range. Needs a board with at least two rows.
    - image: img/waveshare-esp32-s3-rs485-can.jpg
      name: Waveshare ESP32-S3-RS485-CAN
      variant: internal antenna
      text: Fits any distribution board. For places with good WiFi.
    - image: img/waveshare-esp32-s3-poe-eth-8di-8do.jpg
      name: Waveshare ESP32-S3-POE-ETH-8DI-8DO
      variant: Ethernet / PoE
      text: Network and power over one cable. Where WiFi does not reach.
steps:
  title: From box to running in 4 steps
  items:
    - title: Flash the firmware
      text: Over USB, straight from Chrome, Edge or Firefox. The ready-made device comes flashed.
    - title: Connect to the network
      text: Set up WiFi from the installer or through the *GbbDongle Setup* access point. Ethernet works right away.
    - title: Enter your GbbOptimizer details
      text: MQTT server, installation id and token in the dongle's web panel.
    - title: Plug into the inverter
      text: RJ45 cable to the Modbus port. See the pinouts for your inverter.
cta:
  title: Ready-made GbbDongle for 399 PLN gross
  text: Assembled, flashed and tested, with an external WiFi antenna. VAT invoice.
---
