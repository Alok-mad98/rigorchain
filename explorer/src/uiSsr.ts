/**
 * Server-render Explorer HTML shells so block/tx pages are visible without a
 * second client fetch. Presentation only; data still comes from indexed RPC.
 */

import { readFileSync } from "node:fs";
import { DRAW_FINGERPRINT_LABEL, DRAW_IDENTITY_HEADING } from "./consensusFingerprint";
import { emptyEligibleRootNote } from "./convergencePanel";
import { destinationLabel } from "./txSummary";

export function escapeHtml(s: unknown): string {
  return String(s ?? "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function shortHash(h: unknown): string {
  const s = String(h ?? "");
  if (!s) return "";
  return s.length > 20 ? `${s.slice(0, 12)}…${s.slice(-6)}` : s;
}

function timeAgo(unixSeconds: unknown): string {
  const t = Number(unixSeconds);
  if (!Number.isFinite(t) || t <= 0) return "—";
  const diff = Math.max(0, Math.floor(Date.now() / 1000 - t));
  if (diff < 5) return "just now";
  if (diff < 60) return `${diff} second${diff === 1 ? "" : "s"} ago`;
  const minutes = Math.floor(diff / 60);
  if (minutes < 60) return `${minutes} minute${minutes === 1 ? "" : "s"} ago`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours} hour${hours === 1 ? "" : "s"} ago`;
  const days = Math.floor(hours / 24);
  return `${days} day${days === 1 ? "" : "s"} ago`;
}

function timeAbsolute(unixSeconds: unknown): string {
  const t = Number(unixSeconds);
  if (!Number.isFinite(t) || t <= 0) return "";
  return new Date(t * 1000).toISOString();
}

export function injectSsrRoot(templateHtml: string, innerHtml: string): string {
  const rendered = `<div id="root" data-ssr="1">${innerHtml}</div>`;
  if (templateHtml.includes('<div id="root">Loading...</div>')) {
    return templateHtml.replace('<div id="root">Loading...</div>', rendered);
  }
  if (templateHtml.includes('<div id="root">Loading…</div>')) {
    return templateHtml.replace('<div id="root">Loading…</div>', rendered);
  }
  if (templateHtml.includes('id="status-root"')) {
    return templateHtml.replace(
      /<div id="status-root"[^>]*>Loading…<\/div>/,
      `<div id="status-root" class="status-dashboard" data-ssr="1">${innerHtml}</div>`,
    );
  }
  return templateHtml;
}

export function readPublicTemplate(publicDir: string, file: string): string {
  return readFileSync(`${publicDir}/${file}`, "utf8");
}

function renderBlockConvergence(panel: Record<string, unknown> | null | undefined): string {
  if (!panel) return "";
  const status = escapeHtml(panel.status ?? "");
  const cRoot = escapeHtml(panel.c_root ?? panel.convergence_root ?? "—");
  const committed = String(
    panel.draw_candidate_root_committed ?? panel.draw_candidate_root ?? "",
  );
  const computed = String(panel.draw_candidate_root_computed ?? "");
  const fSize = escapeHtml(panel.candidate_set_size ?? 0);
  const eligible = escapeHtml(panel.eligible_count ?? 0);
  const winner = panel.winner_txid
    ? `<a href="/tx/${encodeURIComponent(String(panel.winner_txid))}">${escapeHtml(panel.winner_txid)}</a>`
    : '<span class="muted">—</span>';
  const ids = Array.isArray(panel.candidate_txids) ? panel.candidate_txids : [];
  const list =
    ids.length > 0
      ? ids
          .map((id) => `<li><a href="/tx/${encodeURIComponent(String(id))}">${escapeHtml(id)}</a></li>`)
          .join("")
      : '<li class="muted">No candidates indexed for this block.</li>';
  const committedNote = emptyEligibleRootNote(committed);
  const computedNote = emptyEligibleRootNote(computed);
  const committedHint = committedNote ? ` · ${committedNote}` : "";
  const computedHint = computedNote ? ` · ${computedNote}` : "";
  return `<section class="convergence-panel" id="convergence-panel">
    <h2>Convergence</h2>
    <dl>
      <dt>Convergence Status</dt><dd><span class="convergence-status">${status}</span></dd>
      <dt>Candidate Set Size |F|</dt><dd>|F|=${fSize}</dd>
      <dt>C (convergence_root)</dt><dd><span class="convergence-root-value">${cRoot}</span> <span class="muted">|F|=${fSize}</span></dd>
      <dt>draw_candidate_root_committed</dt><dd><span class="convergence-root-value">${escapeHtml(committed || "—")}</span> <span class="muted">pays parent draw${committedHint}</span></dd>
      <dt>draw_candidate_root_computed</dt><dd><span class="convergence-root-value">${escapeHtml(computed || "—")}</span> <span class="muted">this block awards · eligible=${eligible}${computedHint}</span></dd>
      <dt>winner_txid</dt><dd>${winner}</dd>
      <dt>candidate_txids</dt><dd><ul class="convergence-candidate-list">${list}</ul></dd>
    </dl>
  </section>`;
}

export function renderBlockRoot(data: Record<string, unknown>, hash: string): string {
  const b = (data.block as Record<string, unknown>) || {};
  const s = (data.summary as Record<string, unknown>) || {};
  const panel = data.convergence_panel as Record<string, unknown> | undefined;
  const txs = (data.txs as Array<Record<string, unknown>>) || [];
  const height = s.height ?? b.height ?? "";
  const blockHash = String(s.hash ?? b.block_hash ?? hash);
  const prev = String(s.previous_block_hash ?? b.previous_block_hash ?? "");
  const fp = String(s.consensus_fingerprint_display ?? b.consensus_fingerprint_display ?? "Not available");
  const fpStatus = String(s.consensus_fingerprint_status ?? b.consensus_fingerprint_status ?? "Not available");
  const t = s.time ?? b.time;
  const abs = timeAbsolute(t);
  const rows = txs
    .map((tx, idx) => {
      return `<tr>
        <td>${idx}</td>
        <td>${escapeHtml(tx.tx_kind_label ?? (tx.is_coinbase ? "Coinbase" : ""))}</td>
        <td>${escapeHtml(tx.amount_display ?? "—")}</td>
        <td>${escapeHtml(tx.asset_kind ?? "—")}</td>
        <td><a href="/tx/${encodeURIComponent(String(tx.txid ?? ""))}">${escapeHtml(shortHash(tx.txid))}</a></td>
      </tr>`;
    })
    .join("");
  return `<section class="verify-independently-panel">
      <h2>Block #${escapeHtml(height)}</h2>
      <p class="mono">Hash: ${escapeHtml(blockHash)}</p>
      <p>Previous Block: ${prev ? `<a class="mono" href="/block/${encodeURIComponent(prev)}">${escapeHtml(prev)}</a>` : "N/A"}</p>
      <section class="consensus-identity-panel">
        <h3>${DRAW_IDENTITY_HEADING}</h3>
        <p>${DRAW_FINGERPRINT_LABEL}:</p>
        <p class="mono consensus-fingerprint-value">${escapeHtml(fp)}</p>
        <p>Status: ${escapeHtml(fpStatus)}</p>
      </section>
      <p class="muted">Rendered by Explorer from indexed rigord data (no second browser fetch required).</p>
    </section>
    ${renderBlockConvergence(panel)}
    <h2>Summary</h2>
    <table><tbody>
      <tr><th>Height</th><td>${escapeHtml(height)}</td></tr>
      <tr><th>Hash</th><td class="mono">${escapeHtml(blockHash)}</td></tr>
      <tr><th>Time</th><td>${escapeHtml(timeAgo(t))}${abs ? ` <span class="muted">(${escapeHtml(abs)})</span>` : ""}</td></tr>
      <tr><th>Coinbase</th><td class="mono">${s.coinbase_txid ? `<a href="/tx/${encodeURIComponent(String(s.coinbase_txid))}">${escapeHtml(s.coinbase_txid)}</a>` : ""}</td></tr>
      <tr><th>ORU subsidy</th><td>${escapeHtml(s.oru_subsidy_display ?? "N/A")}</td></tr>
      <tr><th>RES minted</th><td>${escapeHtml(s.res_minted_display ?? "0")}</td></tr>
      ${
        s.res_award_note
          ? `<tr><th>RES award</th><td>${escapeHtml(s.res_award_note)}${
              s.res_payout_block_hash
                ? ` (<a href="/block/${encodeURIComponent(String(s.res_payout_block_hash))}">payout block</a>)`
                : ""
            }</td></tr>`
          : ""
      }
      <tr><th>RES probability target</th><td class="mono">${escapeHtml(s.res_probability_target_display ?? "N/A")}</td></tr>
      <tr><th>bind_height</th><td>${escapeHtml(s.bind_height_display ?? "Not available")}</td></tr>
    </tbody></table>
    <h2>Header</h2>
    <pre class="mono">${escapeHtml(JSON.stringify(b, null, 2))}</pre>
    <h2>Transactions</h2>
    <table>
      <thead><tr><th>#</th><th>kind</th><th>amount</th><th>asset</th><th>txid</th></tr></thead>
      <tbody>${rows}</tbody>
    </table>`;
}

function renderTxConvergence(panel: Record<string, unknown> | null | undefined): string {
  if (!panel) return "";
  let heightHtml = '<span class="muted">—</span>';
  if (panel.block_height != null) {
    heightHtml = escapeHtml(panel.block_height);
    if (panel.block_hash) {
      heightHtml += ` (<a href="/block/${encodeURIComponent(String(panel.block_hash))}">block</a>)`;
    }
    if (
      panel.window_last_height != null &&
      Number(panel.window_last_height) > Number(panel.block_height) &&
      panel.role === "Candidate"
    ) {
      heightHtml += ` <span class="muted">— pending in F through height ${escapeHtml(panel.window_last_height)}</span>`;
    }
  }
  return `<section class="convergence-panel" id="convergence-panel">
    <h2>Convergence</h2>
    <dl>
      <dt>Convergence Role</dt><dd><span class="convergence-status">${escapeHtml(panel.role)}</span></dd>
      <dt>Convergence Block Height</dt><dd>${heightHtml}</dd>
      <dt>Winner Reference</dt><dd><span class="muted">—</span></dd>
    </dl>
  </section>`;
}

export function renderTxRoot(data: Record<string, unknown>, txid: string): string {
  const sum = (data.summary as Record<string, unknown>) || {};
  const fee = (sum.fee as Record<string, unknown>) || {};
  const panel = data.convergence_panel as Record<string, unknown> | undefined;
  const vout = (data.vout as Array<Record<string, unknown>>) || [];
  const destLabel = destinationLabel(typeof sum.amount_role === "string" ? sum.amount_role : null);
  const amountLabel = sum.is_coinbase ? "Subsidy" : "Amount";
  const amount = sum.is_coinbase ? (sum.subsidy_display ?? sum.amount_display) : sum.amount_display;
  const extra =
    !sum.is_coinbase && sum.amount_total_display && sum.amount_total_display !== sum.amount_display
      ? ` <span class="muted">(outputs total ${escapeHtml(sum.amount_total_display)})</span>`
      : "";
  const voutRows = vout
    .map((o) => {
      const kind = String(o.asset_kind ?? "");
      const atoms = String(o.value_atoms ?? "0");
      let valueStr = atoms;
      try {
        if (kind === "ORU") {
          const a = BigInt(atoms);
          valueStr = `${a / 100000000n}.${(a % 100000000n).toString().padStart(8, "0")}`;
        } else if (kind === "RES") {
          const a = BigInt(atoms);
          valueStr = `${a / 100n}.${(a % 100n).toString().padStart(2, "0")}`;
        }
      } catch {
        /* keep raw */
      }
      const cvg2 =
        kind === "ORU" && Number(o.witness_version) === 1
          ? ' <span class="muted">CVG2 / unspendable convergence script</span>'
          : "";
      const addr = o.address
        ? `<a href="/address/${encodeURIComponent(String(o.address))}">${escapeHtml(o.address)}</a>${cvg2}`
        : "";
      return `<tr><td>${escapeHtml(o.vout_index)}</td><td>${escapeHtml(kind)}</td><td>${escapeHtml(valueStr)}</td><td>${addr}</td></tr>`;
    })
    .join("");
  return `<section class="verify-independently-panel">
      <h2>Transaction</h2>
      <p class="mono">TXID: ${escapeHtml(txid)}</p>
      <p class="muted">${DRAW_FINGERPRINT_LABEL}: see the including block (observational short ID, not a consensus check).</p>
    </section>
    ${renderTxConvergence(panel)}
    <h2>Summary</h2>
    <table><tbody>
      <tr><th>Sender</th><td class="mono">${escapeHtml(sum.sender ?? "N/A")}</td></tr>
      <tr><th>${escapeHtml(destLabel)}</th><td class="mono">${sum.receiver ? `<a href="/address/${encodeURIComponent(String(sum.receiver))}">${escapeHtml(sum.receiver)}</a>` : "N/A"}</td></tr>
      <tr><th>${escapeHtml(amountLabel)}</th><td>${escapeHtml(amount ?? "N/A")}${extra}</td></tr>
      <tr><th>Fee</th><td>${escapeHtml(fee.fee_display ?? "N/A")}</td></tr>
    </tbody></table>
    <h2>Outputs</h2>
    <table>
      <thead><tr><th>vout</th><th>asset</th><th>amount</th><th>address</th></tr></thead>
      <tbody>${voutRows}</tbody>
    </table>`;
}

export function renderBlocksRoot(blocks: Array<Record<string, unknown>>): string {
  const rows = blocks
    .map(
      (b) => `<tr>
        <td>${escapeHtml(b.height)}</td>
        <td><a href="/block/${encodeURIComponent(String(b.block_hash ?? ""))}">${escapeHtml(shortHash(b.block_hash))}</a></td>
        <td>${escapeHtml(timeAgo(b.time))}</td>
        <td>${escapeHtml(b.tx_count)}</td>
        <td>${escapeHtml(b.oru_minted_display ?? b.oru_minted ?? "—")}</td>
        <td>${escapeHtml(b.candidate_set_size ?? 0)}</td>
        <td>${escapeHtml(shortHash(b.rigor_winner))}</td>
        <td class="fp-col muted">${escapeHtml(b.consensus_fingerprint_short ?? "N/A")}</td>
        <td class="muted">${escapeHtml(shortHash(b.c_root))}</td>
        <td class="muted">${escapeHtml(shortHash(b.committed_root ?? b.draw_candidate_root))}</td>
        <td class="muted">${escapeHtml(shortHash(b.computed_root))}</td>
      </tr>`,
    )
    .join("");
  return `<table>
    <thead><tr><th>height</th><th>hash</th><th>time</th><th>tx_count</th><th>oru_minted</th><th>|F|</th><th>winner</th><th class="fp-col">${DRAW_FINGERPRINT_LABEL}</th><th>C (convergence_root)</th><th>committed</th><th>computed</th></tr></thead>
    <tbody>${rows}</tbody>
  </table>`;
}

function statusFmt(v: unknown): string {
  if (v === null || v === undefined || v === "") return "—";
  return escapeHtml(v);
}

function statusDl(label: string, valueHtml: string): string {
  return `<dt>${escapeHtml(label)}</dt><dd>${valueHtml}</dd>`;
}

function statusSection(title: string, rows: string[]): string {
  return `<section class="status-card"><h2>${escapeHtml(title)}</h2><dl>${rows.join("")}</dl></section>`;
}

/** Same S1–S4 fields as public/status.js renderDashboard. */
export function renderStatusRoot(data: Record<string, unknown>): string {
  const id = (data.chain_identity as Record<string, unknown>) || {};
  const bh = (data.block_health as Record<string, unknown>) || {};
  const ch = (data.convergence_health as Record<string, unknown>) || {};
  const eco = (data.economic_snapshot as Record<string, unknown>) || {};
  const mine = (data.mining_activity as Record<string, unknown>) || {};
  const sys = (data.system_integrity as Record<string, unknown>) || {};
  const comp = (sys.explorer_compliance as Record<string, unknown>) || {};
  const genesisFull = id.genesis_hash ?? "";
  const genesisRow = genesisFull
    ? `<span class="status-mono">${statusFmt(genesisFull)}</span>`
    : "—";
  const avg =
    bh.avg_block_time_seconds_last_100 != null
      ? `${Number(bh.avg_block_time_seconds_last_100).toFixed(1)} s`
      : "—";
  const freq =
    ch.winner_frequency_last_100 != null
      ? `${(Number(ch.winner_frequency_last_100) * 100).toFixed(1)}%`
      : "—";

  const sections = [
    statusSection("S1 — Chain identity", [
      statusDl("Product", statusFmt(id.product ?? id.chain_name)),
      statusDl("Codename", statusFmt(id.codename ?? id.network_type)),
      statusDl("Chain", statusFmt(id.chain_name)),
      statusDl("Network", statusFmt(id.network)),
      statusDl("Network type", statusFmt(id.network_type)),
      statusDl("Genesis hash", genesisRow),
      statusDl("Node version", statusFmt(id.node_subversion ?? id.node_version)),
      statusDl("Explorer version", statusFmt(id.explorer_version)),
    ]),
    statusSection("S2 — Block production health", [
      statusDl("Height (node)", statusFmt(bh.current_height_node)),
      statusDl("Height (indexer)", statusFmt(bh.current_height_indexer)),
      statusDl("Avg block time (100)", avg),
      statusDl("Difficulty", statusFmt(bh.difficulty)),
      statusDl("PoW display", `${statusFmt(bh.bits)} / ${statusFmt(bh.opus_hash)}`),
      statusDl(
        "Reorg signal",
        `${statusFmt(bh.reorg_signal_blocks_mismatch_last_1000)} mismatch blocks / 1000`,
      ),
      statusDl("IBD", statusFmt(bh.initialblockdownload)),
      statusDl("Mempool tx count", statusFmt(mine.mempool_tx_count)),
    ]),
    statusSection("S3 — Convergence & integrity", [
      statusDl("Winner blocks / 100", statusFmt(ch.winner_blocks_last_100)),
      statusDl("Winner frequency", freq),
      statusDl("Active convergences", statusFmt(ch.active_convergences)),
      statusDl("Pending in F", statusFmt(ch.pending_windows)),
      statusDl("Indexer lag (blocks)", statusFmt(sys.indexer_sync_lag_blocks)),
      statusDl("Indexer lag (seconds)", statusFmt(sys.indexer_sync_lag_seconds)),
      statusDl("Mismatch active", comp.mismatch_active ? "yes" : "no"),
      statusDl("Draw roots & |F|", '<a href="/res.html">RES amount</a>'),
    ]),
    statusSection("S4 — Network RES / ORU", [
      statusDl("Network RES minted", statusFmt(eco.network_res_minted_display)),
      statusDl("ORU burned (lifetime)", statusFmt(eco.oru_burned_lifetime_display)),
      statusDl("ORU in current window", statusFmt(eco.oru_in_current_window_display)),
      statusDl("ORU since last mint", statusFmt(eco.oru_since_last_mint_display)),
      statusDl("Mature transferable ORU", statusFmt(eco.oru_mature_transferable_display)),
      statusDl("Immature coinbase ORU", statusFmt(eco.oru_immature_coinbase_display)),
      statusDl("RES minted (last 100)", statusFmt(eco.res_mint_rate_last_100_blocks_display)),
    ]),
  ];

  return `<div class="status-grid">${sections.join("")}</div>
  <p class="status-updated">Snapshot: ${escapeHtml(data.generated_at ?? "")}</p>`;
}
