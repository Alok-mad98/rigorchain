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
      <a href="/oru.html">ORU amount</a>
      <a href="/res.html">RES amount</a>
      <a href="/res-events.html">RES events</a>
      <a href="/search">Search</a>
      <a href="/status">Status</a>
    `;
  }

  function mountNav(containerId) {
    const el = document.getElementById(containerId);
    if (el) el.innerHTML = navHtml();
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
    apiBase: "/api",
  };
})();
