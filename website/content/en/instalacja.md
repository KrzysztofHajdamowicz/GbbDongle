---
title: Install the firmware
slug: install
description: "Pick your device and firmware line, then flash it over USB straight from your browser. The ready-made GbbDongle comes flashed: start at \"Connect to WiFi\" or \"Connect Ethernet\"."
layout: installer
menu:
  main:
    name: Install
    weight: 20
---

{{< installer >}}

## Step-by-step guide {#guide-title}

<p id="guide-hint" class="hint">Pick your device and firmware line above and the guide adapts to your choice.</p>

<div id="guide" class="guide" hidden>

{{< step title="Get a computer and a cable ready" >}}
- A computer with a browser that supports Web Serial: **Chrome**, **Edge**, **Opera** or **Firefox** 151 or newer. Safari cannot do it; the installation has to be done from a computer, not a phone.
- A **USB-C cable that carries data**. A charge-only cable will not show a port.
- USB power is enough for flashing, on the Ethernet boards too.
{{< only device="kamami" >}}
**Kamami:** remove the RS485 HAT before connecting USB. The USB converter and the HAT share the same pins; never connect both at once.
{{< /only >}}
{{< /step >}}

{{< step title="Flash the firmware" >}}
1. Connect the board over USB.
2. Click **Install firmware** in step 3 of the wizard above.
3. Pick the port that appeared when you plugged the board in (e.g. `COM8` on Windows, `cu.usbmodem…` on macOS).
4. On a first install, tick **Erase device**.
5. Wait for the progress bar to finish. It takes about a minute.
{{< /step >}}

{{< step title="Connect to WiFi" net="wifi" >}}
The easiest way is right in the installer: after flashing choose **Configure Wi-Fi**, pick your network and enter the password.

You can also do it later: a dongle without WiFi settings opens the **GbbDongle Setup** access point. Join it from your phone and the setup page opens by itself.
{{< only device="8di8do-wifi" >}}
**8DI-8DO:** the module has no antenna on the board. WiFi will not work without the SMA antenna screwed on.
{{< /only >}}
{{< /step >}}

{{< step title="Connect Ethernet" net="eth" >}}
{{< only device="kamami" >}}
Unplug USB, mount the RS485 HAT, and only then connect power.
{{< /only >}}
{{< only device="8di8do-eth" >}}
Plug in the network cable. A **PoE** switch (802.3af) powers the board; USB or the 7–36 V terminals work too.
{{< /only >}}
{{< only device="kamami" >}}
Plug in the network cable. A **PoE** switch or a 5 V supply powers the board.
{{< /only >}}
The dongle gets its IP address from your router (DHCP).
{{< /step >}}

{{< step title="Open the dongle's web panel" >}}
Type {{< hostname >}} into your browser.

If `.local` names do not work on your network, look the IP address up in your router's DHCP list. You can also connect the dongle over USB, open this installer, choose **Logs & Console** and read the IP address from the log.
{{< only line="factory" >}}
Log in as `admin` / `admin` and set your own password under **System** → **Panel password** right away. The same password protects firmware uploads. **Restore factory settings** restores all defaults, including the `admin` password.
{{< /only >}}
{{< /step >}}

{{< step title="Connect to GbbOptimizer" >}}
1. In GbbOptimizer open **Instalacje** (Installations), click **Popraw** (Edit) next to your installation and switch the installation type at the top to **GbbDongle**. Until you do, GbbOptimizer talks to the inverter the old way (e.g. through Deye Cloud) and the dongle receives nothing.
2. In the dongle's panel open the **GbbOptimizer cloud** tab and copy the MQTT server, **Plant Id** and token from the installation. Never post the token publicly.
3. Click **Save**, then **Apply and restart**. After about fifteen seconds **Overview** shows "Everything works".

Older firmware has the stock ESPHome panel with English entity names: there the fields are **MQTT Server**, **Plant Id** and **Plant Token**, and the button is **Apply Settings (Restart)**.
{{< /step >}}

{{< step title="Add to Home Assistant (optional)" line="ha" >}}
The dongle shows up as *Discovered* in the ESPHome dashboard next to Home Assistant. Click **Adopt**, or add it to Home Assistant directly; the API encryption key is created on the first connection.

After adoption your ESPHome dashboard compiles the firmware and updates are up to you. You can go back to the Factory firmware from this installer at any time.
{{< /step >}}

{{< step title="Connect to the inverter" >}}
Make the cable from a ready-made Ethernet cable with one plug cut off: the factory-crimped plug goes into the inverter, and the RS485 wires go into the dongle's **A**, **B** and optionally **GND** terminals. Count pin numbers; do not go by wire colours.

The [pinouts]({{< relref "pinouts" >}}) section shows which port and which pins to use. The port defaults to 9600 baud, 8N1; you can change it in the panel without a restart.
{{< /step >}}

{{< step title="Check that it works" >}}
In GbbOptimizer, in **Prognoza baterii** (Battery forecast), choose **Operacje → Pobierz wszystkie dane z instalacji** (download all data from the installation) and watch the dongle's log. You should see requests from GbbOptimizer and answers from the inverter; a line starting with `01 03` is a read from the inverter at Modbus address 1.

Requests without answers almost always mean wiring: check the cable with a tester and try swapping A and B. More in the [FAQ]({{< relref "faq" >}}).
{{< /step >}}

</div>
