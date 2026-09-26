/**
 * Convergence proof visualization panel. Display only.
 */
(function () {
  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function flowStep(title, body) {
    return `<div class="converge-proof-flow-step"><strong>${escapeHtml(title)}</strong>${body}</div>`;
  }

  function flowArrow() {
    return '<div class="converge-proof-flow-arrow" aria-hidden="true">↓</div>';
  }

  function renderFlow(panel, routes) {
    const txPath = routes.txPath.bind(routes);
    const blockPath = routes.blockPath.bind(routes);
    const link = panel;
    const steps = [];

    const burnTxid = link.winner_txid || link.mint_txid;
    const burnBody = burnTxid
      ? `<a href="${escapeHtml(txPath(burnTxid))}" class="converge-proof-hex">${escapeHtml(burnTxid)}</a>`
      : '<span class="muted">—</span>';
    steps.push(flowStep("OP_CONVERGE destroy", burnBody));
    steps.push(flowArrow());
    steps.push(flowStep("Same body transaction", '<span class="muted">Destroy ORU to mint RES at confirming height h</span>'));
    steps.push(flowArrow());
    const outputs = link.res_mint_outputs || [];
    let mintOutBody = '<span class="muted">—</span>';
    if (outputs.length > 0) {
      mintOutBody = outputs
        .map(
          (o) =>
            `vout[${o.vout_index}]: ${escapeHtml(o.amount_display)}${o.address ? ` → ${escapeHtml(o.address)}` : ""}`,
        )
        .join("<br/>");
    } else if (link.res_mint_linkage?.mint_amount_display) {
      mintOutBody = escapeHtml(link.res_mint_linkage.mint_amount_display);
    }
    steps.push(flowStep("RES Mint Output", mintOutBody));

    return `<div class="converge-proof-flow">${steps.join("")}</div>`;
  }

  function renderLinkage(linkage, routes) {
    const txPath = routes.txPath.bind(routes);
    const blockPath = routes.blockPath.bind(routes);
    return `<dl class="converge-proof-meta">
      <dt>mint txid</dt><dd>${linkage.mint_txid ? `<a href="${escapeHtml(txPath(linkage.mint_txid))}">${escapeHtml(linkage.mint_txid)}</a>` : "—"}</dd>
      <dt>destroy txid</dt><dd>${linkage.winner_txid ? `<a href="${escapeHtml(txPath(linkage.winner_txid))}">${escapeHtml(linkage.winner_txid)}</a>` : "—"}</dd>
      <dt>block height</dt><dd>${linkage.block_height != null ? linkage.block_height : "—"}</dd>
      <dt>confirmations (indexed tip)</dt><dd>${linkage.confirmations != null ? linkage.confirmations : "—"}</dd>
      <dt>block</dt><dd>${linkage.block_hash ? `<a href="${escapeHtml(blockPath(linkage.block_hash))}">${escapeHtml(linkage.block_hash)}</a>` : "—"}</dd>
    </dl>`;
  }

  function renderConvergeProofPanel(panel, routes) {
    if (!panel || !panel.visible) return "";

    let html = `<section class="converge-proof-panel" id="converge-proof-panel">
      <h2>ConvergeProof</h2>
      <p class="converge-proof-meta">kind: ${escapeHtml(panel.kind ?? "—")}</p>
      ${renderFlow(panel, routes)}
      <h3>Mint proof</h3>`;

    html += `<p class="muted">Issuance is the same body transaction: OP_CONVERGE destroy plus optional Witness v1 RES. ORU destroyed. RES minted.</p>`;

    html += `<h3>RES mint linkage</h3>${renderLinkage(panel.res_mint_linkage || {}, routes)}
      <div class="converge-proof-notice">
        <p>${escapeHtml(panel.protocol_notice ?? "")}</p>
        <p>${escapeHtml(panel.authority_notice ?? "")}</p>
      </div>
    </section>`;

    return html;
  }

  window.ConvergeProofPanel = { renderConvergeProofPanel };
})();
