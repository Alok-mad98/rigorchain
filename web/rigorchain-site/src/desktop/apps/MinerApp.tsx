import { GITHUB_MINER, GITHUB_RELEASES, NETWORK_NAME } from "../data/links";
import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

export function MinerApp({ win }: { win: WindowRecord }) {
  const { closeWindow, openApp } = useDesktop();
  return (
    <Window
      win={win}
      menu={
        <MenuBar
          menus={[{ label: "File", items: [{ label: "Close", action: () => closeWindow(win.id) }] }]}
        />
      }
      status="C:\\RIGOR\\MINER"
    >
      <div className="app-page">
        <h1>RIGOR Miner</h1>
        <p>
          Mining is Proof of Work. The work function is OpuHash. Difficulty adjustment is ASERT.
          The miner receives the ORU subsidy. RES is not mined.
        </p>
        <p>
          Network: {NETWORK_NAME}. SOLO path: local <span className="mono">rigord</span> GBT →
          rigor-miner → submitblock. Coinbase must be rg1q. Local RPC stays on 127.0.0.1.
        </p>
        <h2>OpuHash</h2>
        <pre className="notepad-body" style={{ padding: 0 }}>
          {`OpuHash(header) = SHA3-256("RGRPOW01" || header_80)
valid           <=>  OpuHash(header) <= target`}
        </pre>
        <h2>Download</h2>
        <div className="app-links">
          <a href={GITHUB_RELEASES} target="_blank" rel="noreferrer">
            RIGOR Miner on GitHub Releases
          </a>
          <a href={GITHUB_MINER} target="_blank" rel="noreferrer">
            miner/ source
          </a>
          <button
            type="button"
            className="win95-btn"
            onClick={() =>
              openApp({ kind: "text", title: "OpuHash.txt", icon: "text", docId: "opuhash" })
            }
          >
            OpuHash.txt
          </button>
        </div>
      </div>
    </Window>
  );
}
