import { useState } from "react";
import { useSystemState } from "../../context/SystemStateContext";
import { explorerTipHeight } from "../../lib/explorerDisplay";
import { RIGOR_VERSION, RIGOR_VERSION_FULL, RELEASE_STAGE } from "../../version";
import { MAINNET_GENESIS, PRODUCT } from "../../protocolFacts";
import { GITHUB_REPO, NETWORK_NAME } from "../data/links";
import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

const SECTIONS = ["NETWORK", "SYSTEM", "PROTOCOL", "SOFTWARE", "ABOUT RIGOR"] as const;
type Section = (typeof SECTIONS)[number];

function initialSection(win: WindowRecord): Section {
  if (win.section === "network") return "NETWORK";
  if (win.section === "system") return "SYSTEM";
  if (win.section === "protocol") return "PROTOCOL";
  if (win.section === "software") return "SOFTWARE";
  if (win.section === "about") return "ABOUT RIGOR";
  return "NETWORK";
}

export function SystemPanel({ win }: { win: WindowRecord }) {
  const { closeWindow } = useDesktop();
  const { state } = useSystemState();
  const [section, setSection] = useState<Section>(initialSection(win));
  const live = state.explorer_status.reachable || state.rpc_status.reachable;
  const height = live ? state.chain_height ?? explorerTipHeight(state.explorer_snapshot) : null;
  const peers = live ? state.network_info?.connections : null;

  return (
    <Window
      win={{ ...win, title: "Control Panel" }}
      gray
      menu={
        <MenuBar
          menus={[{ label: "File", items: [{ label: "Close", action: () => closeWindow(win.id) }] }]}
        />
      }
      status={section}
    >
      <div className="cpl-layout">
        <nav className="cpl-nav">
          {SECTIONS.map((s) => (
            <button
              key={s}
              type="button"
              className={section === s ? "is-active" : ""}
              onClick={() => setSection(s)}
            >
              {s}
            </button>
          ))}
        </nav>
        <div className="cpl-body">
          {section === "NETWORK" ? (
            <>
              <h1>NETWORK</h1>
              <dl className="kv">
                <dt>Network</dt>
                <dd>{NETWORK_NAME}</dd>
                <dt>P2P</dt>
                <dd className="mono">8760</dd>
                <dt>Address</dt>
                <dd className="mono">rg1q</dd>
                <dt>Block height</dt>
                <dd className="mono">{live ? (height ?? "—") : "NETWORK OFFLINE"}</dd>
                <dt>Peers</dt>
                <dd className="mono">{live ? (peers ?? "—") : "—"}</dd>
                <dt>Status</dt>
                <dd>{live ? state.global_status : "CONNECTION FAILED"}</dd>
                <dt>RPC</dt>
                <dd>
                  {state.rpc_status.reachable
                    ? "reachable"
                    : state.rpc_status.last_error ?? "not configured"}
                </dd>
                <dt>Explorer</dt>
                <dd>
                  {state.explorer_status.reachable
                    ? "reachable"
                    : state.explorer_status.last_error ?? "unreachable"}
                </dd>
              </dl>
            </>
          ) : null}
          {section === "SYSTEM" ? (
            <>
              <h1>SYSTEM</h1>
              <dl className="kv">
                <dt>Version</dt>
                <dd className="mono">{RIGOR_VERSION_FULL}</dd>
                <dt>Build</dt>
                <dd className="mono">{import.meta.env.VITE_SITE_VERSION ?? RIGOR_VERSION}</dd>
                <dt>Release</dt>
                <dd className="mono">{RELEASE_STAGE}</dd>
                <dt>Git commit</dt>
                <dd>
                  Not bundled in this desktop. See{" "}
                  <a href={GITHUB_REPO} target="_blank" rel="noreferrer">
                    {GITHUB_REPO}
                  </a>
                </dd>
                <dt>Platform</dt>
                <dd className="mono">{navigator.platform || "browser"}</dd>
                <dt>User agent</dt>
                <dd className="mono">{navigator.userAgent}</dd>
              </dl>
            </>
          ) : null}
          {section === "PROTOCOL" ? (
            <>
              <h1>PROTOCOL</h1>
              <dl className="kv">
                <dt>Consensus</dt>
                <dd>Proof of Work</dd>
                <dt>Ledger</dt>
                <dd>UTXO</dd>
                <dt>Hash</dt>
                <dd>OpuHash</dd>
                <dt>Header</dt>
                <dd>80 bytes</dd>
                <dt>Difficulty</dt>
                <dd>ASERT</dd>
                <dt>Genesis</dt>
                <dd className="mono">{MAINNET_GENESIS}</dd>
              </dl>
            </>
          ) : null}
          {section === "SOFTWARE" ? (
            <>
              <h1>SOFTWARE</h1>
              <p>Production packages: RIGOR Node, RIGOR Wallet, RIGOR Miner, Source Code.</p>
              <p>This desktop does not execute binaries.</p>
              <p>Downloads live in C:\RIGOR\DOWNLOADS and GitHub Releases.</p>
            </>
          ) : null}
          {section === "ABOUT RIGOR" ? (
            <>
              <h1>ABOUT RIGOR</h1>
              <p>{PRODUCT}</p>
              <p>A permissionless proof-of-work blockchain.</p>
              <p>It runs on computers. It communicates through a network. It follows deterministic rules.</p>
            </>
          ) : null}
        </div>
      </div>
    </Window>
  );
}
