/**
 * Chain status dashboard (TASK-EXPLORER-006). Read-only rigord + indexer snapshot.
 */
(function () {
  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function fmt(v) {
    if (v === null || v === undefined || v === "") return "—";
    return escapeHtml(v);
  }

  function dlRow(label, value) {
    return `<dt>${escapeHtml(label)}</dt><dd>${value}</dd>`;
  }

  function shortMono(h) {
    const s = String(h ?? "");
    if (!s) return "—";
    const label = s.length > 20 ? s.slice(0, 12) + "…" + s.slice(-6) : s;
    return `<span class="status-mono" title="${escapeHtml(s)}">${escapeHtml(label)}</span>`;
  }

  function renderSection(title, rows) {
    return `<section class="status-card"><h2>${escapeHtml(title)}</h2><dl>${rows.join("")}</dl></section>`;
  }

  function renderDashboard(data) {
    const id = data.chain_identity ?? {};
    const bh = data.block_health ?? {};
    const eco = data.economic_snapshot ?? {};
    const mine = data.mining_activity ?? {};
    const sys = data.system_integrity ?? {};
    const comp = sys.explorer_compliance ?? {};
    const issued =
      eco.supply_chart && eco.supply_chart.total_issued_oru
        ? eco.supply_chart.total_issued_oru.display
        : eco.oru_supply_cumulative_display;

    const sections = [
      renderSection("Chain identity", [
        dlRow("Product", fmt(id.product ?? id.chain_name)),
        dlRow("Network", fmt(id.network ?? id.network_type)),
        dlRow("Genesis hash", shortMono(id.genesis_hash)),
        dlRow("Node version", fmt(id.node_subversion ?? id.node_version)),
        dlRow("Explorer version", fmt(id.explorer_version)),
        dlRow("Best block", shortMono(bh.best_block_hash)),
      ]),
      renderSection("Block production", [
        dlRow("Height (node)", fmt(bh.current_height_node)),
        dlRow("Avg block time (100)", bh.avg_block_time_seconds_last_100 != null ? `${Number(bh.avg_block_time_seconds_last_100).toFixed(1)} s` : "—"),
        dlRow("Difficulty", fmt(bh.difficulty)),
        dlRow("nBits", fmt(bh.bits)),
        dlRow("OpuHash", shortMono(bh.opus_hash ?? bh.pow_hash)),
        dlRow("Mempool tx count", fmt(mine.mempool_tx_count)),
      ]),
      renderSection("Indexer integrity", [
        dlRow("Height (indexer)", fmt(bh.current_height_indexer)),
        dlRow("Indexer lag (blocks)", fmt(sys.indexer_sync_lag_blocks)),
        dlRow("Indexer lag (seconds)", fmt(sys.indexer_sync_lag_seconds)),
        dlRow("Mismatch active", comp.mismatch_active ? "yes" : "no"),
        dlRow("Reorg signal", `${fmt(bh.reorg_signal_blocks_mismatch_last_1000)} mismatch blocks / 1000`),
        dlRow("IBD", fmt(bh.initialblockdownload)),
      ].concat(sys.rpc_error ? [dlRow("RPC", fmt(sys.rpc_error))] : [])),
      renderSection("Issuance", [
        dlRow("Issued ORU", fmt(issued)),
        dlRow("RES minted", fmt(eco.network_res_minted_display)),
        dlRow("Convergence destroyed ORU", fmt(eco.cumulative_convergence_display ?? eco.oru_burned_lifetime_display)),
        dlRow("ORU destroyed to mint 1 RES", fmt(eco.res_destroy_cost_display)),
        dlRow("Confirming height", fmt(eco.confirm_height ?? eco.tip_height)),
        dlRow("ORU subsidy", fmt(eco.oru_subsidy_tip_display)),
        dlRow("Last mint height", fmt(eco.last_res_height)),
      ]),
    ];

    return `
      <div class="status-grid status-grid--pair">${sections.join("")}</div>
    `;
  }

  async function load() {
    const root = document.getElementById("status-root");
    if (!root) return;
    root.textContent = "Loading…";
    const r = await (window.ExplorerRoutes && ExplorerRoutes.fetchJson
      ? ExplorerRoutes.fetchJson("/api/status")
      : fetch("/api/status", { cache: "no-store" }));
    if (!r.ok) {
      root.textContent = "Failed to load status: HTTP " + r.status;
      return;
    }
    const data = await r.json();
    root.innerHTML = renderDashboard(data);
  }

  window.StatusDashboard = { load, renderDashboard };
})();
