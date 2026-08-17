/**
 * RIGOR Explorer compliance layer (TASK-EXPLORER-002).
 * Read-only display: footer, chain badge, global mismatch banner.
 */
(function () {
  "use strict";

  var POLL_MS = 30000;
  var pollTimer = null;

  function esc(s) {
    return String(s ?? "")
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function ensureShell() {
    var banner = document.getElementById("rigor-compliance-banner");
    if (!banner) {
      banner = document.createElement("div");
      banner.id = "rigor-compliance-banner";
      banner.setAttribute("role", "alert");
      banner.setAttribute("aria-live", "assertive");
      document.body.insertBefore(banner, document.body.firstChild);
    }

    var header = document.getElementById("rigor-compliance-header");
    if (!header) {
      header = document.createElement("div");
      header.id = "rigor-compliance-header";
      var badge = document.createElement("div");
      badge.id = "rigor-compliance-badge";
      header.appendChild(badge);
      if (banner.nextSibling) {
        document.body.insertBefore(header, banner.nextSibling);
      } else {
        document.body.appendChild(header);
      }
    }

    var footer = document.getElementById("rigor-compliance-footer");
    if (!footer) {
      footer = document.createElement("footer");
      footer.id = "rigor-compliance-footer";
      document.body.appendChild(footer);
    }

    return { banner: banner, header: header, footer: footer };
  }

  function renderBadge(badgeEl, data) {
    badgeEl.innerHTML =
      '<span class="rigor-badge-label">Chain</span>' +
      "<span>" +
      esc(data.product || data.chain_name) +
      "</span>" +
      '<span class="rigor-badge-label">Network</span>' +
      "<span>" +
      esc(data.codename || data.network_type) +
      "</span>" +
      '<span class="rigor-badge-label">Genesis</span>' +
      "<span>" +
      esc(data.genesis_hash_short || "—") +
      "</span>";
  }

  function renderFooter(footerEl, data) {
    footerEl.innerHTML =
      '<div class="rigor-footer-meta">' +
      "<span>Chain: " +
      esc(data.chain_name) +
      "</span>" +
      "<span>Network: " +
      esc(data.network_type) +
      "</span>" +
      "<span>Genesis: " +
      esc(data.genesis_hash_short || "—") +
      "</span>" +
      "<span>Explorer v" +
      esc(data.explorer_version) +
      "</span>" +
      "<span>Build: " +
      esc(data.build_timestamp) +
      "</span>" +
      "</div>";
  }

  function renderBanner(bannerEl, data) {
    if (data.mismatch_active) {
      var items = (data.mismatch_messages || [])
        .map(function (msg) {
          return "<li>" + esc(msg) + "</li>";
        })
        .join("");
      bannerEl.innerHTML =
        "<strong>CRITICAL: Explorer mismatch detected — do not trust indexed data until resolved.</strong>" +
        "<ul>" +
        items +
        "</ul>" +
        "<span>Verify via your own node RPC. This banner cannot be dismissed.</span>";
      bannerEl.classList.add("rigor-compliance-banner--active");
      bannerEl.classList.remove("rigor-compliance-banner--warn");
      document.body.classList.add("rigor-compliance-has-banner");
      return;
    }

    if (data.rpc_degraded) {
      var rpcItems = (data.rpc_messages || [])
        .map(function (msg) {
          return "<li>" + esc(msg) + "</li>";
        })
        .join("");
      bannerEl.innerHTML =
        "<strong>Node RPC busy — compliance checks skipped (not a chain mismatch).</strong>" +
        "<ul>" +
        rpcItems +
        "</ul>" +
        "<span>Work queue full is load on rigord. Indexed pages are not automatically untrusted. Retry shortly.</span>";
      bannerEl.classList.add("rigor-compliance-banner--active");
      bannerEl.classList.add("rigor-compliance-banner--warn");
      document.body.classList.add("rigor-compliance-has-banner");
      return;
    }

    bannerEl.classList.remove("rigor-compliance-banner--active");
    bannerEl.classList.remove("rigor-compliance-banner--warn");
    bannerEl.innerHTML = "";
    document.body.classList.remove("rigor-compliance-has-banner");
  }

  function applyCompliance(data) {
    var shell = ensureShell();
    renderBadge(document.getElementById("rigor-compliance-badge"), data);
    renderFooter(shell.footer, data);
    renderBanner(shell.banner, data);
  }

  function fetchCompliance() {
    return fetch("/compliance", { cache: "no-store", signal: AbortSignal.timeout(15000) }).then(function (r) {
      if (!r.ok) throw new Error("compliance fetch failed: " + r.status);
      return r.json();
    });
  }

  function refresh() {
    fetchCompliance()
      .then(applyCompliance)
      .catch(function (e) {
        var shell = ensureShell();
        renderBanner(shell.banner, {
          mismatch_active: false,
          rpc_degraded: true,
          rpc_messages: ["Compliance endpoint unreachable: " + String(e)],
        });
      });
  }

  function start() {
    refresh();
    if (pollTimer) window.clearInterval(pollTimer);
    pollTimer = window.setInterval(refresh, POLL_MS);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", start);
  } else {
    start();
  }
})();
