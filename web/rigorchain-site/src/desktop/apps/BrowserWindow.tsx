import { Fragment, useCallback, useEffect, useState } from "react";
import { useSystemState } from "../../context/SystemStateContext";
import * as explorer from "../../lib/explorerClient";
import {
  displayBlockConvergeBurn,
  displayBlockHash,
  displayBlockHeight,
  displayBlockOruMinted,
  explorerBestHash,
  explorerTipHeight,
} from "../../lib/explorerDisplay";
import { EXPLORER_HOME, EXPLORER_HOST, EXPLORER_ORIGIN, GITHUB_RELEASES, GITHUB_REPO, NETWORK_NAME } from "../data/links";
import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

type PageKind = "home" | "blocks" | "block" | "tx" | "status" | "github" | "unknown";

function parseUrl(raw: string): { href: string; kind: PageKind; id?: string } {
  const trimmed = raw.trim();
  const withScheme = /^https?:\/\//i.test(trimmed) ? trimmed : `http://${trimmed}`;
  let url: URL;
  try {
    url = new URL(withScheme);
  } catch {
    return { href: raw, kind: "unknown" };
  }
  const host = url.hostname.replace(/^www\./, "");
  const path = url.pathname.replace(/\/+$/, "") || "/";

  if (host === EXPLORER_HOST || host === "explorer.rigorchain.com") {
    if (path === "/" || path === "") return { href: EXPLORER_HOME, kind: "home" };
    if (path === "/blocks") return { href: `${EXPLORER_ORIGIN}/blocks`, kind: "blocks" };
    if (path === "/status") return { href: `${EXPLORER_ORIGIN}/status`, kind: "status" };
    const block = path.match(/^\/block\/([0-9a-fA-F]+)$/);
    if (block) {
      return { href: `${EXPLORER_ORIGIN}/block/${block[1]}`, kind: "block", id: block[1] };
    }
    const tx = path.match(/^\/tx\/([0-9a-fA-F]+)$/);
    if (tx) return { href: `${EXPLORER_ORIGIN}/tx/${tx[1]}`, kind: "tx", id: tx[1] };
    return { href: `${EXPLORER_ORIGIN}${path}`, kind: "home" };
  }

  if (host === "github.com" && path.startsWith("/rigorchain/rigorchain")) {
    return { href: url.href, kind: "github" };
  }

  if (/^[0-9a-fA-F]{64}$/.test(trimmed)) {
    return { href: `${EXPLORER_ORIGIN}/block/${trimmed}`, kind: "block", id: trimmed };
  }

  return { href: url.href, kind: "unknown" };
}

