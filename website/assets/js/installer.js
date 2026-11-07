// Install wizard: device → firmware line → ESP Web Tools button, plus the
// install guide below it, whose steps carry data-net / data-device /
// data-line filters. The choice is mirrored in the URL hash
// (#board=<id>&line=factory|ha) so a link can point at a finished choice.
(function () {
  var wizard = document.getElementById("wizard");
  if (!wizard) return;
  var cfg = JSON.parse(wizard.dataset.config);
  var devices = {};
  cfg.devices.forEach(function (d) { devices[d.id] = d; });

  var state = { device: null, line: null };
  var lineNames = {};
  wizard.querySelectorAll(".wz-option").forEach(function (b) {
    lineNames[b.dataset.line] = b.querySelector(".wz-option-title").firstChild.textContent.trim();
  });

  var steps = {};
  wizard.querySelectorAll(".wz-step").forEach(function (s) { steps[s.dataset.wz] = s; });
  var button = wizard.querySelector("esp-web-install-button");
  var versionP = wizard.querySelector(".wz-version");
  var guide = document.getElementById("guide");
  var guideHint = document.getElementById("guide-hint");

  function manifestUrl() {
    var d = devices[state.device];
    return cfg.firmwareBase + "manifest-" + d.manifest + (state.line === "ha" ? "-ha" : "") + ".json";
  }

  function matches(el) {
    var d = devices[state.device];
    var rules = { net: d && d.net, device: state.device, line: state.line };
    for (var key in rules) {
      var want = el.dataset[key];
      if (want && want.split(/\s+/).indexOf(rules[key]) === -1) return false;
    }
    return true;
  }

  function render() {
    var d = state.device && devices[state.device];
    wizard.querySelectorAll(".wz-device").forEach(function (b) {
      b.setAttribute("aria-pressed", String(b.dataset.device === state.device));
    });
    wizard.querySelectorAll(".wz-option").forEach(function (b) {
      b.setAttribute("aria-pressed", String(b.dataset.line === state.line));
    });

    steps.device.classList.toggle("done", !!d);
    steps.line.setAttribute("aria-disabled", String(!d));
    steps.line.classList.toggle("done", !!(d && state.line));
    steps.flash.setAttribute("aria-disabled", String(!(d && state.line)));

    wizard.querySelector('[data-wz-summary="device"]').textContent =
      d ? d.name + (d.badge ? " · " + d.badge : "") : "";
    wizard.querySelector('[data-wz-summary="line"]').textContent =
      state.line ? lineNames[state.line] : "";

    if (d) {
      document.querySelectorAll('[data-fill="hostname"]').forEach(function (el) { el.textContent = d.hostname; });
      document.querySelectorAll(".fill-host-link").forEach(function (a) { a.href = "http://" + d.hostname + ".local/"; });
      wizard.querySelectorAll('[data-fill="name"]').forEach(function (el) {
        el.textContent = d.name + (d.badge ? " · " + d.badge : "");
      });
      var img = wizard.querySelector(".wz-flash-img");
      img.src = wizard.querySelector('.wz-device[data-device="' + d.id + '"] img').src;
    }
    if (state.line) {
      wizard.querySelectorAll('[data-fill="line"]').forEach(function (el) { el.textContent = lineNames[state.line]; });
    }

    if (d && state.line) {
      var url = manifestUrl();
      if (button.getAttribute("manifest") !== url) {
        button.setAttribute("manifest", url);
        showVersion(url);
      }
    }

    if (guide) {
      var ready = !!(d && state.line);
      guide.hidden = !ready;
      if (guideHint) guideHint.hidden = ready;
      if (ready) {
        guide.querySelectorAll("[data-net], [data-device], [data-line]").forEach(function (el) {
          el.hidden = !matches(el);
        });
      }
    }
  }

  function showVersion(url) {
    versionP.hidden = true;
    fetch(url, { cache: "no-cache" })
      .then(function (r) { if (!r.ok) throw new Error(r.status); return r.json(); })
      .then(function (m) {
        if (!m.version || button.getAttribute("manifest") !== url) return;
        var a = versionP.querySelector("a");
        a.textContent = "v" + m.version;
        a.href = cfg.releaseBase + encodeURIComponent(m.version);
        versionP.hidden = false;
      })
      .catch(function () { /* manifest unreachable (e.g. local preview): no version line */ });
  }

  function writeHash() {
    var parts = [];
    if (state.device) parts.push("board=" + state.device);
    if (state.line) parts.push("line=" + state.line);
    history.replaceState(null, "", parts.length ? "#" + parts.join("&") : location.pathname + location.search);
  }

  function readHash() {
    var params = new URLSearchParams(location.hash.slice(1));
    var dev = params.get("board");
    var line = params.get("line");
    state.device = devices[dev] ? dev : null;
    state.line = state.device && (line === "factory" || line === "ha") ? line : null;
  }

  function scrollTo(step) {
    var top = steps[step].getBoundingClientRect().top;
    if (top < 0 || top > window.innerHeight * 0.6) {
      steps[step].scrollIntoView({ behavior: "smooth", block: "start" });
    }
  }

  wizard.addEventListener("click", function (e) {
    var dev = e.target.closest(".wz-device");
    var opt = e.target.closest(".wz-option");
    var reset = e.target.closest("[data-wz-reset]");
    if (dev) {
      state.device = dev.dataset.device;
      render(); writeHash(); scrollTo(state.line ? "flash" : "line");
    } else if (opt && state.device) {
      state.line = opt.dataset.line;
      render(); writeHash(); scrollTo("flash");
    } else if (reset) {
      if (reset.dataset.wzReset === "device") state.device = null;
      state.line = null;
      render(); writeHash(); scrollTo(reset.dataset.wzReset);
    }
  });

  window.addEventListener("hashchange", function () { readHash(); render(); });
  readHash();
  render();
})();
