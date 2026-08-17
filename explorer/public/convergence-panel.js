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

  function emptyNote(hex) {
    const s = String(hex || "").toLowerCase().replace(/^0x/, "");
    if (s === "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d") {
      return " · empty eligible set";
    }
    return "";
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

  function renderBlockPanel(panel, routes) {
    if (!panel) return "";
    const txPath = routes.txPath.bind(routes);
    const statusCls = statusClass(panel.status);
    const winnerHtml = panel.winner_txid
      ? `<a href="${escapeHtml(txPath(panel.winner_txid))}">${escapeHtml(panel.winner_txid)}</a>`
      : '<span class="muted">—</span>';

    const candidates = Array.isArray(panel.candidate_txids) ? panel.candidate_txids : [];
    const listItems =
      candidates.length > 0
        ? candidates
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
          <dd>|F|=${panel.candidate_set_size ?? 0}</dd>

          <dt>C (convergence_root)</dt>
          <dd>
            <div class="convergence-root-row">
              <span class="convergence-root-value">${escapeHtml(panel.c_root || panel.convergence_root || "—")}</span>
              <button type="button" class="convergence-copy-btn" data-copy="${escapeHtml(panel.c_root || panel.convergence_root || "")}">Copy</button>
            </div>
            <div class="muted">|F|=${panel.candidate_set_size ?? 0}</div>
          </dd>

          <dt>draw_candidate_root_committed</dt>
          <dd>
            <div class="convergence-root-row">
              <span class="convergence-root-value">${escapeHtml(panel.draw_candidate_root_committed || panel.draw_candidate_root || "—")}</span>
              <button type="button" class="convergence-copy-btn" data-copy="${escapeHtml(panel.draw_candidate_root_committed || panel.draw_candidate_root || "")}">Copy</button>
            </div>
            <div class="muted">pays parent draw${emptyNote(panel.draw_candidate_root_committed || panel.draw_candidate_root)}</div>
          </dd>

          <dt>draw_candidate_root_computed</dt>
          <dd>
            <div class="convergence-root-row">
              <span class="convergence-root-value">${escapeHtml(panel.draw_candidate_root_computed || "—")}</span>
              <button type="button" class="convergence-copy-btn" data-copy="${escapeHtml(panel.draw_candidate_root_computed || "")}">Copy</button>
            </div>
            <div class="muted">this block awards · eligible=${panel.eligible_count ?? 0}${emptyNote(panel.draw_candidate_root_computed)}</div>
          </dd>

          <dt>winner_txid</dt>
          <dd>${winnerHtml}</dd>

          <dt>candidate_txids</dt>
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
      if (
        panel.window_last_height != null &&
        Number(panel.window_last_height) > Number(panel.block_height) &&
        panel.role === "Candidate"
      ) {
        heightHtml += ` <span class="muted">— pending in F through height ${escapeHtml(String(panel.window_last_height))}</span>`;
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
      </section>
    `;
  }

  window.ConvergencePanel = {
    renderBlockPanel,
    renderTxPanel,
    bindCopyButtons,
  };
})();
