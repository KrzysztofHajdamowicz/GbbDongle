// GbbDongle web panel, served from flash as /0.js (web_server js_include, see
// firmware/common/webui.yaml). It replaces the stock ESPHome UI, which is
// loaded from oi.esphome.io, with a self-contained panel built on the same
// web_server API:
//   GET  /events                          server-sent events: "ping" (title,
//        ota, log, uptime), "state" (one per entity; the first one carries
//        domain, name, options, limits), "log" (one line, ANSI-coloured)
//   POST /<domain>/<entity name>/<action>  e.g. /text/Plant%20Id/set?value=x
// Entities are addressed by "<domain>/<display name>", the same id the event
// stream sends, so renaming an entity in YAML breaks its place here (and in
// the REST API). An entity this file does not know is shown under
// System > Other with its ESPHome name.
// Values from the device only ever reach the page through textContent.
// ?classic loads the stock ESPHome UI instead (needs internet access).
(() => {
  "use strict";

  if (new URLSearchParams(location.search).has("classic")) {
    const s = document.createElement("script");
    s.src = "https://oi.esphome.io/v3/www.js";
    document.body.appendChild(s);
    return;
  }

  // ---------- helpers ----------
  const $ = (s, r = document) => r.querySelector(s);
  const store = {
    get(k) { try { return localStorage.getItem(k); } catch (e) { return null; } },
    set(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* private mode */ } },
  };
  // h("div", {class: "x", text: "y", onclick: fn}, child, ...)
  function h(tag, attrs, ...kids) {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs || {})) {
      if (v === undefined || v === null || v === false) continue;
      if (k === "text") el.textContent = v;
      else if (k === "class") el.className = v;
      else if (k.startsWith("on")) el.addEventListener(k.slice(2), v);
      else el.setAttribute(k, v === true ? "" : v);
    }
    for (const kid of kids.flat(Infinity)) if (kid !== null && kid !== undefined && kid !== false) el.append(kid);
    return el;
  }
  const ICONS = {
    bolt: '<path d="M13 2 4 14h7l-1 8 9-12h-7z"/>',
    grid: '<rect x="3" y="3" width="7" height="9" rx="1"/><rect x="14" y="3" width="7" height="5" rx="1"/><rect x="14" y="12" width="7" height="9" rx="1"/><rect x="3" y="16" width="7" height="5" rx="1"/>',
    cloud: '<path d="M17.5 19a4.5 4.5 0 1 0-1.4-8.8A6 6 0 0 0 4.3 12 3.5 3.5 0 0 0 6.5 19z"/>',
    inverter: '<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M7 15l3-6 3 6 3-6"/>',
    wifi: '<path d="M2 8.5a15 15 0 0 1 20 0M5 12a10 10 0 0 1 14 0M8.5 15.5a5 5 0 0 1 7 0"/><circle cx="12" cy="19" r="1"/>',
    gear: '<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M4.9 19.1 7 17M17 7l2.1-2.1"/>',
    moon: '<path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"/>',
    sun: '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>',
    auto: '<circle cx="12" cy="12" r="9"/><path d="M12 3a9 9 0 0 1 0 18z" fill="currentColor"/>',
    lines: '<path d="M4 6h16M4 12h16M4 18h10"/>',
    chev: '<path d="M6 15l6-6 6 6"/>',
    check: '<path d="M5 12.5l4.5 4.5L19 7.5"/>',
    alert: '<path d="M12 7v6M12 17h.01"/>',
    dots: '<path d="M6 12h.01M12 12h.01M18 12h.01"/>',
  };
  function icon(name, cls) {
    const s = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    s.setAttribute("viewBox", "0 0 24 24");
    s.setAttribute("fill", "none");
    s.setAttribute("stroke", "currentColor");
    s.setAttribute("stroke-width", "2");
    s.setAttribute("stroke-linecap", "round");
    s.setAttribute("stroke-linejoin", "round");
    s.setAttribute("aria-hidden", "true");
    if (cls) s.setAttribute("class", cls);
    s.innerHTML = ICONS[name]; // constant markup from the table above
    return s;
  }
  const encName = (s) => encodeURIComponent(s).replace(/[()!'*]/g, (c) => "%" + c.charCodeAt(0).toString(16).toUpperCase());
  async function post(id, action, params) {
    const slash = id.indexOf("/");
    let url = "/" + id.slice(0, slash) + "/" + encName(id.slice(slash + 1)) + "/" + action;
    if (params) url += "?" + Object.entries(params).map(([k, v]) => k + "=" + encodeURIComponent(v)).join("&");
    const r = await fetch(url, { method: "POST", body: "" });
    if (!r.ok) throw new Error("HTTP " + r.status);
  }

  // ---------- translations ----------
  const STR = {
    pl: {
      "nav.overview": "Przegląd", "nav.cloud": "Chmura GbbOptimizer", "nav.inverter": "Falownik (RS485)",
      "nav.network": "Sieć", "nav.system": "System",
      "theme.auto": "Auto", "theme.light": "Jasny", "theme.dark": "Ciemny",
      "link.ok": "Połączono z dongle'em", "link.bad": "Brak połączenia z dongle'em",
      "pill.cloud.ok": "Chmura połączona", "pill.cloud.bad": "Brak chmury", "pill.cloud.off": "Chmura wyłączona", "pill.cloud.setup": "Do skonfigurowania",
      "ban.t": "Zmiany czekają na zastosowanie.", "ban.b": "Ustawienia chmury zaczną działać po ponownym uruchomieniu (ok. 15 s).",
      "ban.btn": "Zastosuj i uruchom ponownie",
      "lost.t": "Brak połączenia z dongle'em.", "lost.b": "Strona połączy się sama, gdy dongle będzie znowu dostępny.",
      "hero.ok": "Wszystko działa", "hero.okb": "Dongle jest połączony z GbbOptimizerem, który może teraz sterować falownikiem.",
      "hero.setup": "Dokończ konfigurację", "hero.setupb": "Wpisz dane instalacji z GbbOptimizera, a potem uruchom dongle ponownie.",
      "hero.setupbtn": "Przejdź do ustawień chmury",
      "hero.off": "Połączenie z chmurą jest wyłączone", "hero.offb": "GbbOptimizer nie steruje teraz falownikiem przez ten dongle.",
      "hero.bad": "Brak połączenia z GbbOptimizerem", "hero.badb": "Sprawdź Plant Id, token i serwer w ustawieniach chmury oraz czy dongle ma dostęp do internetu. Szczegóły są w logach.",
      "hero.logs": "Pokaż logi", "hero.restart": "Zmiany czekają na zastosowanie", "hero.restartb": "Uruchom dongle ponownie, żeby nowe ustawienia chmury zaczęły działać.",
      "hero.wait": "Łączenie…", "hero.waitb": "Pobieram stan urządzenia.",
      "st.conn": "Połączono", "st.disc": "Rozłączono", "st.off": "Wyłączono", "on": "włączone", "off": "wyłączone",
      "f.server": "Serwer", "f.tls": "Szyfrowanie", "f.params": "Parametry łącza", "f.req": "Obsłużone zapytania", "f.err": "Błędy Modbus",
      "f.ip": "Adres IP", "f.conn": "Połączenie", "f.version": "Wersja", "f.uptime": "Działa od", "f.update": "Aktualizacje",
      "t.device": "Urządzenie", "eth": "Ethernet (kabel)", "sig.good": "dobry sygnał", "sig.ok": "średni sygnał", "sig.weak": "słaby sygnał",
      "c.h": "Połączenie z GbbOptimizerem", "c.p": "Dane przepiszesz z GbbOptimizera. Po zapisaniu uruchom dongle ponownie.",
      "c.where": "Skąd wziąć te dane?", "c.s1": "W GbbOptimizerze otwórz Instalacje i przy swojej instalacji kliknij Popraw.",
      "c.s2": "Na górze zmień typ instalacji na GbbDongle.", "c.s3": "Przepisz stamtąd serwer MQTT, Plant Id i token do pól poniżej.",
      "c.en": "Połączenie z chmurą", "c.enh": "Wyłącz, jeśli chcesz na chwilę odciąć dongle od GbbOptimizera.",
      "c.plant": "Plant Id", "c.planth": "Identyfikator instalacji w GbbOptimizerze.",
      "c.token": "Token instalacji", "c.tokenh": "Traktuj jak hasło i nigdy go nie publikuj.",
      "c.tokenph": "Ukryty. Wpisz nowy, aby go zmienić.", "c.show": "Pokaż", "c.hide": "Ukryj",
      "c.server": "Serwer MQTT", "c.serverh": "Np. gbboptimizer7-mqtt.gbbsoft.pl", "c.adv": "Ustawienia zaawansowane",
      "c.port": "Port", "c.porth": "Domyślnie 8883.", "c.tls": "Szyfrowanie (TLS)", "c.tlsh": "Zostaw włączone.",
      "c.cn": "Nie sprawdzaj nazwy serwera w certyfikacie", "c.cnh": "Tylko do testów z własnym brokerem. Osłabia bezpieczeństwo.",
      "c.foot": "Zmiany w tej sekcji wymagają ponownego uruchomienia.",
      "c.fixed": "Poprawiłem adres serwera: zostawiłem samą nazwę hosta.",
      "e.h": "Komendy awaryjne", "e.p": "Gdy chmura przestanie odpowiadać, dongle sam wyśle do falownika ustawienia przygotowane wcześniej przez GbbOptimizer.",
      "e.stored": "Zapisane zestawy", "e.persist": "Pamiętaj po restarcie", "e.persisth": "Zestawy przetrwają wyłączenie zasilania (zapis do pamięci flash).",
      "i.h": "Łącze z falownikiem", "i.p": "Ustawienia portu RS485 muszą zgadzać się z ustawieniami Modbus w falowniku. Dla Deye zwykle jest to 9600 i brak parzystości.",
      "i.baud": "Prędkość", "i.par": "Parzystość", "i.live": "Te ustawienia działają od razu, bez restartu.",
      "i.stats": "Statystyki od uruchomienia",
      "opt.None": "Brak", "opt.Even": "Parzysta", "opt.Odd": "Nieparzysta",
      "n.wifi": "Wi-Fi", "n.wifip": "Sieć Wi-Fi ustawiasz przy instalacji. Jeśli dongle nie znajdzie znanej sieci, po chwili uruchomi punkt dostępowy „GbbDongle Setup”.",
      "n.eth": "Sieć przewodowa", "n.ethp": "Dongle łączy się kablem Ethernet i dostaje adres z routera (DHCP).",
      "n.sig": "Siła sygnału", "n.rssi": "Poziom sygnału",
      "s.fw": "Oprogramowanie", "s.updh": "Dongle sprawdza co 6 godzin i sam instaluje nowe wersje.",
      "s.latest": "Masz najnowszą wersję", "s.avail": "Dostępna wersja {v}", "s.installing": "Instalowanie…", "s.unknown": "Sprawdzanie…",
      "s.install": "Zainstaluj teraz",
      "s.pw": "Hasło do panelu", "s.pwp": "Login: `admin`. Po zmianie hasła przeglądarka poprosi o nowe.",
      "s.new": "Nowe hasło", "s.rep": "Powtórz hasło", "s.pwbtn": "Zmień hasło",
      "s.maint": "Konserwacja", "s.rb": "Uruchom ponownie", "s.rbh": "Zastosuje zapisane ustawienia chmury.",
      "s.file": "Wgraj firmware z pliku", "s.fileh": "Dla serwisu: plik .ota.bin z GitHuba. Zwykle nie jest potrzebne, aktualizacje instalują się same.",
      "s.filebtn": "Wybierz plik…", "s.uploading": "Wysyłanie… {p}%", "s.uploaded": "Wgrano. Dongle uruchamia się ponownie.",
      "s.fr": "Przywróć ustawienia fabryczne", "s.frh": "Kasuje ustawienia chmury, RS485 i hasło. Dongle trzeba będzie skonfigurować od nowa.",
      "s.frbtn": "Przywróć…", "fr.q": "Skasować wszystkie ustawienia?", "fr.yes": "Tak, skasuj", "no": "Anuluj",
      "s.other": "Inne", "s.help": "Pomoc", "s.helpb": "Instrukcja i najczęstsze pytania są na stronie projektu.",
      "s.classic": "Klasyczny panel ESPHome (wymaga internetu)",
      "b.save": "Zapisz", "b.cancel": "Cofnij", "b.press": "Uruchom", "b.close": "Zamknij",
      "dirty": "Masz niezapisane zmiany.", "saved": "Zapisano.", "savefail": "Nie udało się zapisać. Sprawdź połączenie i spróbuj ponownie.",
      "pw.empty": "Wpisz nowe hasło.", "pw.mis": "Hasła się różnią.", "pw.ok": "Hasło zmienione.",
      "port.bad": "Port musi być liczbą od 1 do 65535.",
      "log.t": "Logi urządzenia", "log.all": "Wszystko", "log.warn": "Ostrzeżenia i błędy", "log.pause": "Wstrzymaj", "log.resume": "Wznów",
      "log.clear": "Wyczyść", "log.copy": "Kopiuj", "log.copied": "Skopiowano", "log.hint": "Zgłaszając problem, dołącz logi z ostatnich minut.",
      "log.off": "Logi są wyłączone w tym firmware.",
      "rb.t": "Uruchamianie ponownie…", "rb.b": "Strona połączy się sama, gdy dongle będzie gotowy.",
      "rb.slow": "To trwa dłużej niż zwykle. Jeśli dongle zmienił adres IP, znajdziesz go w routerze.",
    },
    en: {
      "nav.overview": "Overview", "nav.cloud": "GbbOptimizer cloud", "nav.inverter": "Inverter (RS485)",
      "nav.network": "Network", "nav.system": "System",
      "theme.auto": "Auto", "theme.light": "Light", "theme.dark": "Dark",
      "link.ok": "Connected to the dongle", "link.bad": "No connection to the dongle",
      "pill.cloud.ok": "Cloud connected", "pill.cloud.bad": "Cloud offline", "pill.cloud.off": "Cloud off", "pill.cloud.setup": "Needs setup",
      "ban.t": "Changes are waiting to be applied.", "ban.b": "Cloud settings take effect after a restart (about 15 s).",
      "ban.btn": "Apply and restart",
      "lost.t": "No connection to the dongle.", "lost.b": "This page reconnects by itself when the dongle is reachable again.",
      "hero.ok": "Everything works", "hero.okb": "The dongle is connected to GbbOptimizer, which can now control the inverter.",
      "hero.setup": "Finish the setup", "hero.setupb": "Enter your installation details from GbbOptimizer, then restart the dongle.",
      "hero.setupbtn": "Go to cloud settings",
      "hero.off": "The cloud connection is off", "hero.offb": "GbbOptimizer is not controlling the inverter through this dongle.",
      "hero.bad": "No connection to GbbOptimizer", "hero.badb": "Check the Plant Id, token and server in the cloud settings, and whether the dongle has internet access. The logs have the details.",
      "hero.logs": "Show logs", "hero.restart": "Changes are waiting to be applied", "hero.restartb": "Restart the dongle so the new cloud settings take effect.",
      "hero.wait": "Connecting…", "hero.waitb": "Loading the device state.",
      "st.conn": "Connected", "st.disc": "Offline", "st.off": "Off", "on": "on", "off": "off",
      "f.server": "Server", "f.tls": "Encryption", "f.params": "Line settings", "f.req": "Requests handled", "f.err": "Modbus errors",
      "f.ip": "IP address", "f.conn": "Connection", "f.version": "Version", "f.uptime": "Up for", "f.update": "Updates",
      "t.device": "Device", "eth": "Ethernet (cable)", "sig.good": "good signal", "sig.ok": "fair signal", "sig.weak": "weak signal",
      "c.h": "Connection to GbbOptimizer", "c.p": "Copy these from GbbOptimizer. Restart the dongle after saving.",
      "c.where": "Where do I find these?", "c.s1": "In GbbOptimizer open Installations and click Edit next to yours.",
      "c.s2": "At the top, change the installation type to GbbDongle.", "c.s3": "Copy the MQTT server, Plant Id and token into the fields below.",
      "c.en": "Cloud connection", "c.enh": "Turn off to cut the dongle off from GbbOptimizer for a while.",
      "c.plant": "Plant Id", "c.planth": "Your installation's id in GbbOptimizer.",
      "c.token": "Plant token", "c.tokenh": "Treat it like a password and never share it.",
      "c.tokenph": "Hidden. Type a new one to change it.", "c.show": "Show", "c.hide": "Hide",
      "c.server": "MQTT server", "c.serverh": "e.g. gbboptimizer7-mqtt.gbbsoft.pl", "c.adv": "Advanced settings",
      "c.port": "Port", "c.porth": "Default 8883.", "c.tls": "Encryption (TLS)", "c.tlsh": "Leave on.",
      "c.cn": "Don't check the server name in the certificate", "c.cnh": "Only for testing with your own broker. Weakens security.",
      "c.foot": "Changes here need a restart.",
      "c.fixed": "I trimmed the server address down to the host name.",
      "e.h": "Emergency commands", "e.p": "If the cloud stops answering, the dongle sends the inverter the settings GbbOptimizer prepared in advance.",
      "e.stored": "Stored sets", "e.persist": "Keep after restart", "e.persisth": "Sets survive a power cut (written to flash).",
      "i.h": "Inverter link", "i.p": "The RS485 settings must match the inverter's Modbus settings. For Deye this is usually 9600 and no parity.",
      "i.baud": "Speed", "i.par": "Parity", "i.live": "These settings apply immediately, no restart needed.",
      "i.stats": "Statistics since start",
      "opt.None": "None", "opt.Even": "Even", "opt.Odd": "Odd",
      "n.wifi": "Wi-Fi", "n.wifip": "You choose the Wi-Fi network during setup. If the dongle can't find a known network, it opens the “GbbDongle Setup” access point after a minute.",
      "n.eth": "Wired network", "n.ethp": "The dongle is connected by Ethernet cable and gets its address from the router (DHCP).",
      "n.sig": "Signal strength", "n.rssi": "Signal level",
      "s.fw": "Firmware", "s.updh": "The dongle checks every 6 hours and installs new versions by itself.",
      "s.latest": "You have the latest version", "s.avail": "Version {v} is available", "s.installing": "Installing…", "s.unknown": "Checking…",
      "s.install": "Install now",
      "s.pw": "Panel password", "s.pwp": "Login: `admin`. After a change your browser asks for the new password.",
      "s.new": "New password", "s.rep": "Repeat password", "s.pwbtn": "Change password",
      "s.maint": "Maintenance", "s.rb": "Restart", "s.rbh": "Applies saved cloud settings.",
      "s.file": "Upload firmware from a file", "s.fileh": "For service: the .ota.bin file from GitHub. Usually not needed, updates install by themselves.",
      "s.filebtn": "Choose file…", "s.uploading": "Uploading… {p}%", "s.uploaded": "Uploaded. The dongle is restarting.",
      "s.fr": "Restore factory settings", "s.frh": "Erases the cloud, RS485 and password settings. The dongle will need to be set up again.",
      "s.frbtn": "Restore…", "fr.q": "Erase all settings?", "fr.yes": "Yes, erase", "no": "Cancel",
      "s.other": "Other", "s.help": "Help", "s.helpb": "The guide and the FAQ are on the project website.",
      "s.classic": "Classic ESPHome panel (needs internet)",
      "b.save": "Save", "b.cancel": "Undo", "b.press": "Run", "b.close": "Close",
      "dirty": "You have unsaved changes.", "saved": "Saved.", "savefail": "Saving failed. Check the connection and try again.",
      "pw.empty": "Enter a new password.", "pw.mis": "The passwords differ.", "pw.ok": "Password changed.",
      "port.bad": "The port must be a number from 1 to 65535.",
      "log.t": "Device logs", "log.all": "All", "log.warn": "Warnings and errors", "log.pause": "Pause", "log.resume": "Resume",
      "log.clear": "Clear", "log.copy": "Copy", "log.copied": "Copied", "log.hint": "When reporting a problem, include the logs from the last few minutes.",
      "log.off": "Logs are disabled in this firmware.",
      "rb.t": "Restarting…", "rb.b": "This page reconnects by itself when the dongle is ready.",
      "rb.slow": "This is taking longer than usual. If the dongle got a new IP address, your router lists it.",
    },
  };
  let lang = store.get("gbb-lang") || ((navigator.language || "pl").toLowerCase().startsWith("pl") ? "pl" : "en");
  const t = (k, vars) => {
    let s = STR[lang][k] ?? STR.pl[k] ?? k;
    if (vars) for (const [a, b] of Object.entries(vars)) s = s.replace("{" + a + "}", b);
    return s;
  };
  // Every translated node registers here so a language switch can redraw it.
  const i18n = [];
  // `backticks` in a string mark literal values (logins, commands) shown as code.
  const setText = (el, s) => {
    if (s.includes("`")) el.replaceChildren(...s.split("`").map((part, i) => (i % 2 ? h("code", { text: part }) : part)));
    else el.textContent = s;
  };
  const tx = (el, key, vars) => { el._tk = key; el._tv = vars; setText(el, t(key, vars)); if (!el._reg) { el._reg = true; i18n.push(el); } return el; };
  const T = (tag, key, attrs) => tx(h(tag, attrs), key);

  // ---------- entity state ----------
  const ID = {
    cloudConnected: "binary_sensor/Cloud Connected", restartRequired: "binary_sensor/Restart Required",
    apply: "button/Apply Settings (Restart)", uptime: "text_sensor/Uptime",
    cloudEnabled: "switch/Cloud Connection", plantId: "text/Plant Id", token: "text/Plant Token",
    host: "text/MQTT Server", port: "number/MQTT Port", tls: "switch/TLS", skipCn: "switch/TLS Skip CN Check",
    persist: "switch/Persist Emergency Commands", sets: "sensor/Emergency Sets Stored",
    baud: "select/RS485 Baud Rate", parity: "select/RS485 Parity",
    req: "sensor/Requests Handled", err: "sensor/Modbus Errors",
    ip: "text_sensor/IP Address", wifiPct: "sensor/WiFi Signal Percent", wifiDb: "sensor/WiFi Signal dB",
    fw: "text_sensor/Firmware Version", update: "update/Firmware", adminPw: "text/Admin Password",
    factory: "button/Factory Reset",
  };
  const KNOWN = new Set(Object.values(ID));
  const ents = new Map();           // id -> merged JSON from the event stream
  const watchers = new Map();       // id -> [fn(entity)]
  const anyWatchers = [];           // fn() after every state event
  const watch = (id, fn) => { if (!watchers.has(id)) watchers.set(id, []); watchers.get(id).push(fn); };
  const ent = (id) => ents.get(id);
  const on = (id) => ent(id)?.value === true;
  const val = (id) => ent(id)?.value;
  const str = (id) => { const e = ent(id); return e && e.state !== undefined ? String(e.state) : ""; };
  let info = { title: "GbbDongle", ota: true, log: true };

  // ---------- shell ----------
  const PAGES = [
    ["overview", "nav.overview", "grid"], ["cloud", "nav.cloud", "cloud"], ["inverter", "nav.inverter", "inverter"],
    ["network", "nav.network", "wifi"], ["system", "nav.system", "gear"],
  ];
  document.documentElement.classList.add("gbb");
  if (!$('meta[name="viewport"]')) document.head.append(h("meta", { name: "viewport", content: "width=device-width, initial-scale=1, viewport-fit=cover" }));
  $("esp-app")?.remove();

  const navDots = {};
  const nav = h("nav", { "aria-label": "Menu" }, PAGES.map(([id, key, ic]) => {
    const dot = id === "cloud" ? (navDots.cloud = h("i", { class: "dot" })) : null;
    return h("a", { href: "#" + id, "data-page": id }, icon(ic), T("span", key), dot);
  }));
  const brandName = h("b", { text: "GbbDongle" });
  const brandSub = h("small", { text: location.host });
  const footVer = h("span");
  const footUp = h("span");
  const side = h("aside", { class: "side" },
    h("div", { class: "brand" }, h("div", { class: "brand-mark" }, icon("bolt")), h("div", null, brandName, brandSub)),
    nav, h("div", { class: "side-foot" }, footVer, footUp));

  const pageTitle = h("h1");
  const linkPill = h("span", { class: "pill", role: "status" });
  const cloudPill = h("span", { class: "pill" });
  const langBtn = h("button", { class: "icon-btn", type: "button", title: "Język / Language" });
  const themeLbl = h("span", { class: "theme-lbl" });
  const themeBtn = h("button", { class: "icon-btn", type: "button" });
  const topbar = h("header", { class: "topbar" }, pageTitle, cloudPill, linkPill, langBtn, themeBtn);

  const restartBanner = h("div", { class: "banner", hidden: true },
    h("p", null, T("b", "ban.t"), " ", T("span", "ban.b")),
    h("button", { class: "btn primary", type: "button", onclick: () => restart() }, T("span", "ban.btn")));
  const lostBanner = h("div", { class: "banner bad", hidden: true }, h("p", null, T("b", "lost.t"), " ", T("span", "lost.b")));

  const sections = {};
  for (const [id] of PAGES) sections[id] = h("section", { class: "sec", "data-section": id, hidden: true });
  const content = h("div", { class: "content" }, lostBanner, restartBanner, Object.values(sections));
  const main = h("div", { class: "main" }, topbar, content);

  // ---------- building blocks ----------
  function card(titleKey, descKey, ...body) {
    const c = h("div", { class: "card" });
    if (titleKey) c.append(h("div", { class: "card-head" }, T("h2", titleKey), descKey ? T("p", descKey) : null));
    const b = h("div", { class: "card-body" }, body);
    c.append(b);
    c._body = b;
    return c;
  }
  // A labelled row bound to one entity; hidden until the device reports it.
  function row(id, labelKey, helpKey, control) {
    const lbl = h("div", { class: "lbl" }, labelKey ? T("span", labelKey) : h("span", { text: id.split("/")[1] }),
      helpKey ? T("small", helpKey) : null);
    lbl.title = "ESPHome: " + id.slice(id.indexOf("/") + 1);
    const r = h("div", { class: "row", hidden: true }, lbl, h("div", { class: "val" }, control));
    watch(id, () => { r.hidden = false; });
    return r;
  }
  const readout = (id, fmt) => { const s = h("span"); watch(id, (e) => { s.textContent = fmt ? fmt(e) : String(e.state ?? ""); }); return s; };
  const fmtNum = (e) => (typeof e.value === "number" && isFinite(e.value) ? Math.round(e.value).toLocaleString(lang === "pl" ? "pl-PL" : "en-GB") : "—");
  function switchEl(attrs) {
    const input = h("input", Object.assign({ type: "checkbox", role: "switch" }, attrs));
    const lab = h("label", { class: "switch" }, input, h("span"));
    lab._input = input;
    return lab;
  }

  // A card whose controls are staged and written together by "Save".
  function formCard(titleKey, descKey, footKey) {
    const c = card(titleKey, descKey);
    const fields = [];
    const hint = T("span", footKey, { class: "hint" });
    const save = h("button", { class: "btn primary", type: "button", disabled: true }, T("span", "b.save"));
    const cancel = h("button", { class: "btn", type: "button", hidden: true }, T("span", "b.cancel"));
    c.append(h("div", { class: "card-foot" }, hint, cancel, save));
    const refresh = () => {
      const dirty = fields.some((f) => f.dirty());
      save.disabled = !dirty;
      cancel.hidden = !dirty;
      hint.className = "hint" + (dirty ? " dirty" : "");
      tx(hint, dirty ? "dirty" : footKey);
    };
    cancel.addEventListener("click", () => { fields.forEach((f) => f.reset()); refresh(); });
    save.addEventListener("click", async () => {
      const dirty = fields.filter((f) => f.dirty());
      for (const f of dirty) {
        const err = f.validate?.();
        if (err) { hint.className = "hint bad"; tx(hint, err); return; }
      }
      save.disabled = true;
      try {
        for (const f of dirty) { await f.write(); f.commit(); }
        hint.className = "hint"; tx(hint, "saved");
        cancel.hidden = true;
        c._afterSave?.(dirty);
        setTimeout(refresh, 2500);
      } catch (e) {
        hint.className = "hint bad"; tx(hint, "savefail");
        save.disabled = false;
      }
    });
    c._field = (f) => { fields.push(f); f.changed = refresh; return f; };
    return c;
  }
  // Field helpers for formCard. Each tracks the device value and the user's edit.
  function textField(fc, id, opts = {}) {
    const input = h("input", { type: opts.password ? "password" : "text", autocomplete: "off", spellcheck: "false", maxlength: "128", id: "f-" + id.replace(/\W/g, "") });
    let edited = false;
    const f = fc._field({
      el: input,
      dirty: () => edited && (opts.password ? input.value !== "" : input.value !== String(val(id) ?? "")),
      reset: () => { edited = false; input.classList.remove("dirty", "invalid"); input.value = opts.password ? "" : String(val(id) ?? ""); },
      write: () => post(id, "set", { value: input.value.trim() }),
      commit: () => {
        edited = false; input.classList.remove("dirty");
        const e = ent(id);
        if (opts.password) input.value = "";
        else if (e) { e.value = e.state = input.value.trim(); input.value = e.value; }
      },
    });
    input.addEventListener("input", () => { edited = true; input.classList.toggle("dirty", f.dirty()); f.changed(); });
    watch(id, (e) => {
      if (e.max_length) input.maxLength = e.max_length;
      if (opts.password) input.placeholder = t("c.tokenph");
      if (!edited) input.value = opts.password ? "" : String(e.value ?? "");
    });
    return f;
  }
  function numberField(fc, id) {
    const input = h("input", { type: "number", inputmode: "numeric", id: "f-" + id.replace(/\W/g, "") });
    let edited = false;
    const f = fc._field({
      el: input,
      dirty: () => edited && input.value !== String(val(id) ?? ""),
      reset: () => { edited = false; input.classList.remove("dirty", "invalid"); input.value = String(val(id) ?? ""); },
      validate: () => {
        const e = ent(id) || {};
        const n = Number(input.value);
        const ok = input.value !== "" && Number.isInteger(n) && n >= Number(e.min_value ?? -Infinity) && n <= Number(e.max_value ?? Infinity);
        input.classList.toggle("invalid", !ok);
        return ok ? null : "port.bad";
      },
      write: () => post(id, "set", { value: input.value }),
      commit: () => { edited = false; input.classList.remove("dirty", "invalid"); const e = ent(id); if (e) e.value = e.state = input.value; },
    });
    input.addEventListener("input", () => { edited = true; input.classList.toggle("dirty", f.dirty()); f.changed(); });
    watch(id, (e) => {
      if (e.min_value !== undefined) input.min = e.min_value;
      if (e.max_value !== undefined) input.max = e.max_value;
      if (!edited) input.value = String(e.value ?? "");
    });
    return f;
  }
  function switchField(fc, id) {
    const sw = switchEl({ id: "f-" + id.replace(/\W/g, "") });
    const input = sw._input;
    const f = fc._field({
      el: sw,
      dirty: () => input.checked !== on(id),
      reset: () => { input.checked = on(id); sw.classList.remove("dirty"); },
      write: () => post(id, input.checked ? "turn_on" : "turn_off"),
      commit: () => { sw.classList.remove("dirty"); const e = ent(id); if (e) { e.value = input.checked; e.state = input.checked ? "ON" : "OFF"; } },
    });
    input.addEventListener("change", () => { sw.classList.toggle("dirty", f.dirty()); f.changed(); });
    watch(id, () => { if (!sw.classList.contains("dirty")) input.checked = on(id); });
    return f;
  }
  function selectField(fc, id, labelPrefix) {
    const sel = h("select", { id: "f-" + id.replace(/\W/g, "") });
    let edited = false;
    const f = fc._field({
      el: sel,
      dirty: () => edited && sel.value !== String(val(id) ?? ""),
      reset: () => { edited = false; sel.classList.remove("dirty"); sel.value = String(val(id) ?? ""); },
      write: () => post(id, "set", { option: sel.value }),
      commit: () => { edited = false; sel.classList.remove("dirty"); const e = ent(id); if (e) e.value = e.state = sel.value; },
    });
    sel.addEventListener("change", () => { edited = true; sel.classList.toggle("dirty", f.dirty()); f.changed(); });
    watch(id, (e) => {
      if (Array.isArray(e.option) && sel.options.length !== e.option.length) {
        sel.replaceChildren(...e.option.map((o) => {
          const opt = h("option", { value: o });
          if (labelPrefix && STR.pl[labelPrefix + o]) tx(opt, labelPrefix + o); else opt.textContent = o;
          return opt;
        }));
      }
      if (!edited) sel.value = String(e.value ?? "");
    });
    return f;
  }
  // Immediate controls, used outside form cards and for unknown entities.
  function liveSwitch(id) {
    const sw = switchEl({});
    sw._input.addEventListener("change", () => {
      post(id, sw._input.checked ? "turn_on" : "turn_off").catch(() => { sw._input.checked = on(id); });
    });
    watch(id, () => { sw._input.checked = on(id); });
    return sw;
  }
  function genericControl(e) {
    const id = e.id;
    switch (e.domain) {
      case "switch": return liveSwitch(id);
      case "button": return h("button", { class: "btn", type: "button", onclick: () => post(id, "press") }, T("span", "b.press"));
      default: return readout(id);
    }
  }

  // ---------- Overview ----------
  const heroIco = h("div", { class: "hero-ico" });
  const heroT = h("h2"), heroB = h("p");
  const heroBtn = h("button", { class: "btn", type: "button", hidden: true });
  const hero = h("div", { class: "card hero" }, heroIco, h("div", null, heroT, heroB), heroBtn);

  const tile = (href, ic, titleKey, pill, rows) => h("a", { class: "card tile", href },
    h("div", { class: "tile-head" }, icon(ic), T("h3", titleKey), pill), h("dl", { class: "kv" }, rows));
  const kv = (key, valueEl, id) => {
    const dt = T("dt", key), dd = h("dd", null, valueEl);
    if (id) { dt.hidden = dd.hidden = true; watch(id, () => { dt.hidden = dd.hidden = false; }); }
    return [dt, dd];
  };
  const tileCloudPill = h("span", { class: "pill" });
  const lineParams = h("span");
  const bars = (pct) => h("span", { class: "bars" + (pct < 40 ? " weak" : ""), "aria-hidden": "true" },
    [1, 2, 3, 4].map((i) => h("i", { class: pct >= i * 25 - 12 ? "on" : "" })));
  const sigText = (pct) => t(pct >= 60 ? "sig.good" : pct >= 35 ? "sig.ok" : "sig.weak");
  const wifiSummary = h("span");
  const updateText = h("span");
  sections.overview.append(hero, h("div", { class: "tiles" },
    tile("#cloud", "cloud", "nav.cloud", tileCloudPill, [
      kv("c.plant", readout(ID.plantId), ID.plantId),
      kv("f.server", readout(ID.host), ID.host),
      kv("f.tls", readout(ID.tls, (e) => t(e.value ? "on" : "off")), ID.tls),
    ]),
    tile("#inverter", "inverter", "nav.inverter", null, [
      kv("f.params", lineParams, ID.baud),
      kv("f.req", readout(ID.req, fmtNum), ID.req),
      kv("f.err", readout(ID.err, fmtNum), ID.err),
    ]),
    tile("#network", "wifi", "nav.network", null, [
      kv("f.conn", wifiSummary),
      kv("f.ip", readout(ID.ip), ID.ip),
    ]),
    tile("#system", "gear", "t.device", null, [
      kv("f.version", readout(ID.fw), ID.fw),
      kv("f.uptime", readout(ID.uptime), ID.uptime),
      kv("f.update", updateText, ID.update),
    ]),
  ));

  // ---------- Cloud ----------
  {
    const fc = formCard("c.h", "c.p", "c.foot");
    const en = switchField(fc, ID.cloudEnabled);
    const plant = textField(fc, ID.plantId);
    const token = textField(fc, ID.token, { password: true });
    const host = textField(fc, ID.host);
    const port = numberField(fc, ID.port);
    const tls = switchField(fc, ID.tls);
    const cn = switchField(fc, ID.skipCn);
    const fixedMsg = T("small", "c.fixed", { class: "msg ok", hidden: true });
    // People paste "mqtts://host:8883/" from elsewhere: keep the host only
    // and move a port into the port field, both still to be saved.
    host.el.addEventListener("change", () => {
      const v = host.el.value.trim();
      let s = v.replace(/^[a-z]+:\/\//i, "").replace(/\/.*$/, "");
      const m = s.match(/^([^:]+):(\d{1,5})$/);
      if (m) {
        s = m[1];
        if (port.el.value !== m[2]) { port.el.value = m[2]; port.el.dispatchEvent(new Event("input")); }
      }
      fixedMsg.hidden = s === v;
      if (s !== host.el.value) { host.el.value = s; host.el.dispatchEvent(new Event("input")); }
    });
    const eye = h("button", { class: "btn", type: "button" });
    tx(eye, "c.show");
    eye.addEventListener("click", () => {
      const show = token.el.type === "password";
      token.el.type = show ? "text" : "password";
      tx(eye, show ? "c.hide" : "c.show");
    });
    fc._body.append(
      h("div", { class: "callout" }, T("b", "c.where"), h("ol", null, T("li", "c.s1"), T("li", "c.s2"), T("li", "c.s3"))),
      row(ID.cloudEnabled, "c.en", "c.enh", en.el),
      row(ID.plantId, "c.plant", "c.planth", plant.el),
      row(ID.token, "c.token", "c.tokenh", h("div", { class: "pw" }, token.el, eye)),
      row(ID.host, "c.server", "c.serverh", h("div", { style: "display:grid;gap:4px;width:100%" }, host.el, fixedMsg)),
      h("details", { class: "adv" }, T("summary", "c.adv"),
        row(ID.port, "c.port", "c.porth", port.el),
        row(ID.tls, "c.tls", "c.tlsh", tls.el),
        row(ID.skipCn, "c.cn", "c.cnh", cn.el)),
    );
    // The firmware raises Restart Required itself; show the banner at once
    // instead of waiting for that state event.
    fc._afterSave = () => { pendingRestart = true; fixedMsg.hidden = true; renderDerived(); };
    sections.cloud.append(fc,
      card("e.h", "e.p",
        row(ID.sets, "e.stored", null, readout(ID.sets, fmtNum)),
        row(ID.persist, "e.persist", "e.persisth", liveSwitch(ID.persist))));
  }

  // ---------- Inverter ----------
  {
    const fc = formCard("i.h", "i.p", "i.live");
    const baud = selectField(fc, ID.baud);
    const par = selectField(fc, ID.parity, "opt.");
    fc._body.append(
      row(ID.baud, "i.baud", null, [baud.el, h("span", { class: "unit", text: "bit/s" })]),
      row(ID.parity, "i.par", null, par.el));
    const stat = (id, key) => {
      const s = h("div", { class: "stat", hidden: true }, T("small", key), h("span", { class: "big" }, readout(id, fmtNum)));
      watch(id, () => { s.hidden = false; });
      return s;
    };
    const stats = card("i.stats", null);
    stats._body.replaceWith(h("div", { class: "stats" }, stat(ID.req, "f.req"), stat(ID.err, "f.err")));
    sections.inverter.append(fc, stats);
  }

  // ---------- Network ----------
  const wifiCard = card("n.wifi", "n.wifip",
    row(ID.wifiPct, "n.sig", null, readout(ID.wifiPct, (e) => (typeof e.value === "number" ? Math.round(e.value) + "% · " + sigText(e.value) : "—"))),
    row(ID.wifiDb, "n.rssi", null, readout(ID.wifiDb, (e) => (typeof e.value === "number" ? Math.round(e.value) + " dBm" : "—"))),
    row(ID.ip, "f.ip", null, readout(ID.ip)));
  const ethCard = card("n.eth", "n.ethp", row(ID.ip, "f.ip", null, readout(ID.ip)));
  wifiCard.hidden = true;
  sections.network.append(wifiCard, ethCard);

  // ---------- System ----------
  const otherCard = card("s.other", null);
  otherCard.hidden = true;
  {
    const updPill = h("span", { class: "pill" });
    const updBtn = h("button", { class: "btn primary", type: "button", hidden: true, onclick: () => post(ID.update, "install").catch(() => {}) }, T("span", "s.install"));
    sections.system.append(card("s.fw", null,
      row(ID.fw, "f.version", null, readout(ID.fw)),
      row(ID.update, "f.update", "s.updh", [updPill, updBtn]),
      row(ID.uptime, "f.uptime", null, readout(ID.uptime))));
    watch(ID.update, (e) => {
      const s = String(e.state || "").toUpperCase();
      const avail = s.includes("AVAILABLE");
      updPill.className = "pill " + (avail ? "warn" : s.includes("INSTALL") ? "warn" : s.includes("NO UPDATE") ? "ok" : "");
      tx(updPill, avail ? "s.avail" : s.includes("INSTALL") ? "s.installing" : s.includes("NO UPDATE") ? "s.latest" : "s.unknown", { v: e.value || "" });
      tx(updateText, updPill._tk, { v: e.value || "" });
      updBtn.hidden = !avail;
    });

    // Admin password (factory line only).
    const pw1 = h("input", { type: "password", autocomplete: "new-password", id: "pw1", maxlength: "64" });
    const pw2 = h("input", { type: "password", autocomplete: "new-password", id: "pw2", maxlength: "64" });
    const pwMsg = h("span", { class: "msg", role: "status" });
    const pwBtn = h("button", { class: "btn primary", type: "button" }, T("span", "s.pwbtn"));
    pwBtn.addEventListener("click", async () => {
      const bad = !pw1.value ? "pw.empty" : pw1.value !== pw2.value ? "pw.mis" : null;
      if (bad) { pwMsg.className = "msg bad"; tx(pwMsg, bad); return; }
      try {
        await post(ID.adminPw, "set", { value: pw1.value });
        pw1.value = pw2.value = "";
        pwMsg.className = "msg ok"; tx(pwMsg, "pw.ok");
      } catch (e) { pwMsg.className = "msg bad"; tx(pwMsg, "savefail"); }
    });
    const pwCard = card("s.pw", "s.pwp",
      row(ID.adminPw, "s.new", null, pw1),
      row(ID.adminPw, "s.rep", null, [pw2, pwMsg]));
    pwCard.append(h("div", { class: "card-foot" }, pwBtn));
    pwCard.hidden = true;
    watch(ID.adminPw, () => { pwCard.hidden = false; });

    // Firmware upload through the web_server OTA endpoint.
    const file = h("input", { type: "file", accept: ".bin", hidden: true });
    const upMsg = h("span", { class: "msg", role: "status" });
    const upBar = h("div", { class: "progress", hidden: true }, h("i"));
    const upBtn = h("button", { class: "btn", type: "button", onclick: () => file.click() }, T("span", "s.filebtn"));
    file.addEventListener("change", () => {
      if (!file.files.length) return;
      const fd = new FormData();
      fd.append("update", file.files[0]);
      const xhr = new XMLHttpRequest();
      upBar.hidden = false; upBtn.disabled = true;
      xhr.upload.onprogress = (ev) => {
        if (!ev.lengthComputable) return;
        const p = Math.round((ev.loaded / ev.total) * 100);
        upBar.firstChild.style.width = p + "%";
        upMsg.className = "msg"; tx(upMsg, "s.uploading", { p });
      };
      xhr.onload = () => {
        upBtn.disabled = false; file.value = "";
        if (xhr.status === 200) { upMsg.className = "msg ok"; tx(upMsg, "s.uploaded"); waitForReboot(); }
        else { upMsg.className = "msg bad"; upMsg.textContent = xhr.responseText || "HTTP " + xhr.status; }
      };
      xhr.onerror = () => { upBtn.disabled = false; upMsg.className = "msg bad"; tx(upMsg, "savefail"); };
      xhr.open("POST", "/update");
      xhr.send(fd);
    });
    const otaRow = h("div", { class: "row" },
      h("div", { class: "lbl" }, T("span", "s.file"), T("small", "s.fileh")),
      h("div", { class: "val" }, upBtn, file, upBar, upMsg));

    // Factory reset with an in-page confirmation step.
    const frBox = h("div", { class: "val" });
    const frIdle = () => {
      const b = h("button", { class: "btn danger", type: "button" }, T("span", "s.frbtn"));
      b.addEventListener("click", () => {
        const no = h("button", { class: "btn", type: "button", onclick: frIdle }, T("span", "no"));
        const yes = h("button", { class: "btn danger solid", type: "button" }, T("span", "fr.yes"));
        yes.addEventListener("click", () => { post(ID.factory, "press").then(waitForReboot).catch(frIdle); });
        frBox.replaceChildren(T("b", "fr.q"), yes, no);
        no.focus();
      });
      frBox.replaceChildren(b);
    };
    frIdle();
    const frRow = h("div", { class: "row", hidden: true },
      h("div", { class: "lbl", title: "ESPHome: Factory Reset" }, T("span", "s.fr"), T("small", "s.frh")), frBox);
    watch(ID.factory, () => { frRow.hidden = false; });

    const maint = card("s.maint", null,
      row(ID.apply, "s.rb", "s.rbh", h("button", { class: "btn", type: "button", onclick: () => restart() }, T("span", "s.rb"))),
      otaRow, frRow);
    watch("__info", () => { otaRow.hidden = !info.ota; });

    const help = card("s.help", "s.helpb", h("p", { style: "display:flex;gap:12px;flex-wrap:wrap;margin:8px 0 0" },
      h("a", { href: "https://gbbdongle.hajdamowicz.info/", target: "_blank", rel: "noopener" }, "gbbdongle.hajdamowicz.info"),
      tx(h("a", { href: "?classic" }), "s.classic")));
    sections.system.append(pwCard, maint, otherCard, help);
  }

  // ---------- Log drawer ----------
  const logEl = h("div", { class: "log", role: "log", "aria-live": "off", tabindex: "0" });
  const lastLine = h("span", { class: "last" });
  const badge = h("span", { class: "badge", hidden: true });
  const drawerBar = h("button", { class: "drawer-bar", type: "button", "aria-expanded": "false", "aria-controls": "gbb-log" },
    icon("lines"), T("span", "log.t"), badge, lastLine, icon("chev", "chev"));
  const pauseBtn = h("button", { class: "btn", type: "button", "aria-pressed": "false" });
  tx(pauseBtn, "log.pause");
  const fAll = h("button", { class: "btn", type: "button", "aria-pressed": "true" }, T("span", "log.all"));
  const fWarn = h("button", { class: "btn", type: "button", "aria-pressed": "false" }, T("span", "log.warn"));
  const copyBtn = h("button", { class: "btn", type: "button" });
  tx(copyBtn, "log.copy");
  const clearBtn = h("button", { class: "btn", type: "button" }, T("span", "log.clear"));
  const drawerBody = h("div", { class: "drawer-body", id: "gbb-log", hidden: true },
    h("div", { class: "log-tools" }, fAll, fWarn, pauseBtn, clearBtn, copyBtn, T("span", "log.hint", { class: "sp" })), logEl);
  const drawer = h("div", { class: "drawer" }, drawerBar, drawerBody);

  const lines = [];
  const MAX_LINES = 1000;
  let logOpen = false, paused = false, warnOnly = false, unseenW = 0, unseenE = 0;
  function setLogOpen(v) {
    logOpen = v;
    drawer.classList.toggle("open", v);
    drawerBody.hidden = !v;
    drawerBar.setAttribute("aria-expanded", String(v));
    store.set("gbb-log-open", v ? "1" : "0");
    if (v) { unseenW = unseenE = 0; renderBadge(); redraw(); }
  }
  function renderBadge() {
    const n = unseenW + unseenE;
    badge.hidden = n === 0;
    badge.textContent = n > 99 ? "99+" : String(n);
    badge.className = "badge" + (unseenE ? " err" : "");
  }
  const lineEl = (l) => {
    const d = h("div", { class: l.lvl }, h("span", { class: "t", text: l.time + " " }), l.text);
    d.hidden = warnOnly && !(l.lvl === "W" || l.lvl === "E");
    return d;
  };
  function redraw() {
    if (!logOpen) return;
    logEl.replaceChildren(...lines.map(lineEl));
    logEl.scrollTop = logEl.scrollHeight;
  }
  function onLog(raw) {
    // ESPHome sends one line per event with ANSI colour codes: "\x1b[0;33m[W][tag:12]: text\x1b[0m".
    const text = String(raw).replace(/\x1b\[[0-9;]*m/g, "");
    const m = text.match(/^\[(VV|[EWIDVC])\]/);
    const lvl = m ? m[1] : "D";
    const l = { lvl, text, time: new Date().toTimeString().slice(0, 8) };
    lines.push(l);
    if (lines.length > MAX_LINES) { lines.shift(); if (logOpen && !paused) logEl.firstChild?.remove(); }
    lastLine.textContent = text;
    if (!logOpen && (lvl === "W" || lvl === "E")) { if (lvl === "E") unseenE++; else unseenW++; renderBadge(); }
    if (!logOpen || paused) return;
    const stick = logEl.scrollHeight - logEl.scrollTop - logEl.clientHeight < 40;
    logEl.append(lineEl(l));
    if (stick) logEl.scrollTop = logEl.scrollHeight;
  }
  drawerBar.addEventListener("click", () => setLogOpen(!logOpen));
  const setFilter = (w) => { warnOnly = w; fAll.setAttribute("aria-pressed", String(!w)); fWarn.setAttribute("aria-pressed", String(w)); redraw(); };
  fAll.addEventListener("click", () => setFilter(false));
  fWarn.addEventListener("click", () => setFilter(true));
  pauseBtn.addEventListener("click", () => {
    paused = !paused;
    pauseBtn.setAttribute("aria-pressed", String(paused));
    tx(pauseBtn, paused ? "log.resume" : "log.pause");
    if (!paused) redraw();
  });
  clearBtn.addEventListener("click", () => { lines.length = 0; logEl.replaceChildren(); lastLine.textContent = ""; });
  copyBtn.addEventListener("click", () => {
    const text = lines.map((l) => l.time + " " + l.text).join("\n");
    const done = () => { tx(copyBtn, "log.copied"); setTimeout(() => tx(copyBtn, "log.copy"), 1500); };
    const fallback = () => { const r = document.createRange(); r.selectNodeContents(logEl); const s = getSelection(); s.removeAllRanges(); s.addRange(r); };
    if (navigator.clipboard?.writeText) navigator.clipboard.writeText(text).then(done, fallback); else fallback();
  });

  // ---------- restart overlay ----------
  const ovT = T("b", "rb.t");
  const ovB = T("span", "rb.b", { style: "color:var(--muted)" });
  const ovClose = h("button", { class: "btn", type: "button", hidden: true, onclick: () => endReboot() }, T("span", "b.close"));
  const overlay = h("div", { class: "overlay", hidden: true, role: "alertdialog", "aria-live": "assertive" },
    h("div", { class: "card" }, h("div", { class: "spinner" }), ovT, ovB, ovClose));
  // A reboot is over when the device reports a later boot time than before
  // it (boot time = arrival time minus the uptime in a ping); sawDrop is the
  // fallback for pings without an uptime.
  let rebooting = false, sawDrop = false, slowTimer = 0, probeTimer = 0, pendingRestart = false;
  let bootAt = 0, bootBefore = 0;
  function waitForReboot() {
    rebooting = true; sawDrop = false; bootBefore = bootAt;
    overlay.hidden = false; ovClose.hidden = true;
    tx(ovB, "rb.b");
    clearTimeout(slowTimer);
    slowTimer = setTimeout(() => { tx(ovB, "rb.slow"); ovClose.hidden = false; }, 60000);
    clearTimeout(probeTimer);
    probeTimer = setTimeout(probe, 1000);
  }
  function endReboot() {
    rebooting = false; overlay.hidden = true; clearTimeout(slowTimer); clearTimeout(probeTimer);
    renderDerived();
  }
  // The ESP reboots without closing the /events connection, so the browser
  // notices the dead stream only when TCP gives up, about 45 s later. While
  // a reboot is expected, short requests check whether the device still
  // answers; the first one that fails reopens the stream.
  function probe() {
    if (!rebooting) return;
    const ctl = new AbortController();
    const timer = setTimeout(() => ctl.abort(), 2000);
    fetch("/text_sensor/" + encName("Uptime"), { cache: "no-store", signal: ctl.signal })
      .then((r) => r.arrayBuffer())
      .then(() => true, () => false)
      .then((up) => {
        clearTimeout(timer);
        if (!rebooting) return;
        if (!up && streamOpen) dropStream();
        probeTimer = setTimeout(probe, 1000);
      });
  }
  async function restart() {
    try {
      await post(ID.apply, "press");
    } catch (e) {
      // An HTTP error means the device answered and refused: no reboot is
      // coming. A network error is the device dropping the request as it
      // reboots.
      if (String(e.message).startsWith("HTTP")) { restartBanner.hidden = false; tx($("b", restartBanner), "savefail"); return; }
    }
    waitForReboot();
    renderDerived();
  }

  // ---------- derived state ----------
  function cloudState() {
    if (!ents.size) return "wait";
    if (!str(ID.plantId) || !str(ID.host)) return "setup";
    if (ent(ID.cloudEnabled) && !on(ID.cloudEnabled)) return "off";
    if (on(ID.cloudConnected)) return "ok";
    if (pendingRestart || on(ID.restartRequired)) return "restart";
    return "bad";
  }
  function renderDerived() {
    const cs = cloudState();
    const tone = { ok: "ok", bad: "bad", off: "", setup: "warn", restart: "warn", wait: "" }[cs];
    const pillKey = { ok: "pill.cloud.ok", bad: "pill.cloud.bad", off: "pill.cloud.off", setup: "pill.cloud.setup", restart: "pill.cloud.bad", wait: "hero.wait" }[cs];
    cloudPill.className = "pill " + tone; tx(cloudPill, pillKey);
    tileCloudPill.className = "pill " + tone;
    tx(tileCloudPill, { ok: "st.conn", off: "st.off", setup: "pill.cloud.setup" }[cs] || "st.disc");
    if (navDots.cloud) navDots.cloud.className = "dot " + tone;

    heroIco.className = "hero-ico " + tone;
    heroIco.replaceChildren(icon(cs === "ok" ? "check" : cs === "wait" || cs === "off" ? "dots" : "alert"));
    tx(heroT, "hero." + cs);
    tx(heroB, "hero." + cs + "b");
    heroBtn.hidden = !(cs === "setup" || cs === "bad" || cs === "restart");
    if (cs === "setup") { heroBtn.onclick = () => { location.hash = "cloud"; }; tx(heroBtn, "hero.setupbtn"); }
    else if (cs === "restart") { heroBtn.onclick = () => restart(); tx(heroBtn, "ban.btn"); }
    else if (cs === "bad") { heroBtn.onclick = () => setLogOpen(true); tx(heroBtn, "hero.logs"); }

    restartBanner.hidden = rebooting || !(pendingRestart || on(ID.restartRequired));

    const baud = str(ID.baud), par = str(ID.parity);
    lineParams.textContent = baud ? baud + " 8" + (par ? par[0].toUpperCase() : "N") + "1" : "—";

    const pct = val(ID.wifiPct);
    const hasWifi = ents.has(ID.wifiPct) || ents.has(ID.wifiDb);
    wifiCard.hidden = !hasWifi;
    ethCard.hidden = hasWifi || !ents.size;
    if (hasWifi && typeof pct === "number") wifiSummary.replaceChildren(bars(pct), " Wi-Fi · " + Math.round(pct) + "%");
    else if (hasWifi) wifiSummary.textContent = "Wi-Fi";
    else wifiSummary.textContent = ents.size ? t("eth") : "—";

    footVer.textContent = str(ID.fw) ? t("f.version") + " " + str(ID.fw) : "";
    footUp.textContent = str(ID.uptime) ? t("f.uptime") + " " + str(ID.uptime) : "";
  }
  anyWatchers.push(renderDerived);

  function addOther(e) {
    if (KNOWN.has(e.id) || e._other) return;
    e._other = true;
    otherCard.hidden = false;
    otherCard._body.append(row(e.id, null, null, genericControl(e)));
  }

  // ---------- event stream ----------
  let link = false;
  function setLink(v) {
    link = v;
    linkPill.className = "pill " + (v ? "ok" : "bad");
    linkPill.hidden = v;
    tx(linkPill, v ? "link.ok" : "link.bad");
    lostBanner.hidden = v || rebooting;
  }
  function onState(data) {
    const prev = ents.get(data.id);
    const e = prev ? Object.assign(prev, data) : data;
    ents.set(data.id, e);
    if (!prev && e.domain) addOther(e);
    (watchers.get(data.id) || []).forEach((fn) => fn(e));
    anyWatchers.forEach((fn) => fn());
  }
  // ESPHome asks EventSource to wait 30 s (retry: 30000) before reconnecting,
  // and a stream that silently stops (device rebooted or off the network)
  // only errors once TCP gives up, so the panel closes the stream itself and
  // reconnects: when it has heard nothing for 25 s (the device pings every
  // 10 s), and during a reboot when a new stream has not opened in 3 s.
  let es = null, reconnectTimer = 0, streamOpen = false, lastHeard = 0;
  function connect() {
    clearTimeout(reconnectTimer);
    const self = (es = new EventSource("/events"));
    streamOpen = false; lastHeard = Date.now();
    const heard = () => { if (self === es) lastHeard = Date.now(); };
    es.addEventListener("open", () => { if (self !== es) return; heard(); streamOpen = true; setLink(true); });
    es.addEventListener("ping", (ev) => {
      if (self !== es) return;
      heard(); setLink(true);
      let d = {};
      try { d = JSON.parse(ev.data || "{}"); } catch (e) { /* keep defaults */ }
      const known = typeof d.uptime === "number";
      if (known) bootAt = Date.now() / 1000 - d.uptime;
      if (d.title) {
        // A full ping (title, ota, log, uptime) starts every new connection.
        info = Object.assign(info, d);
        brandName.textContent = d.title;
        document.title = d.title;
        (watchers.get("__info") || []).forEach((fn) => fn());
        drawer.hidden = info.log === false;
        if (rebooting && (known && bootBefore ? bootAt > bootBefore + 3 : sawDrop)) {
          pendingRestart = false;
          tx($("b", restartBanner), "ban.t");
          endReboot();
        }
      }
    });
    es.addEventListener("state", (ev) => { heard(); try { onState(JSON.parse(ev.data)); } catch (e) { /* malformed event */ } });
    es.addEventListener("log", (ev) => { heard(); onLog(ev.data); });
    es.addEventListener("error", () => { if (self === es) dropStream(); });
  }
  function dropStream() {
    if (es) es.close();
    es = null; streamOpen = false;
    if (rebooting) sawDrop = true;
    setLink(false);
    clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(connect, rebooting ? 1000 : 4000);
  }
  setInterval(() => {
    if (!es) return;
    const quiet = Date.now() - lastHeard;
    if (streamOpen ? quiet > 25000 : rebooting && quiet > 3000) dropStream();
  }, 1000);

  // ---------- routing, language, theme ----------
  function route() {
    let p = location.hash.slice(1);
    if (!sections[p]) p = "overview";
    for (const [id, s] of Object.entries(sections)) s.hidden = id !== p;
    for (const a of nav.children) {
      if (a.dataset.page === p) a.setAttribute("aria-current", "page"); else a.removeAttribute("aria-current");
    }
    const key = PAGES.find(([id]) => id === p)[1];
    tx(pageTitle, key);
  }
  function applyLang() {
    document.documentElement.lang = lang;
    for (const el of i18n) setText(el, t(el._tk, el._tv));
    langBtn.textContent = lang === "pl" ? "EN" : "PL";
    applyTheme();
    for (const [id, fns] of watchers) { const e = ents.get(id); if (e) fns.forEach((fn) => fn(e)); }
    renderDerived();
  }
  langBtn.addEventListener("click", () => { lang = lang === "pl" ? "en" : "pl"; store.set("gbb-lang", lang); applyLang(); });
  const THEMES = ["auto", "light", "dark"];
  let theme = store.get("gbb-theme");
  if (!THEMES.includes(theme)) theme = "auto";
  function applyTheme() {
    if (theme === "auto") document.documentElement.removeAttribute("data-theme");
    else document.documentElement.setAttribute("data-theme", theme);
    themeBtn.replaceChildren(icon(theme === "dark" ? "moon" : theme === "light" ? "sun" : "auto"), themeLbl);
    themeLbl.textContent = t("theme." + theme);
  }
  themeBtn.addEventListener("click", () => { theme = THEMES[(THEMES.indexOf(theme) + 1) % 3]; store.set("gbb-theme", theme); applyTheme(); });
  addEventListener("hashchange", () => { route(); scrollTo(0, 0); });

  document.body.append(h("div", { class: "shell" }, side, main), drawer, overlay);
  langBtn.textContent = lang === "pl" ? "EN" : "PL";
  document.documentElement.lang = lang;
  applyTheme();
  route();
  linkPill.hidden = true; // shown only while the event stream is down
  renderDerived();
  setLogOpen(store.get("gbb-log-open") === "1");
  connect();
})();
