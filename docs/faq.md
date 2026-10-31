# GbbDongle FAQ

Answers to the questions that come up most often on the GbbOptimizer Discord. Installation steps and
the board list are in the [README](../README.md); the cloud protocol is in [protocol.md](protocol.md).

## Buying and choosing hardware

### Can I buy a ready-made GbbDongle?

Yes. The author sells an assembled, flashed and tested GbbDongle for 399 PLN gross (VAT invoice
available); ask on the GbbDongle Discord channel. It is a Waveshare ESP32-S3-RS485-CAN on a DIN rail
with an external WiFi antenna.

### Can I build one myself?

Yes, everything is public. Buy one of the [supported boards](../README.md#supported-boards), flash it
from the [web installer](https://krzysztofhajdamowicz.github.io/GbbDongle/) over USB in Chrome or Edge,
and make the inverter cable from an Ethernet cable with one plug cut off (see
[Wiring](#wiring-deye)). The installer page lists each board by its exact name: copy it into your
shop's search box.

### Which board should I pick?

- **WiFi reaches the inverter well:** Waveshare ESP32-S3-RS485-CAN. It mounts on a DIN rail. The
  versions sold in Polish shops (e.g. Allegro) have an internal antenna, which is noticeably weaker than
  the external antenna on the author's units; if your current inverter logger already struggles with
  WiFi, an internal-antenna board will not fix that.
- **Weak or no WiFi at the inverter** (a metal shed is the classic case): run an Ethernet cable and
  either put a WiFi access point there or use an Ethernet board: Waveshare ESP32-S3-POE-ETH-8DI-8DO
  (Ethernet firmware, PoE) or Kamami KAmod ESP32 ETH+PoE. The 8DI-8DO is 8 modules wide on a DIN rail.
- **LilyGo T-CAN485** works, but it is a bare board with no enclosure or DIN mount.

### What does it cost to run?

About 0.5 W. The dongle writes nothing to local storage in normal operation and needs no computer,
unlike GbbConnect2 or its Home Assistant add-on, which need a machine running 24/7.

## Flashing and first setup

### Do I need extra power to flash the board?

No, USB alone is enough, for the Ethernet boards too. Firmware is always flashed over USB the first
time; later updates arrive over the network.

### The installer asks whether to erase the device

On a first install, tick it. In the port list, pick the serial port that appears when you plug the board
in (e.g. `COM8` on Windows).

### How do I find the dongle on my network?

WiFi boards: configure WiFi from the installer, or join the `GbbDongle Setup` access point. Ethernet
boards get an address over DHCP. Then open the web panel:

- by name, e.g. `http://gbbdongle-8di8do-eth.local`;
- if `.local` names do not resolve on your network, look the IP up in your router's DHCP list, or
  connect the board over USB, open the web installer, choose **Logs & Console** and find the IP address
  in the log (with the Ethernet cable plugged in).

The web panel login is `admin` / `admin` (Factory firmware). Change it with the **Admin Password**
field; saving an empty password restores `admin`, and **Factory Reset** restores all defaults.

### What do I configure in GbbOptimizer?

1. In **Instalacje** (Installations), open your installation with **Popraw** (Edit) and switch the
   installation type at the top to **GbbDongle**. Until you do, GbbOptimizer keeps talking to the
   inverter through its previous method (e.g. Deye Cloud) and the dongle receives nothing.
2. Copy the MQTT server address, the installation id (Plant Id) and the access token from the
   installation into the dongle's web panel and press **Apply Settings (Restart)**. Never post the
   token publicly.
3. Use your real, active installation straight away. Switching costs at most about an hour of
   inverter control if something goes wrong.

### How do I check that it works?

In GbbOptimizer, in **Prognoza baterii** (Battery forecast), run **Operacje → Pobierz wszystkie dane z
instalacji** (download all data from the installation) and watch the dongle's log: you should see
requests arriving from GbbOptimizer and responses coming back from the inverter. A line starting with
`01 03` is a read from the inverter at Modbus address 1. Requests with no answer usually mean a wiring
problem (see below).

## Wiring (Deye)

### Which inverter port do I use?

The RJ45 port labelled **Modbus** (on the SG04LP3/SG05LP3 models, the second from the right). Users have
also run the dongle on the **Meter-485** port while another device used the Modbus port. Do not use the
**BMS** or **CAN** ports.

SolarAssistant's guides show the RS485 pinout for each Deye model, e.g.
[SG04LP3](https://solar-assistant.io/help/inverters/deye/SG04LP3/rs485) and
[SG05LP3](https://solar-assistant.io/help/inverters/deye/SG05LP3/rs485).

### How do I make the cable?

Take a ready-made Ethernet cable and cut the plug off one end. Keep the factory-crimped plug on the
inverter side, since it is almost always wired correctly. Strip the cut end and screw the RS485 wires
into the dongle's terminals, which are labelled **A**, **B** and **GND**. You need 2 or 3 of the 8 wires
(A, B and optionally GND). Count pin numbers; do not go by the Ethernet colour code. An RJ45-to-terminal
adapter also works, but it usually stops the inverter's cover from closing.

If the dongle gets no answer from the inverter, check the cable with a tester and try swapping A and B.

### The RS485 port is shared with my battery

On some models one RJ45 socket carries both the battery's CAN link and Modbus. Make your own split
cable: the CAN pair goes to the battery, the Modbus pair to the dongle.

### What do I set on the inverter?

In the inverter's advanced settings, find the Modbus settings: the inverter must be **master** with
Modbus address **01** and **9600** baud. The dongle defaults to 9600 baud 8N1 and can be changed live in
its web panel.

### Can the dongle share the bus with SolarAssistant, an ESPHome Modbus integration or another reader?

Not on the same wires. Only one device on an RS485 bus may actively ask for data. This is not Ethernet,
where everyone talks to everyone. Put each reader on its own port (e.g. the dongle on Meter-485 and the
other device on Modbus), or move the other tools to Modbus TCP.

## Several inverters (Deye master/slave)

### Does one dongle work with several Deye inverters?

Yes, since October 2026, with Deye only. All inverters share one RS485 bus, and each answers at its
own Modbus address.

- **Topology:** a daisy chain, not a star. The bus starts at the dongle, runs through every inverter and
  ends with a 120 Ω terminator after the last one. Order on the bus does not matter.
- **Addresses:** set in each inverter's settings. The master has address 1, slaves have 2, 3 and so on.
- **This bus is in addition to** the inverters' own parallel link, not a replacement for it.
- **Wiring with Cat5e:** pins 1 and 2 of the first inverter go to the dongle, and that inverter's pins
  7 and 8 go to pins 1 and 2 of the next one (Deye SUN-14-20K-SG05LP3 manual, page 51). Line A is always
  A and line B always B: nothing is crossed. In the first working installation, a common GND between the
  inverters was also essential.
- **In GbbOptimizer:** set the installation type to GbbDongle. In the slave list, enter only the slave
  inverters (Modbus address 2 or higher), each with its serial number and Modbus unit. Do not list the
  master as a slave: that doubles production and other figures.
- **Check:** the dongle's log should show reads from every address, e.g. `01 03` and `02 03`. If one
  address never answers, fix the wiring.

## Home Assistant

### Does GbbDongle send inverter data to Home Assistant?

No. The dongle only relays Modbus frames between GbbOptimizer and the inverter and does not interpret
them, so it has no inverter readings to publish.

The **HA-compatible** firmware exposes the dongle itself to Home Assistant through the native ESPHome
API: its own entities and the board's GPIOs, which you can use as you like. After adoption you build
and maintain that firmware yourself (bring your own support). You can switch between the Factory and
HA-compatible firmware at any time by flashing the other image from the web installer.

### How is this different from an ESPHome Modbus integration for my inverter?

Both put commands on the inverter's Modbus bus. An ESPHome inverter integration interprets the answers
and turns them into Home Assistant entities, which you then have to map onto GbbOptimizer yourself.
GbbDongle does not touch the data: it receives a request, sends it, reads the answer, returns it to
GbbOptimizer, and that is all. That is why one firmware fits any inverter that GbbOptimizer knows how to
talk to.

## Cloud, connectivity and safety

### Can I keep Deye Cloud / the original WiFi logger?

Yes. The factory WiFi logger stays in its own socket and keeps working, or you can remove it. Keep in
mind that a Solarman-type logger does one thing at a time. Loading it with Home Assistant and
GbbOptimizer requests at once causes timeouts, which is a common reason to switch to the dongle.

### Without the original logger, can the manufacturer still update my inverter?

No. With the logger removed, the only way to send commands to the inverter is through GbbOptimizer,
which sends commands to the dongle, which passes them to the inverter.

### Will I get an email if the dongle loses its connection?

Yes. The dongle sends a keepalive every 60 seconds, and GbbOptimizer has timeouts on the commands it
sends, so the server notices the outage and can email you. Set the delay in the installation
parameters (**Wyślij email po utracie połączenia**, email after connection loss).

### What happens if the internet goes down?

GbbOptimizer can send an emergency ("last will") command set together with the hourly settings. If no
new settings arrive by minute 10 of the hour, the dongle runs that set on the inverter itself. What the
set contains is decided in GbbOptimizer. By default it is kept in RAM only. Turn on **Persist Emergency
Commands** if it should survive a power cut or restart of the dongle. Details are in
[protocol.md](protocol.md).

### After a router or internet outage, the dongle did not come back

Firmware up to 2.0.1 does not restart itself when the network or cloud connection is lost. A power
cycle brings it back; a smart plug lets you do that remotely. Before restarting, you can connect over
USB and read the log in the web installer to see the state of the network interface.

## Inverters and batteries

### Does it work with my inverter?

The dongle itself is protocol-agnostic. Whether an inverter works depends on GbbOptimizer: it needs
the inverter's Modbus register table and code on GbbOptimizer's side. Deye is supported. For other
brands, ask on the channel; Hoymiles HIT-G3 and FoxESS hybrids were not supported as of October 2026.

### Does the battery brand matter?

No. The dongle and GbbOptimizer talk to the inverter, not to the battery, so a Deye inverter works
whatever battery is behind it.

## Updates

### How do updates work?

Since 2.0.1 the published firmware updates itself. It checks for a new release every 6 hours and
installs it after a random 0–5 hour delay. Devices on 2.0.0 or older need one manual update. Devices
adopted into your own ESPHome dashboard are updated by you.

### I was given a test build from GitHub Actions. Which file do I flash?

Use the OTA file whose name contains your board and firmware line, e.g.
`ota-gbbdongle-tcan485-factory` for a LilyGo T-CAN485 on the Factory firmware, and upload it through
the dongle's web panel. The unsuffixed `gbbdongle` files are for the Waveshare ESP32-S3-RS485-CAN.
Flashing a file built for a different board leaves the device unusable until you reflash it over USB.
