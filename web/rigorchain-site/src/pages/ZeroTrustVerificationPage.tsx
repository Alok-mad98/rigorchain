import { FormEvent, useEffect, useMemo, useState } from "react";
import { Link, useSearchParams } from "react-router-dom";
import { Button } from "../components/Button";
import { DataPanel } from "../components/DataPanel";
import { InputBox } from "../components/InputBox";
import { SubNav } from "../components/SubNav";
import { DRAW_FINGERPRINT_LABEL } from "../lib/zeroTrust/constants";
import { runSelfCheck, type SelfCheckRow } from "../lib/zeroTrust/selfCheck";
import { loadPublicBlock, loadPublicTransaction } from "../lib/zeroTrust/publicData";
import { parseRawBlockJson, verifyBlock, verifyTransaction } from "../lib/zeroTrust/verify";
import type { BlockVerificationResult, CheckStatus, FinalStatus, TxVerificationResult } from "../lib/zeroTrust/types";

type Mode = "tx" | "block" | "raw";

function statusClass(s: CheckStatus | FinalStatus): string {
  if (s === "VALID" || s === "MATCH") return "zt-ok";
  if (s === "INVALID" || s === "MISMATCH") return "zt-bad";
  return "zt-mid";
}

export function ZeroTrustVerificationPage() {
  const [params, setParams] = useSearchParams();
  const mode = (params.get("mode") as Mode) || "tx";
  const [txid, setTxid] = useState(params.get("txid") ?? "");
  const [blockHash, setBlockHash] = useState(params.get("hash") ?? "");
  const [rawJson, setRawJson] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [txResult, setTxResult] = useState<TxVerificationResult | null>(null);
  const [blockResult, setBlockResult] = useState<BlockVerificationResult | null>(null);
  const [selfCheck, setSelfCheck] = useState<SelfCheckRow[] | null>(null);

  const activeMode: Mode = mode === "block" || mode === "raw" ? mode : "tx";

  useEffect(() => {
    void runSelfCheck().then(setSelfCheck);
  }, []);

  useEffect(() => {
    if (activeMode === "tx" && params.get("txid") && !txResult && !busy) {
      void onVerify();
    }
    if (activeMode === "block" && params.get("hash") && !blockResult && !busy) {
      void onVerify();
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const selfOk = useMemo(() => selfCheck?.every((r) => r.ok) ?? false, [selfCheck]);

  function setMode(next: Mode) {
    const nextParams = new URLSearchParams(params);
    nextParams.set("mode", next);
    setParams(nextParams, { replace: true });
    setError(null);
  }

  async function onVerify(e?: FormEvent) {
    e?.preventDefault();
    setBusy(true);
    setError(null);
    setTxResult(null);
    setBlockResult(null);
    try {
      if (activeMode === "tx") {
        const id = txid.trim();
        if (!/^[0-9a-fA-F]{64}$/.test(id)) throw new Error("Enter a 64-character TXID.");
        const tx = await loadPublicTransaction(id);
        const block = tx.blockHashDisplay ? await loadPublicBlock(tx.blockHashDisplay) : null;
        setTxResult(await verifyTransaction(tx, block));
      } else if (activeMode === "block") {
        const id = blockHash.trim();
        if (!/^[0-9a-fA-F]{64}$/.test(id)) throw new Error("Enter a 64-character block hash.");
        const block = await loadPublicBlock(id);
        setBlockResult(await verifyBlock(block));
      } else {
        const block = parseRawBlockJson(rawJson);
        setBlockResult(await verifyBlock(block));
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setBusy(false);
    }
  }

  const resultLabel = txResult?.final ?? blockResult?.final ?? (busy ? "COMPUTING" : "—");
  const resultTone =
    resultLabel === "VALID"
      ? "zt-ok"
      : resultLabel === "INVALID"
        ? "zt-bad"
        : "zt-mid";

  return (
    <div className="page zt-page">
      <SubNav />
      <header className="page-header">
        <p className="tag">TRUST LAYER</p>
        <h1>Zero-Trust Verification</h1>
        <p className="muted">Anyone can verify without trusting a server.</p>
        <p className="muted">
          You do not need to trust a node. This browser recomputes consensus outputs locally.{" "}
          <Link to="/zero-trust-verification/what-is-zero-trust">What is Zero Trust?</Link>
        </p>
      </header>

      <section className="card zt-trust-card">
        <h2>What this portal does not trust</h2>
        <ul>
          <li>RIGOR nodes, explorer servers, wallet software, backend APIs, or cached verdicts</li>
          <li>Explorer-calculated {DRAW_FINGERPRINT_LABEL} or “valid” flags</li>
        </ul>
        <p className="muted">
          Public identifiers may be fetched as <em>inputs</em>. Genesis hash, parent hash
          (<code>hashPrevBlock</code>), ConvergenceRoot, Sigma, and winner selection are computed
          locally with WebCrypto over <code>Uint8Array</code>. {DRAW_FINGERPRINT_LABEL} is an
          Explorer observational short ID for one RES draw — not a verdict and not used here.
          This portal is independent validation of public bytes. It is not a Coq⇄C++ refinement
          proof. See <Link to="/documentation/formal-verification">Formal Verification</Link>.
        </p>
      </section>

      <div className="zt-mode-tabs" role="tablist">
        <button type="button" className={activeMode === "tx" ? "active" : ""} onClick={() => setMode("tx")}>
          Transaction
        </button>
        <button type="button" className={activeMode === "block" ? "active" : ""} onClick={() => setMode("block")}>
          Block
        </button>
        <button type="button" className={activeMode === "raw" ? "active" : ""} onClick={() => setMode("raw")}>
          Advanced raw JSON
        </button>
      </div>

      <DataPanel title="INPUT">
        <form className="zt-form" onSubmit={(e) => void onVerify(e)}>
          {activeMode === "tx" && (
            <>
              <InputBox
                id="zt-txid"
                label="TXID"
                value={txid}
                onChange={(e) => setTxid(e.target.value)}
                placeholder="64-character transaction id"
                spellCheck={false}
                autoComplete="off"
              />
              <p className="muted">
                Source: Explorer, or wallet <code>gettransaction</code> / <code>sendtoaddress</code>.
              </p>
            </>
          )}
          {activeMode === "block" && (
            <>
              <InputBox
                id="zt-hash"
                label="Block hash"
                value={blockHash}
                onChange={(e) => setBlockHash(e.target.value)}
                placeholder="64-character block hash"
                spellCheck={false}
                autoComplete="off"
              />
              <p className="muted">
                Source: Explorer, or <code>getblockhash &lt;height&gt;</code>.
              </p>
            </>
          )}
          {activeMode === "raw" && (
            <>
              <InputBox
                id="zt-raw"
                label="Raw block JSON"
                multiline
                value={rawJson}
                onChange={(e) => setRawJson(e.target.value)}
                placeholder='{"hash":"...","height":103,"previousblockhash":"...","tx":[...]}'
                spellCheck={false}
              />
              <p className="muted">
                Auditor mode. Source: <code>getblock &lt;hash&gt; 2</code>.
              </p>
            </>
          )}
          <Button type="submit" className="zt-verify-btn" disabled={busy}>
            {busy ? "Computing locally…" : "VERIFY"}
          </Button>
        </form>
      </DataPanel>

      <div className="zt-result-box" role="status">
        <p className="zt-result-label">RESULT</p>
        <p className={`zt-result-value ${resultTone}`}>{resultLabel === "VALID" ? "VALID" : resultLabel}</p>
      </div>

      {error && (
        <div className="banner banner-error" role="alert">
          {error}
        </div>
      )}

      {txResult && <TxResultCard result={txResult} />}
      {blockResult && <BlockResultCard result={blockResult} />}

      <section className="card">
        <h2>Local primitive self-check</h2>
        <p className="muted">
          WebCrypto SHA256d and binary pipeline, run in this tab.{" "}
          {selfOk ? "All primitives matched." : "Checking…"}
        </p>
        <ul className="zt-selfcheck">
          {(selfCheck ?? []).map((row) => (
            <li key={row.name}>
              <span className={row.ok ? "zt-ok" : "zt-bad"}>{row.ok ? "PASS" : "FAIL"}</span> {row.name}
              <span className="muted"> — {row.detail}</span>
            </li>
          ))}
        </ul>
      </section>
    </div>
  );
}

function TxResultCard({ result }: { result: TxVerificationResult }) {
  return (
    <section className="card zt-result">
      <h2>Transaction Verification</h2>
      <dl className="kv-grid zt-kv">
        <dt>Transaction</dt>
        <dd className={statusClass(result.transaction)}>{result.transaction}</dd>
        <dt>Block</dt>
        <dd className="mono">{result.blockHeight ?? "—"}</dd>
        <dt>ORU</dt>
        <dd className="mono">{result.oru}</dd>
        <dt>Fee</dt>
        <dd className="mono">{result.fee}</dd>
        <dt>RES</dt>
        <dd className="mono">{result.res}</dd>
        <dt>FINAL RESULT</dt>
        <dd className={statusClass(result.final)}>{result.final}</dd>
      </dl>
      <TraceAndNotes result={result} />
    </section>
  );
}

function BlockResultCard({ result }: { result: BlockVerificationResult }) {
  return (
    <section className="card zt-result">
      <h2>Block Verification</h2>
      <dl className="kv-grid zt-kv">
        <dt>Height</dt>
        <dd className="mono">{result.height ?? "—"}</dd>
        <dt>Genesis</dt>
        <dd className={statusClass(result.genesis)}>{result.genesis}</dd>
        <dt>Parent hash</dt>
        <dd className={statusClass(result.parentHash)}>{result.parentHash}</dd>
        <dt>ConvergenceRoot</dt>
        <dd className={statusClass(result.convergenceRoot)}>{result.convergenceRoot}</dd>
        <dt>Winner</dt>
        <dd className={statusClass(result.winner)}>{result.winner}</dd>
        <dt>FINAL</dt>
        <dd className={statusClass(result.final)}>{result.final === "VALID" ? "VALID BLOCK" : result.final}</dd>
      </dl>
      <TraceAndNotes result={result} />
    </section>
  );
}

function TraceAndNotes({ result }: { result: TxVerificationResult | BlockVerificationResult }) {
  return (
    <>
      {result.trace && (
        <div className="zt-trace">
          <h3>Browser-computed pipeline</h3>
          <p className="mono truncate-wrap">ConvergenceRoot: {result.trace.convergenceRootDisplay}</p>
          {result.trace.previousHashDisplay && (
            <p className="mono truncate-wrap">Parent hash: {result.trace.previousHashDisplay}</p>
          )}
          {result.trace.winnerTxidDisplay && (
            <p className="mono truncate-wrap">Winner: {result.trace.winnerTxidDisplay}</p>
          )}
        </div>
      )}
      <p className="source-label">Public data source (input only): {result.publicSource}</p>
      {result.notes.length > 0 && (
        <ul className="zt-notes">
          {result.notes.map((n) => (
            <li key={n}>{n}</li>
          ))}
        </ul>
      )}
    </>
  );
}
