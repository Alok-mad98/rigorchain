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

  function renderSection(title, rows) {
    return `<section class="status-card"><h2>${escapeHtml(title)}</h2><dl>${rows.join("")}</dl></section>`;
  }

  function renderDashboard(data) {
    const id = data.chain_identity ?? {};
    const bh = data.block_health ?? {};
    const ch = data.convergence_health ?? {};
    const eco = data.economic_snapshot ?? {};
    const mine = data.mining_activity ?? {};
    const sys = data.system_integrity ?? {};
    const comp = sys.explorer_compliance ?? {};

    const genesisFull = id.genesis_hash ?? "";
    const genesisRow = genesisFull
      ? `<span class="status-mono">${fmt(genesisFull)}</span>`
      : "—";

    const sections = [
      renderSection("S1 — Chain identity", [
        dlRow("Product", fmt(id.product ?? id.chain_name)),
        dlRow("Codename", fmt(id.codename ?? id.network_type)),
        dlRow("Chain", fmt(id.chain_name)),
        dlRow("Network", fmt(id.network)),
        dlRow("Network type", fmt(id.network_type)),
        dlRow("Genesis hash", genesisRow),
        dlRow("Node version", fmt(id.node_subversion ?? id.node_version)),
        dlRow("Explorer version", fmt(id.explorer_version)),
      ]),
      renderSection("S2 — Block production health", [
        dlRow("Height (node)", fmt(bh.current_height_node)),
        dlRow("Height (indexer)", fmt(bh.current_height_indexer)),
        dlRow("Avg block time (100)", bh.avg_block_time_seconds_last_100 != null ? `${Number(bh.avg_block_time_seconds_last_100).toFixed(1)} s` : "—"),
        dlRow("Difficulty", fmt(bh.difficulty)),
        dlRow("PoW display", `${fmt(bh.bits)} / ${fmt(bh.opus_hash)}`),
        dlRow("Reorg signal", `${fmt(bh.reorg_signal_blocks_mismatch_last_1000)} mismatch blocks / 1000`),
        dlRow("IBD", fmt(bh.initialblockdownload)),
        dlRow("Mempool tx count", fmt(mine.mempool_tx_count)),
      ]),
      renderSection("S3 — Convergence & integrity", [
        dlRow("Winner blocks / 100", fmt(ch.winner_blocks_last_100)),
        dlRow("Winner frequency", ch.winner_frequency_last_100 != null ? `${(Number(ch.winner_frequency_last_100) * 100).toFixed(1)}%` : "—"),
        dlRow("Active convergences", fmt(ch.active_convergences)),
        dlRow("Pending in F", fmt(ch.pending_windows)),
        dlRow("Indexer lag (blocks)", fmt(sys.indexer_sync_lag_blocks)),
        dlRow("Indexer lag (seconds)", fmt(sys.indexer_sync_lag_seconds)),
        dlRow("Mismatch active", comp.mismatch_active ? "yes" : "no"),
        dlRow("Draw roots & |F|", '<a href="/res.html">RES amount</a>'),
      ]),
      renderSection("S4 — Network RES / ORU", [
        dlRow("Network RES minted", fmt(eco.network_res_minted_display)),
        dlRow("Total issued ORU", fmt((eco.supply_chart && eco.supply_chart.total_issued_oru && eco.supply_chart.total_issued_oru.display) || eco.oru_supply_cumulative_display)),
        dlRow("Convergence destroyed ORU", fmt(eco.cumulative_convergence_display ?? eco.oru_burned_lifetime_display)),
        dlRow("Deflationary Fee Burn ORU", fmt(eco.rfc018_fee_burn && eco.rfc018_fee_burn.cumulative_burned_oru_display)),
        dlRow("Circulating spendable ORU", fmt(eco.oru_circulating_spendable_display)),
        dlRow("ORU in current window", fmt(eco.oru_in_current_window_display)),
        dlRow("ORU since last mint", fmt(eco.oru_since_last_mint_display)),
        dlRow("Mature transferable ORU", fmt(eco.oru_mature_transferable_display)),
        dlRow("Immature coinbase ORU", fmt(eco.oru_immature_coinbase_display)),
        dlRow("RES minted (last 100)", fmt(eco.res_mint_rate_last_100_blocks_display)),
      ]),
    ];

    return `
      <div class="status-grid">${sections.join("")}</div>
      <p class="status-updated">Snapshot: ${escapeHtml(data.generated_at ?? "")}</p>
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
