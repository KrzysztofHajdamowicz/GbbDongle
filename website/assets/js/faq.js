// Fold every FAQ question (h3) and the content up to the next heading into a
// <details> block; a #fragment pointing at a question opens it.
(function () {
  var root = document.querySelector(".faq");
  if (!root) return;
  root.querySelectorAll("h3").forEach(function (h) {
    var details = document.createElement("details");
    details.className = "faq-item";
    if (h.id) details.id = h.id;
    var summary = document.createElement("summary");
    summary.innerHTML = h.innerHTML;
    details.appendChild(summary);
    var body = document.createElement("div");
    body.className = "faq-answer";
    var next = h.nextElementSibling;
    while (next && !/^H[1-3]$/.test(next.tagName)) {
      var move = next;
      next = next.nextElementSibling;
      body.appendChild(move);
    }
    details.appendChild(body);
    h.replaceWith(details);
  });
  function openHash() {
    var id = decodeURIComponent(location.hash.slice(1));
    var el = id && document.getElementById(id);
    if (el && el.tagName === "DETAILS") { el.open = true; el.scrollIntoView(); }
  }
  window.addEventListener("hashchange", openHash);
  openHash();
})();
