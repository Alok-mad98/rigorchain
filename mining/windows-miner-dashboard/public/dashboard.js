(function () {
  function esc(value) {
    return String(value == null ? "—" : value)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function tone(status) {
    const s = String(status || "").toUpperCase();
    if (["CONNECTED", "ONLINE", "ACTIVE", "WINNER", "READY", "VISIBLE", "YES", "MINING", "LIVE"].includes(s)) {
      return "ok";
    }
    if (["WAITING", "MATURING", "OFFLINE", "NO"].includes(s)) return "warn";
    if (["DISCONNECTED"].includes(s)) return "bad";
    return "off";
  }

  function kv(rows) {
    return rows
      .map(function (row) {
        return "<dt>" + esc(row[0]) + "</dt><dd>" + row[1] + "</dd>";
      })
      .join("");
  }

  function payoutAddress() {
    try {
      return localStorage.getItem("rigor.miner.payoutAddress") || "";
    } catch (err) {
      return "";
    }
  }

  function setPayoutAddress(value) {
    try {
      if (value) localStorage.setItem("rigor.miner.payoutAddress", value);
      else localStorage.removeItem("rigor.miner.payoutAddress");
    } catch (err) {}
  }

  function withAddress(url) {
    var addr = payoutAddress();
    if (!addr) return url;
    return url + (url.indexOf("?") >= 0 ? "&" : "?") + "address=" + encodeURIComponent(addr);
  }

  function fetchJson(url) {
    return fetch(withAddress(url), { cache: "no-store" }).then(function (res) {
      if (!res.ok) throw new Error(url + " " + res.status);
      return res.json();
    });
  }

  function renderLifecycle(items) {
    var root = document.getElementById("lifecycle");
    root.innerHTML = (items || [])
      .map(function (step) {
        return (
          "<li>" +
          '<span class="label">' +
          esc(step.label) +
          "</span>" +
          '<span class="state ' +
          tone(step.status) +
          '">' +
          esc(step.status) +
          "</span>" +
          '<span class="detail">' +
          esc(step.detail) +
          "</span>" +
          "</li>"
        );
      })
      .join("");
  }

  function renderSources(sources) {
    var rows = [
      ["Wallet", sources && sources.wallet],
      ["rigord :8761", sources && sources.node],
      ["Explorer :18081", sources && sources.explorer],
      ["rigor-miner", sources && sources.miner],
      ["Pool observer", sources && sources.pool_status],
    ];
    document.getElementById("source-list").innerHTML = rows
      .map(function (row) {
        var ok = !!row[1];
        return (
          "<li><span class=\"" +
          (ok ? "ok" : "warn") +
          '">' +
          (ok ? "LIVE" : "OFF") +
          "</span> " +
          esc(row[0]) +
          "</li>"
        );
      })
      .join("");
  }

  function renderMetrics(m) {
    m = m || {};
    document.getElementById("m-hashrate").textContent = m.hashrate || "—";
    document.getElementById("m-height").textContent = m.height || "—";
    document.getElementById("m-trusted").textContent = m.trusted_oru || "—";
    var conv = document.getElementById("m-convergence");
    conv.textContent = m.convergence || "—";
    conv.className = "metric-v " + tone(m.convergence);
  }

  function renderOverview(miner, snap) {
    document.getElementById("overview-kv").innerHTML = kv([
      ["Wallet", '<span class="mono">' + esc(miner.wallet) + "</span>"],
      ["Mining path", esc(miner.mining_path || "SOLO GBT")],
      ["Algorithm", esc(miner.algorithm || snap.algorithm)],
      ["Mining Status", '<span class="' + tone(miner.mining_status) + '">' + esc(miner.mining_status) + "</span>"],
      ["Miner", '<span class="' + tone(miner.miner) + '">' + esc(miner.miner) + "</span>"],
      ["GBT height", esc(miner.gbt_height)],
      ["Last submitblock", esc(miner.last_submit)],
      ["Local node", '<span class="' + tone(miner.node) + '">' + esc(miner.node) + " · " + esc(miner.rpc) + "</span>"],
      ["Platform", esc(miner.platform)],
      ["Current Height", esc(miner.current_height)],
      ["Hashrate", esc(miner.hashrate)],
      ["Mining Time", esc(miner.mining_time)],
    ]);
    document.getElementById("platform-badge").textContent = miner.platform || "Windows";
    if (snap.product) document.getElementById("product-badge").textContent = snap.product.replace(/^RIGOR\s+/i, "");
    if (miner.mining_path) document.getElementById("path-badge").textContent = miner.mining_path;
  }

  function renderWorkers(workers) {
    var body = document.getElementById("worker-body");
    if (!workers || !workers.length) {
      body.innerHTML = '<tr><td colspan="10">No miner instance yet. Start installer/windows/rigor-miner.cmd.</td></tr>';
      return;
    }
    body.innerHTML = workers
      .map(function (w) {
        return (
          "<tr>" +
          "<td>" +
          esc(w.worker) +
          "</td>" +
          '<td class="' +
          tone(w.status) +
          '">' +
          esc(w.status) +
          "</td>" +
          "<td>" +
          esc(w.os) +
          "</td>" +
          "<td>" +
          esc(w.path) +
          "</td>" +
          "<td>" +
          esc(w.hashrate) +
          "</td>" +
          "<td>" +
          esc(w.shares_submitted) +
          "</td>" +
          "<td>" +
          esc(w.accepted) +
          "</td>" +
          "<td>" +
          esc(w.rejected) +
          "</td>" +
          "<td>" +
          esc(w.submitted_blocks) +
          "</td>" +
          "<td>" +
          esc(w.last_share) +
          "</td>" +
          "</tr>"
        );
      })
      .join("");
  }

  function renderWallet(w) {
    document.getElementById("wallet-kv").innerHTML = kv([
      ["Address", esc(w.address)],
      ["Immature ORU", esc(w.immature_oru)],
      ["Trusted ORU", esc(w.trusted_oru)],
      ["Spendable", '<span class="' + tone(w.spendable) + '">' + esc(w.spendable) + "</span>"],
      ["Coinbase maturity", esc(w.coinbase_maturity_display || 100) + " blocks"],
      ["Source", esc(w.source)],
    ]);
  }

  function renderConvergence(c) {
    document.getElementById("conv-status").textContent = c.status || "WAITING";
    document.getElementById("conv-status").className = tone(c.status);
    var mc = c.mining_contribution || c;
    document.getElementById("conv-steps").innerHTML = [
      ["Pending", mc.pending],
      ["Eligible", mc.eligible],
      ["Selected", mc.selected],
      ["Settled", mc.settled],
    ]
      .map(function (row) {
        return '<li><span class="k">' + esc(row[0]) + '</span><span class="v">' + esc(row[1]) + "</span></li>";
      })
      .join("");
  }

  function renderPool(p) {
    var panel = document.getElementById("pool");
    if (!p || !p.visible) {
      panel.hidden = true;
      return;
    }
    panel.hidden = false;
    document.getElementById("pool-kv").innerHTML = kv([
      ["Connected Miners", esc(p.connected_miners)],
      ["Accepted Shares", esc(p.accepted_shares)],
      ["Rejected Shares", esc(p.rejected_shares)],
      ["Block Candidates", esc(p.block_candidates)],
      ["Submitted Blocks", esc(p.submitted_blocks)],
      ["Stratum", esc(p.stratum)],
    ]);
  }

  function renderAll(snap) {
    var miner = snap.miner || snap;
    renderLifecycle(snap.lifecycle || miner.lifecycle);
    renderOverview(miner, snap);
    renderWorkers((snap.workers && snap.workers.workers) || snap.workers || []);
    renderWallet(snap.wallet || {});
    renderConvergence(snap.convergence || {});
    renderPool(snap.pool || {});
    renderSources(snap.sources || miner.sources || {});
    renderMetrics(snap.metrics || miner.metrics || {});
    var hint = document.getElementById("address-hint");
    if (snap.address_error) {
      hint.textContent = snap.address_error;
      hint.className = "note bad";
    } else if (snap.payout_address) {
      hint.textContent = "Watching " + snap.payout_address + ". This dashboard does not mine or sign.";
      hint.className = "note";
    } else {
      hint.innerHTML =
        "Paste your miner <code>--coinbase-addr</code>. Not a private key. RES <code>rg1p…</code> addresses are rejected.";
      hint.className = "note";
    }
    var input = document.getElementById("payout-address");
    if (snap.payout_address && input && !input.value) input.value = snap.payout_address;
    document.getElementById("generated-at").textContent = snap.generated_at || miner.generated_at || "";
  }

  function load() {
    return fetchJson("/api/status").then(renderAll);
  }

  function showError(err) {
    document.getElementById("overview-kv").innerHTML =
      "<dt>Dashboard</dt><dd class=\"bad\">" + esc(err.message || err) + "</dd>";
  }

  load().catch(showError);
  if (window.EventSource) {
    var es = new EventSource(withAddress("/api/stream"));
    es.onmessage = function (ev) {
      try {
        renderAll(JSON.parse(ev.data));
      } catch (err) {}
    };
  } else {
    setInterval(function () {
      load().catch(function () {});
    }, 5000);
  }

  var form = document.getElementById("address-form");
  var input = document.getElementById("payout-address");
  var stored = payoutAddress();
  if (stored && input) input.value = stored;
  if (form) {
    form.addEventListener("submit", function (ev) {
      ev.preventDefault();
      setPayoutAddress((input && input.value.trim()) || "");
      window.location.reload();
    });
  }
  var clearBtn = document.getElementById("address-clear");
  if (clearBtn) {
    clearBtn.addEventListener("click", function () {
      setPayoutAddress("");
      if (input) input.value = "";
      window.location.reload();
    });
  }
})();
