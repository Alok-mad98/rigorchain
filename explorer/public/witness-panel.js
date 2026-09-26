/**
 * Witness and RES creation panel. Presentation only.
 */
(function () {
  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function pathClass(kind) {
    if (kind === "oru_input") return "path-oru";
    if (kind === "res_key_path") return "path-res-key";
    if (kind === "coinbase") return "path-coinbase";
    return "";
  }

  function copyBtn(hex) {
    return `<button type="button" class="convergence-copy-btn" data-copy="${escapeHtml(hex)}">Copy</button>`;
  }

  function renderVinInput(vin, routes) {
    const cls = pathClass(vin.path_kind);
    let body = `<p class="witness-meta muted">Stack depth: ${vin.stack_depth}</p>`;

    if (vin.witness_stack && vin.witness_stack.length > 0) {
      body += `<div class="witness-hex">${vin.witness_stack.map((h, i) => `[${i}] ${escapeHtml(h)}`).join("<br/>")}</div>`;
      body += `<p>${copyBtn(vin.witness_hex)}</p>`;
      if (vin.path_kind === "res_key_path") {
        body += `<p class="witness-meta">RES key-path spend (2-item stack)</p>`;
      }
    } else {
      body += `<p class="muted">No witness data indexed.</p>`;
    }

    return `<article class="witness-vin-card ${cls}">
      <h3>vin[${vin.vin_index}] — ${escapeHtml(vin.path_label)}</h3>
      ${body}
    </article>`;
  }

  function renderResMintProof(proof, routes) {
    if (!proof.present && proof.confirmation_status === "Not a RES mint") {
      return `<h3>RES mint proof</h3><p class="muted">No RES mint proof indexed for this transaction.</p>`;
    }

    const blockPath = routes.blockPath.bind(routes);
    const txPath = routes.txPath.bind(routes);

    const statusCls =
      proof.confirmation_status === "Confirmed in block" ? "witness-status-confirmed" : "witness-status-pending";

    let html = `<h3>RES mint proof</h3>
      <dl class="witness-meta">
        <dt>Confirmation</dt>
        <dd><span class="${statusCls}">${escapeHtml(proof.confirmation_status)}</span>
          ${proof.block_height != null ? ` at height ${proof.block_height}` : ""}</dd>
        <dt>mint txid</dt>
        <dd>${proof.mint_txid ? `<a href="${escapeHtml(txPath(proof.mint_txid))}">${escapeHtml(proof.mint_txid)}</a>` : "—"}</dd>
        <dt>mint vout</dt>
        <dd>${proof.mint_vout_index != null ? `vout[${proof.mint_vout_index}]` : "—"}
          ${proof.mint_amount_display ? ` — ${escapeHtml(proof.mint_amount_display)}` : ""}</dd>
        <dt>OP_CONVERGE txid</dt>
        <dd>${proof.winner_convergence_txid ? `<a href="${escapeHtml(txPath(proof.winner_convergence_txid))}">${escapeHtml(proof.winner_convergence_txid)}</a>` : "—"}</dd>
        <dt>same confirming block</dt>
        <dd>${proof.matches_winner_block ? "yes" : "no"}</dd>
      </dl>`;

    if (proof.block_hash) {
      html += `<p class="witness-meta">Block: <a href="${escapeHtml(blockPath(proof.block_hash))}">${escapeHtml(proof.block_hash)}</a></p>`;
    }

    if (proof.present) {
      html += `<p class="muted">Issuance is the same body transaction: <code>OP_CONVERGE</code> destroy plus Witness v1 RES. ORU destroyed. RES minted.</p>`;
    }

    return html;
  }

  function renderProofStatus(status) {
    return `<h3>Convergence &amp; proof status</h3>
      <dl class="witness-meta">
        <dt>Candidate set height</dt>
        <dd>${status.candidate_set_height != null ? status.candidate_set_height : "—"}</dd>
        <dt>Issuance status</dt>
        <dd>${escapeHtml(status.winner_status === "Winner" ? "Mint" : status.winner_status === "Candidate" ? "Destroy" : status.winner_status)}</dd>
      </dl>`;
  }

  function renderWitnessPanel(panel, routes) {
    const oruInputs = panel.inputs.filter((i) => i.path_kind === "oru_input" || i.path_kind === "coinbase" || i.path_kind === "other");
    const resInputs = panel.inputs.filter((i) => i.path_kind === "res_key_path");

    let inputsHtml = "";
    if (resInputs.length > 0) {
      inputsHtml += `<h3>RES mint / RES spend paths</h3>${resInputs.map((v) => renderVinInput(v, routes)).join("")}`;
    }
    if (oruInputs.length > 0) {
      inputsHtml += `<h3>ORU input witnesses</h3>${oruInputs.map((v) => renderVinInput(v, routes)).join("")}`;
    }
    if (panel.inputs.length === 0) {
      inputsHtml = `<p class="muted">No inputs indexed.</p>`;
    }

    return `<section class="witness-panel" id="witness-panel">
      <h2>Witness &amp; RES mint proof</h2>
      <div class="witness-inputs-scroll">${inputsHtml}</div>
      ${renderResMintProof(panel.res_mint_proof, routes)}
      ${renderProofStatus(panel.proof_status)}
    </section>`;
  }

  window.WitnessPanel = {
    renderWitnessPanel,
  };
})();