export function BrowserWindow({ win }: { win: WindowRecord }) {
  const { closeWindow } = useDesktop();
  const { state, refresh } = useSystemState();
  const [draft, setDraft] = useState(EXPLORER_HOME);
  const [stack, setStack] = useState<string[]>([EXPLORER_HOME]);
  const [index, setIndex] = useState(0);
  const [pageError, setPageError] = useState<string | null>(null);
  const [blockDetail, setBlockDetail] = useState<Record<string, unknown> | null>(null);
  const [txDetail, setTxDetail] = useState<Record<string, unknown> | null>(null);
  const [loading, setLoading] = useState(false);

  const current = stack[index] ?? EXPLORER_HOME;
  const parsed = parseUrl(current);

  const navigate = (href: string, push = true) => {
    const next = parseUrl(href).href;
    setDraft(next);
    if (push) {
      const nextStack = stack.slice(0, index + 1);
      nextStack.push(next);
      setStack(nextStack);
      setIndex(nextStack.length - 1);
    }
  };

  const loadDetail = useCallback(async (p: ReturnType<typeof parseUrl>) => {
    setPageError(null);
    setBlockDetail(null);
    setTxDetail(null);
    if (p.kind === "block" && p.id) {
      setLoading(true);
      const res = await explorer.getBlock(p.id);
      setLoading(false);
      if (res.error || !res.body) {
        setPageError(res.error ?? "Block not found");
        return;
      }
      setBlockDetail(res.body);
      return;
    }
    if (p.kind === "tx" && p.id) {
      setLoading(true);
      const res = await explorer.getTx(p.id);
      setLoading(false);
      if (res.error || !res.body) {
        setPageError(res.error ?? "Transaction not found");
        return;
      }
      setTxDetail(res.body);
    }
  }, []);

  useEffect(() => {
    void loadDetail(parseUrl(current));
  }, [current, loadDetail]);

  const live = state.explorer_status.reachable || state.rpc_status.reachable;
  const height = live ? state.chain_height ?? explorerTipHeight(state.explorer_snapshot) : null;
  const hash = live ? state.best_block_hash ?? explorerBestHash(state.explorer_snapshot) : null;
  const peers = live ? state.network_info?.connections : null;
  const blocks = live ? state.recent_blocks ?? [] : [];
  const mining =
    live && state.explorer_snapshot?.mining_activity && typeof state.explorer_snapshot.mining_activity === "object"
      ? (state.explorer_snapshot.mining_activity as Record<string, unknown>)
      : null;

  return (
    <Window
      win={{ ...win, title: "Rigor Browser" }}
      menu={
        <MenuBar
          menus={[
            {
              label: "File",
              items: [
                { label: "Home", action: () => navigate(EXPLORER_HOME) },
                { label: "Close", action: () => closeWindow(win.id) },
              ],
            },
            { label: "Edit", items: [{ label: "Copy URL", action: () => void navigator.clipboard?.writeText(current) }] },
            {
              label: "View",
              items: [{ label: "Refresh", action: () => void refresh() }],
            },
            {
              label: "Go",
              items: [
                { label: "Explorer Home", action: () => navigate(EXPLORER_HOME) },
                { label: "Blocks", action: () => navigate(`${EXPLORER_ORIGIN}/blocks`) },
                { label: "Status", action: () => navigate(`${EXPLORER_ORIGIN}/status`) },
                { label: "GitHub", action: () => navigate(GITHUB_REPO) },
              ],
            },
            {
              label: "Help",
              items: [{ label: "About Rigor Browser" }],
            },
          ]}
        />
      }
      toolbar={
        <>
          <div className="win95-window-toolbar">
            <button
              type="button"
              className="win95-btn win95-btn-sm"
              disabled={index <= 0}
              onClick={() => {
                const i = index - 1;
                setIndex(i);
                setDraft(stack[i]);
              }}
            >
              Back
            </button>
            <button
              type="button"
              className="win95-btn win95-btn-sm"
              disabled={index >= stack.length - 1}
              onClick={() => {
                const i = index + 1;
                setIndex(i);
                setDraft(stack[i]);
              }}
            >
              Forward
            </button>
            <button
              type="button"
              className="win95-btn win95-btn-sm"
              onClick={() => {
                void refresh();
                void loadDetail(parsed);
              }}
            >
              Refresh
            </button>
            <button type="button" className="win95-btn win95-btn-sm" onClick={() => navigate(EXPLORER_HOME)}>
              Home
            </button>
          </div>
          <form
            className="browser-addr"
            onSubmit={(e) => {
              e.preventDefault();
              navigate(draft);
            }}
          >
            <span className="browser-addr-label">Address</span>
            <input
              className="win95-field"
              value={draft}
              onChange={(e) => setDraft(e.target.value)}
              spellCheck={false}
            />
            <button type="submit" className="win95-btn win95-btn-sm">
              Go
            </button>
          </form>
        </>
      }
      status={
        live
          ? `Document: Done  |  ${EXPLORER_HOST}  |  ${NETWORK_NAME}`
          : `CONNECTION FAILED  |  ${state.explorer_status.last_error ?? "NETWORK OFFLINE"}`
      }
    >
      <div className="browser-page">
        {parsed.kind === "home" || parsed.kind === "blocks" ? (
          <>
            <h1>explorer.rigorchain.com</h1>
            <p>Read-only indexer. Not a validation authority.</p>
            <p>Network: {NETWORK_NAME}</p>
            {!live ? (
              <p className="err">
                NETWORK OFFLINE
                <br />
                CONNECTION FAILED
                {state.explorer_status.last_error ? (
                  <>
                    <br />
                    {state.explorer_status.last_error}
                  </>
                ) : null}
              </p>
            ) : null}
            <dl className="kv">
              <dt>Block Height</dt>
              <dd className="mono">{live ? (height ?? "—") : "NETWORK OFFLINE"}</dd>
              <dt>Tip hash</dt>
              <dd className="mono">{live ? (hash ?? "—") : "—"}</dd>
              <dt>Peers</dt>
              <dd className="mono">{live ? (peers ?? "—") : "—"}</dd>
              <dt>Network Status</dt>
              <dd>{live ? state.global_status : "CONNECTION FAILED"}</dd>
              <dt>Source</dt>
              <dd>
                {state.rpc_status.reachable
                  ? "rigord RPC + explorer"
                  : state.explorer_status.reachable
                    ? "explorer API"
                    : "unavailable"}
              </dd>
            </dl>
            {mining ? (
              <>
                <h2>Mining Information</h2>
                <dl className="kv">
                  {Object.entries(mining)
                    .filter(([, v]) => v != null && v !== "" && typeof v !== "object")
                    .slice(0, 8)
                    .map(([k, v]) => (
                      <Fragment key={k}>
                        <dt>{k}</dt>
                        <dd className="mono">{String(v)}</dd>
                      </Fragment>
                    ))}
                </dl>
              </>
            ) : null}
            <p>
              <button type="button" className="win95-btn" onClick={() => navigate(`${EXPLORER_ORIGIN}/blocks`)}>
                Latest blocks
              </button>{" "}
              <button type="button" className="win95-btn" onClick={() => navigate(`${EXPLORER_ORIGIN}/status`)}>
                Network status
              </button>{" "}
              <a href={EXPLORER_HOME} target="_blank" rel="noreferrer">
                Open in system browser
              </a>
            </p>
            <h2>Latest Blocks</h2>
            {!live ? (
              <p className="err">NETWORK OFFLINE</p>
            ) : state.recent_blocks === null && state.explorer_status.last_error ? (
              <p className="err">CONNECTION FAILED: {state.explorer_status.last_error}</p>
            ) : (
              <table className="browser-table">
                <thead>
                  <tr>
                    <th>Height</th>
                    <th>Hash</th>
                    <th>Tx</th>
                    <th>ORU</th>
                    <th>Burn</th>
                  </tr>
                </thead>
                <tbody>
                  {blocks.map((b, i) => {
                    const h = displayBlockHash(b);
                    return (
                      <tr key={h || i}>
                        <td>{displayBlockHeight(b)}</td>
                        <td>
                          {h ? (
                            <button
                              type="button"
                              className="win95-btn win95-btn-sm"
                              onClick={() => navigate(`${EXPLORER_ORIGIN}/block/${h}`)}
                            >
                              {h.slice(0, 16)}…
                            </button>
                          ) : (
                            "—"
                          )}
                        </td>
                        <td>{b.tx_count ?? "—"}</td>
                        <td>{displayBlockOruMinted(b)}</td>
                        <td>{displayBlockConvergeBurn(b)}</td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            )}
          </>
        ) : null}

        {parsed.kind === "status" ? (
          <>
            <h1>Network Status</h1>
            <p>Network: {NETWORK_NAME}</p>
            {!live ? (
              <p className="err">
                NETWORK OFFLINE
                <br />
                CONNECTION FAILED
              </p>
            ) : null}
            <p>Live observer fields. Failures are shown; values are not invented.</p>
            <dl className="kv">
              <dt>Global</dt>
              <dd>{state.global_status}</dd>
              <dt>Height</dt>
              <dd className="mono">{height ?? "—"}</dd>
              <dt>Best hash</dt>
              <dd className="mono">{hash ?? "—"}</dd>
              <dt>IBD</dt>
              <dd>{state.initial_block_download == null ? "—" : String(state.initial_block_download)}</dd>
              <dt>Peers</dt>
              <dd>{peers ?? "—"}</dd>
              <dt>RPC</dt>
              <dd>
                {state.rpc_status.reachable
                  ? `reachable (${state.rpc_status.latency_ms ?? "?"} ms)`
                  : state.rpc_status.last_error ?? "not configured / unreachable"}
              </dd>
              <dt>Explorer</dt>
              <dd>
                {state.explorer_status.reachable
                  ? "reachable"
                  : state.explorer_status.last_error ?? "unreachable"}
              </dd>
              <dt>equivalence_claimed</dt>
              <dd>{String(state.explorer_status.equivalence_claimed ?? "—")}</dd>
            </dl>
          </>
        ) : null}

        {parsed.kind === "block" ? (
          <>
            <h1>Block</h1>
            {loading ? <p>Loading…</p> : null}
            {pageError ? <p className="err">{pageError}</p> : null}
            {blockDetail ? (
              <pre className="notepad-body" style={{ padding: 0 }}>
                {JSON.stringify(blockDetail, null, 2)}
              </pre>
            ) : null}
          </>
        ) : null}

        {parsed.kind === "tx" ? (
          <>
            <h1>Transaction</h1>
            {loading ? <p>Loading…</p> : null}
            {pageError ? <p className="err">{pageError}</p> : null}
            {txDetail ? (
              <pre className="notepad-body" style={{ padding: 0 }}>
                {JSON.stringify(txDetail, null, 2)}
              </pre>
            ) : null}
          </>
        ) : null}

        {parsed.kind === "github" ? (
          <>
            <h1>github.com/rigorchain/rigorchain</h1>
            <p>Public source repository. This browser does not embed GitHub.</p>
            <p>
              <a href={GITHUB_REPO} target="_blank" rel="noreferrer">
                {GITHUB_REPO}
              </a>
            </p>
            <p>
              <a href={GITHUB_RELEASES} target="_blank" rel="noreferrer">
                {GITHUB_RELEASES}
              </a>
            </p>
          </>
        ) : null}

        {parsed.kind === "unknown" ? (
          <>
            <h1>Cannot display this page here</h1>
            <p className="mono">{current}</p>
            <p>Rigor Browser fetches explorer.rigorchain.com APIs and known GitHub URLs.</p>
            <p>
              <a href={current} target="_blank" rel="noreferrer">
                Open in system browser
              </a>
            </p>
          </>
        ) : null}
      </div>
    </Window>
  );
}
