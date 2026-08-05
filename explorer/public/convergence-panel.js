/**
 * Convergence detail panel rendering (TASK-EXPLORER-004). Presentation only.
 */
(function () {
  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function statusClass(status) {
    if (status === "Convergence Winner Block") return "convergence-status convergence-status-winner";
    if (status === "Convergence Candidate Block") return "convergence-status convergence-status-candidate";
    return "convergence-status";
  }

  function roleClass(role) {
    if (role === "Winner") return "convergence-status convergence-status-winner";
    if (role === "Candidate") return "convergence-status convergence-status-candidate";
    return "convergence-status";
  }

  function bindCopyButtons(root) {
    root.querySelectorAll("[data-copy]").forEach((btn) => {
      btn.addEventListener("click", () => {
        const text = btn.getAttribute("data-copy") || "";
        navigator.clipboard.writeText(text).then(
          () => {
            const prev = btn.textContent;
            btn.textContent = "Copied";
            setTimeout(() => {
              btn.textContent = prev;
            }, 1500);
          },
          () => {
            btn.textContent = "Failed";
          },
        );
      });
    });
  }

  function rpcNoticeHtml(notice) {
    return `<div class="convergence-rpc-notice" role="note">${escapeHtml(notice)}</div>`;
  }

  function renderBlockPanel(panel, routes) {
    const txPath = routes.txPath.bind(routes);
    const statusCls = statusClass(panel.status);
    const winnerHtml = panel.winner_txid
      ? `<a href="${escapeHtml(txPath(panel.winner_txid))}">${escapeHtml(panel.winner_txid)}</a>`
      : '<span class="muted">—</span>';

    const listItems =
      panel.candidate_txids.length > 0
        ? panel.candidate_txids
            .map(
              (id) =>
                `<li><a href="${escapeHtml(txPath(id))}">${escapeHtml(id)}</a></li>`,
            )
            .join("")
        : '<li class="muted">No candidates indexed for this block.</li>';

    return `
      <section class="convergence-panel" id="convergence-panel">
        <h2>Convergence</h2>
        <dl>
          <dt>Convergence Status</dt>
          <dd><span class="${statusCls}">${escapeHtml(panel.status)}</span></dd>

          <dt>Candidate Set Size |F|</dt>
          <dd>Candidate Set Size: ${panel.candidate_set_size}</dd>

          <dt>convergence_root</dt>
          <dd>
            <div class="convergence-root-row">
              <span class="convergence-root-value">${escapeHtml(panel.convergence_root)}</span>
              <button type="button" class="convergence-copy-btn" data-copy="${escapeHtml(panel.convergence_root)}">Copy</button>
            </div>
          </dd>

          <dt>winner_txid</dt>
          <dd>${winnerHtml}</dd>

          <dt>candidate_txids</dt>
          <dd>
            <ul class="convergence-candidate-list">${listItems}</ul>
          </dd>
        </dl>
        ${rpcNoticeHtml(panel.rpc_authority_notice)}
      </section>
    `;
  }

  function renderTxPanel(panel, routes) {
    const blockPath = routes.blockPath.bind(routes);
    const statusCls = roleClass(panel.role);

    let heightHtml = '<span class="muted">—</span>';
    if (panel.block_height != null) {
      heightHtml = String(panel.block_height);
      if (panel.block_hash) {
        heightHtml += ` (<a href="${escapeHtml(blockPath(panel.block_hash))}">block</a>)`;
      }
    }

    let winnerRefHtml = '<span class="muted">—</span>';
    if (panel.winner_reference) {
      const wr = panel.winner_reference;
      winnerRefHtml = `Winner at height ${wr.block_height} — <a href="${escapeHtml(blockPath(wr.block_hash))}">${escapeHtml(wr.block_hash)}</a>`;
    }

    return `
      <section class="convergence-panel" id="convergence-panel">
        <h2>Convergence</h2>
        <dl>
          <dt>Convergence Role</dt>
          <dd><span class="${statusCls}">${escapeHtml(panel.role)}</span></dd>

          <dt>Convergence Block Height</dt>
          <dd>${heightHtml}</dd>

          <dt>Winner Reference</dt>
          <dd>${winnerRefHtml}</dd>
        </dl>
        ${rpcNoticeHtml(panel.rpc_authority_notice)}
      </section>
    `;
  }

  window.ConvergencePanel = {
    renderBlockPanel,
    renderTxPanel,
    bindCopyButtons,
  };
})();
