/**
 * Canonical Explorer routing helpers (TASK-EXPLORER-003).
 * Read-only UI; JSON data via /api/*.
 */
(function () {
  function segmentAfter(prefix) {
    const p = window.location.pathname;
    if (!p.startsWith(prefix)) return "";
    const rest = p.slice(prefix.length).replace(/\/$/, "");
    return rest ? decodeURIComponent(rest) : "";
  }

  function installCanonicalLink() {
    const href = window.location.origin + window.location.pathname;
    let link = document.querySelector('link[rel="canonical"]');
    if (!link) {
      link = document.createElement("link");
      link.rel = "canonical";
      document.head.appendChild(link);
    }
    link.href = href;
  }

  function navHtml() {
    return `
      <a href="/">Home</a>
      <a href="/blocks">Blocks</a>
      <a href="/oru.html">ORU</a>
      <a href="/res.html">RES</a>
      <a href="/res-events.html">RES events</a>
      <a href="/search">Search</a>
    `;
  }

  function mountNav(containerId) {
    const el = document.getElementById(containerId);
    if (!el) return;
    el.classList.add("site-nav");
    el.classList.remove("muted", "row");
    el.innerHTML = navHtml();
  }

  function plural(n, unit) {
    return n === 1 ? `1 ${unit} ago` : `${n} ${unit}s ago`;
  }

  function timeAgo(unixSeconds) {
    const t = Number(unixSeconds);
    if (!Number.isFinite(t) || t <= 0) return "—";
    const diff = Math.max(0, Math.floor(Date.now() / 1000 - t));
    if (diff < 5) return "just now";
    if (diff < 60) return plural(diff, "second");
    const minutes = Math.floor(diff / 60);
    if (minutes < 60) return plural(minutes, "minute");
    const hours = Math.floor(minutes / 60);
    if (hours < 24) return plural(hours, "hour");
    const days = Math.floor(hours / 24);
    if (days < 30) return plural(days, "day");
    const months = Math.floor(days / 30);
    if (months < 12) return plural(months, "month");
    return plural(Math.floor(days / 365), "year");
  }

  function timeAbsolute(unixSeconds) {
    const t = Number(unixSeconds);
    if (!Number.isFinite(t) || t <= 0) return "";
    return new Date(t * 1000).toLocaleString();
  }

  function fetchJson(url, timeoutMs) {
    const ms = timeoutMs == null ? 15000 : timeoutMs;
    const ctrl = new AbortController();
    const timer = setTimeout(function () {
      ctrl.abort();
    }, ms);
    return fetch(url, { signal: ctrl.signal, cache: "no-store" }).finally(function () {
      clearTimeout(timer);
    });
  }

  function timeCellHtml(unixSeconds) {
    const abs = timeAbsolute(unixSeconds);
    if (!abs) return "—";
    const title = abs.replace(/&/g, "&amp;").replace(/"/g, "&quot;");
    return `<span title="${title}">${timeAgo(unixSeconds)}</span>`;
  }

  window.ExplorerRoutes = {
    blockHash: () => segmentAfter("/block/"),
    txid: () => segmentAfter("/tx/"),
    address: () => segmentAfter("/address/"),
    txPath: (txid) => "/tx/" + encodeURIComponent(txid),
    blockPath: (hash) => "/block/" + encodeURIComponent(hash),
    addressPath: (addr) => "/address/" + encodeURIComponent(addr),
    installCanonicalLink,
    mountNav,
    timeAgo,
    timeAbsolute,
    timeCellHtml,
    fetchJson,
    apiBase: "/api",
  };
})();
