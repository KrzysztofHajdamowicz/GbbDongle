// Header light/dark toggle. With nothing stored the page follows the
// system colour scheme (CSS media query). Clicking stores the opposite of
// what is shown in localStorage "theme" and mirrors it in <html data-theme>;
// a choice equal to the system scheme clears the key, so the page goes back
// to following the system.
(function () {
  var root = document.documentElement;
  var media = window.matchMedia("(prefers-color-scheme: dark)");
  var button = document.querySelector(".theme-toggle");
  if (!button) return;

  function system() { return media.matches ? "dark" : "light"; }
  function current() { return root.dataset.theme || system(); }

  function render() {
    var dark = current() === "dark";
    var label = button.dataset[dark ? "labelLight" : "labelDark"];
    button.setAttribute("aria-label", label);
    button.title = label;
  }

  function apply(theme) {
    var store = theme === system() ? null : theme;
    if (store) root.dataset.theme = store; else delete root.dataset.theme;
    try {
      if (store) localStorage.setItem("theme", store); else localStorage.removeItem("theme");
    } catch (e) { /* storage blocked: the choice lasts for this page only */ }
    render();
  }

  button.addEventListener("click", function () {
    apply(current() === "dark" ? "light" : "dark");
  });
  media.addEventListener("change", render);
  // Keep other open tabs of the site in step.
  window.addEventListener("storage", function (e) {
    if (e.key !== "theme") return;
    if (e.newValue === "dark" || e.newValue === "light") root.dataset.theme = e.newValue;
    else delete root.dataset.theme;
    render();
  });
  button.hidden = false;
  render();
})();
