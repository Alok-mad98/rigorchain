/**
 * Convergence detail panel rendering. Presentation only.
 */
(function () {
  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function emptyNote(hex) {
    const s = String(hex || "").toLowerCase().replace(/^0x/, "");
    if (s === "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d") {
      return " · empty eligible set";
    }
    return "";
  }

  function displayBlockStatus(status) {
    if (status === "Convergence Winner Block") return "RES Mint Block";
    if (status === "Convergence Candidate Block") return "Convergence Destroy Block";
    return status || "";
  }

  function displayTxRole(role) {
    if (role === "Winner") return "Mint";
    if (role === "Candidate") return "Destroy";
    return role || "";
  }

  function statusClass(status) {
    if (status === "Convergence Winner Block" || status === "RES Mint Block") {
      return "convergence-status convergence-status-winner";
    }
    if (status === "Convergence Candidate Block" || status === "Convergence Destroy Block" || status === "Convergence Burn Block") {
      return "convergence-status convergence-status-candidate";
    }
    return "convergence-status";
  }

  function roleClass(role) {
    if (role === "Winner" || role === "Mint") return "convergence-status convergence-status-winner";
    if (role === "Candidate" || role === "Destroy" || role === "Burn") return "convergence-status convergence-status-candidate";
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

  function renderBlockPanel(panel, routes) {
    if (!panel) return "";
    const txPath = routes.txPath.bind(routes);
    const statusCls = statusClass(panel.status);

    const candidates = Array.isArray(panel.candidate_txids) ? panel.candidate_txids : [];
    const listItems =
      candidates.length > 0
        ? candidates
            .map(
              (id) =>
                `<li><a href="${escapeHtml(txPath(id))}">${escapeHtml(id)}</a></li>`,
            )
            .join("")
        : '<li class="muted">No OP_CONVERGE destroys indexed for this block.</li>';

    return `
      <section class="convergence-panel" id="convergence-panel">
        <h2>Convergence</h2>
        <dl>
          <dt>Convergence Status</dt>
          <dd><span class="${statusCls}">${escapeHtml(displayBlockStatus(panel.status))}</span></dd>

          <dt>Convergence destroys</dt>
          <dd>${panel.candidate_set_size ?? 0}</dd>

          <dt>C (convergence_root)</dt>
          <dd>
            <div class="convergence-root-row">
              <span class="convergence-root-value">${escapeHtml(panel.c_root || panel.convergence_root || "—")}</span>
              <button type="button" class="convergence-copy-btn" data-copy="${escapeHtml(panel.c_root || panel.convergence_root || "")}">Copy</button>
            </div>
          </dd>

          <dt>convergence_txids</dt>
          <dd>
            <ul class="convergence-candidate-list">${listItems}</ul>
          </dd>
        </dl>
      </section>
    `;
  }

  function renderTxPanel(panel, routes) {
    if (!panel) return "";
    const blockPath = routes.blockPath.bind(routes);
    const statusCls = roleClass(panel.role);

    let heightHtml = '<span class="muted">—</span>';
    if (panel.block_height != null) {
      heightHtml = String(panel.block_height);
      if (panel.block_hash) {
        heightHtml += ` (<a href="${escapeHtml(blockPath(panel.block_hash))}">block</a>)`;
      }
    }

    return `
      <section class="convergence-panel" id="convergence-panel">
        <h2>Convergence</h2>
        <dl>
          <dt>Convergence Role</dt>
          <dd><span class="${statusCls}">${escapeHtml(displayTxRole(panel.role))}</span></dd>

          <dt>Confirming height</dt>
          <dd>${heightHtml}</dd>
        </dl>
      </section>
    `;
  }

  window.ConvergencePanel = {
    renderBlockPanel,
    renderTxPanel,
    bindCopyButtons,
  };
})();
