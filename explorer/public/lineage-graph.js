/**
 * Convergence lineage graph (TASK-EXPLORER-009). Read-only indexed relationships.
 */
(function () {
  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function shortHash(h) {
    if (!h) return "";
    const s = String(h);
    return s.length > 20 ? s.slice(0, 12) + "…" + s.slice(-6) : s;
  }

  function stateClass(state) {
    return "state-" + String(state || "Candidate").replace(/\s+/g, "");
  }

  function nodeLink(node) {
    if (!node.href) {
      return node.value ? `<span class="mono">${escapeHtml(shortHash(node.value))}</span>` : '<span class="muted">—</span>';
    }
    const label = node.value ? shortHash(node.value) : node.label;
    return `<a href="${escapeHtml(node.href)}">${escapeHtml(label)}</a>`;
  }

  function renderNode(node) {
    return `<div class="lineage-graph-node ${stateClass(node.state)}" data-kind="${escapeHtml(node.kind)}">
      <div class="lineage-graph-node-kind">${escapeHtml(node.label)}</div>
      <div>${nodeLink(node)}</div>
      <div class="lineage-graph-node-state">${escapeHtml(node.state)}</div>
    </div>`;
  }

  function arrow() {
    return '<div class="lineage-graph-arrow" aria-hidden="true">↓</div>';
  }

  function renderChainNodes(nodes) {
    return nodes.map((n) => renderNode(n) + arrow()).join("").replace(/↓$/, "");
  }

  function renderGraph(graph) {
    const byId = Object.fromEntries((graph.nodes || []).map((n) => [n.id, n]));
    const candidates = (graph.nodes || []).filter((n) => n.kind === "candidate");
    const winner = (graph.nodes || []).find((n) => n.kind === "winner");
    const chainKinds = ["convergence_root", "sigma_digest", "pcorb", "res_mint"];
    const chain = chainKinds.map((k) => (graph.nodes || []).find((n) => n.kind === k)).filter(Boolean);

    let html = '<div class="lineage-graph-canvas">';

    const setNode = byId["candidate_set"];
    if (setNode) {
      html += renderNode(setNode) + arrow();
    }

    if (candidates.length > 1) {
      html += `<div class="lineage-graph-branch-row">${candidates.map(renderNode).join("")}</div>` + arrow();
    } else if (candidates.length === 1) {
      html += renderNode(candidates[0]) + arrow();
    }

    if (winner) {
      html += renderNode(winner) + arrow();
    }

    html += renderChainNodes(chain);
    html += "</div>";
    return html;
  }

  function renderSummary(graph) {
    const rows = [
      ["|F|", String(graph.f_set_size ?? 0)],
      ["winner_txid", graph.winner?.txid ?? "—"],
      ["convergence_root", graph.convergence_root ?? "—"],
      ["selection_digest_h", graph.sigma?.selection_digest_h ?? "—"],
      ["mint_txid", graph.mint?.mint_txid ?? "—"],
      ["mint_height", graph.mint?.mint_height != null ? String(graph.mint.mint_height) : "—"],
    ];
    return `<dl class="lineage-graph-summary">${rows
      .map(([k, v]) => `<dt>${escapeHtml(k)}</dt><dd>${escapeHtml(v)}</dd>`)
      .join("")}</dl>`;
  }

  function renderLegend(graph) {
    const items = graph.legend || [];
    return `<div class="lineage-graph-legend"><strong>Legend</strong><ul>${items
      .map((i) => `<li><strong>${escapeHtml(i.kind)}</strong> — ${escapeHtml(i.description)}</li>`)
      .join("")}</ul>
      <p class="muted">Visual states: Candidate, Winner, Minted, Confirmed, Orphaned (indexed mismatch), Revoked (reserved).</p>
    </div>`;
  }

  function renderPcorbTable(graph) {
    const items = graph.pcorb?.items;
    if (!items || items.length === 0) return "";
    const rows = items
      .map(
        (it) =>
          `<tr><td>${it.index}</td><td>${escapeHtml(it.field_name)}</td><td class="mono">${escapeHtml(shortHash(it.hex))}</td></tr>`,
      )
      .join("");
    return `<div class="lineage-graph-pcorb"><strong>PCORB stack</strong><table><thead><tr><th>#</th><th>field</th><th>hex</th></tr></thead><tbody>${rows}</tbody></table></div>`;
  }

  function renderMintOutputs(graph) {
    const outs = graph.mint?.res_outputs || [];
    if (outs.length === 0) return "";
    const lines = outs
      .map((o) => {
        const addr = o.address_href
          ? `<a href="${escapeHtml(o.address_href)}">${escapeHtml(o.address || "")}</a>`
          : escapeHtml(o.address || "—");
        return `<li>${addr} — ${escapeHtml(o.amount_display)}</li>`;
      })
      .join("");
    return `<div class="lineage-graph-mint-outputs"><strong>RES mint outputs</strong><ul>${lines}</ul></div>`;
  }

  function renderLineagePanel(graph, routes) {
    if (!graph || !graph.visible) return "";

    const blockLink =
      graph.block_context?.block_href && graph.block_context?.block_hash
        ? ` <a href="${escapeHtml(graph.block_context.block_href)}">block ${graph.block_context.block_height ?? ""}</a>`
        : "";

    return `<section class="lineage-graph-panel" id="lineage-graph-panel">
      <h2>Convergence Lineage Graph</h2>
      <div class="lineage-graph-notice" role="note">${escapeHtml(graph.readonly_notice || "")}</div>
      <div class="lineage-graph-notice muted">${escapeHtml(graph.rpc_authority_notice || "")}${blockLink}</div>
      ${renderSummary(graph)}
      ${renderGraph(graph)}
      ${renderPcorbTable(graph)}
      ${renderMintOutputs(graph)}
      ${renderLegend(graph)}
      <p class="muted">equivalence_claimed: false</p>
    </section>`;
  }

  window.LineageGraph = { renderLineagePanel };
})();
