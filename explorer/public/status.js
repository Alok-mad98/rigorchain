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

  function copyBtn(text) {
    return `<button type="button" class="convergence-copy-btn" data-copy="${escapeHtml(text)}">Copy</button>`;
  }

  function renderSection(title, rows) {
    return `<section class="status-card"><h2>${escapeHtml(title)}</h2><dl>${rows.join("")}</dl></section>`;
  }

  function renderDistribution(dist) {
    if (!dist || typeof dist !== "object") return "—";
    return Object.entries(dist)
      .sort((a, b) => Number(a[0]) - Number(b[0]))
      .map(([k, v]) => `|F|=${k}: ${v} block(s)`)
      .join("\n");
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
      ? `${fmt(id.genesis_hash_short)} ${copyBtn(genesisFull)} <span class="status-mono">${fmt(genesisFull)}</span>`
      : "—";

    const sections = [
      renderSection("S1 — Chain identity", [
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
        dlRow("Orphan rate", `${fmt(bh.orphan_rate)} <span class="status-muted">${fmt(bh.orphan_rate_note)}</span>`),
        dlRow("Reorg signal", `${fmt(bh.reorg_signal_blocks_mismatch_last_1000)} mismatch blocks / 1000`),
        dlRow("IBD", fmt(bh.initialblockdownload)),
      ]),
      renderSection("S3 — Convergence system", [
        dlRow("Draw status", `<span class="status-draw-banner">${escapeHtml(ch.draw_status ?? "")}</span>`),
        dlRow("|F| distribution", `<pre class="status-dist">${escapeHtml(renderDistribution(ch.f_set_distribution_last_100_blocks))}</pre>`),
        dlRow("Winner blocks / 100", fmt(ch.winner_blocks_last_100)),
        dlRow("Winner frequency", ch.winner_frequency_last_100 != null ? `${(Number(ch.winner_frequency_last_100) * 100).toFixed(1)}%` : "—"),
        dlRow("Root stability", ch.convergence_root_stability
          ? `${ch.convergence_root_stability.distinct_committed_roots_last_100} distinct roots; mismatch blocks: ${ch.convergence_root_stability.mismatch_blocks_last_100}`
          : "—"),
      ]),
      renderSection("S4 — RES / ORU economic snapshot", [
        dlRow("ORU indexed UTXO total", fmt(eco.oru_indexed_utxo_total_display)),
        dlRow("ORU cumulative convergence (RPC)", fmt(eco.oru_cumulative_convergence_rpc)),
        dlRow("Convergence-fee absorption", fmt(eco.convergence_fee_absorption_window_rpc)),
        dlRow("Fee metric label", fmt(eco.convergence_fee_label)),
        dlRow("RES minted total", fmt(eco.res_minted_total_display)),
        dlRow("RES mint rate (100 blk)", fmt(eco.res_mint_rate_last_100_blocks_display)),
      ]),
      renderSection("S5 — Mempool / mining", [
        dlRow("Mempool tx count", fmt(mine.mempool_tx_count)),
        dlRow("Active miners", fmt(mine.active_miners)),
        dlRow("Pending block jobs", fmt(mine.pending_blocks_active_jobs)),
        dlRow("Stale jobs", fmt(mine.stale_job_count)),
        dlRow("Note", `<span class="status-muted">${fmt(mine.availability_note)}</span>`),
      ]),
      renderSection("S6 — System integrity", [
        dlRow("RPC authority", fmt(sys.rpc_authority)),
        dlRow("Indexer lag (blocks)", fmt(sys.indexer_sync_lag_blocks)),
        dlRow("Indexer lag (seconds)", fmt(sys.indexer_sync_lag_seconds)),
        dlRow("Wallet observer", fmt(sys.wallet_observer?.status ?? sys.wallet_observer)),
        dlRow("Compliance frozen", comp.frozen ? "yes" : "no"),
        dlRow("Mismatch active", comp.mismatch_active ? "yes" : "no"),
        dlRow("equivalence_claimed", fmt(comp.equivalence_claimed)),
      ]),
    ];

    return `
      <div class="status-rpc-disclaimer" role="note">${escapeHtml(data.rpc_authority_disclaimer ?? "")}</div>
      <div class="status-grid">${sections.join("")}</div>
      <p class="status-updated">Snapshot: ${escapeHtml(data.generated_at ?? "")}</p>
    `;
  }

  async function load() {
    const root = document.getElementById("status-root");
    if (!root) return;
    root.textContent = "Loading…";
    const r = await fetch("/api/status");
    if (!r.ok) {
      root.textContent = "Failed to load status: HTTP " + r.status;
      return;
    }
    const data = await r.json();
    root.innerHTML = renderDashboard(data);
    if (window.ConvergencePanel?.bindCopyButtons) {
      ConvergencePanel.bindCopyButtons(root);
    } else {
      root.querySelectorAll("[data-copy]").forEach((btn) => {
        btn.addEventListener("click", () => {
          const text = btn.getAttribute("data-copy") || "";
          navigator.clipboard.writeText(text);
        });
      });
    }
  }

  window.StatusDashboard = { load, renderDashboard };
})();
