import { GITHUB_CORE, GITHUB_RELEASES, GITHUB_REPO, NETWORK_NAME } from "../data/links";
import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

export function NodeApp({ win }: { win: WindowRecord }) {
  const { closeWindow, openApp } = useDesktop();
  return (
    <Window
      win={win}
      menu={
        <MenuBar
          menus={[{ label: "File", items: [{ label: "Close", action: () => closeWindow(win.id) }] }]}
        />
      }
      status="C:\\RIGOR\\NODE"
    >
      <div className="app-page">
        <h1>RIGOR Node</h1>
        <p>
          A node is a computer that stores the chain, checks every block against consensus rules,
          and talks to peers. It is the operational consensus implementation.
        </p>
        <p>
          Binaries: <span className="mono">rigord</span>, <span className="mono">rigor-cli</span>.
          Do not expose JSON-RPC to the public Internet.
        </p>
        <h2>Network</h2>
        <p>
          {NETWORK_NAME}. P2P 8760. Address prefix rg1q. RPC stays on 127.0.0.1.
        </p>
        <h2>Platforms</h2>
        <ul>
          <li>Linux x86_64</li>
          <li>Windows</li>
          <li>Other platforms: build from the repository</li>
        </ul>
        <div className="app-links">
          <a href={GITHUB_CORE} target="_blank" rel="noreferrer">
            core/ on GitHub
          </a>
          <a href={GITHUB_REPO} target="_blank" rel="noreferrer">
            Source Code
          </a>
          <a href={GITHUB_RELEASES} target="_blank" rel="noreferrer">
            Checksums / Release Notes
          </a>
          <button
            type="button"
            className="win95-btn"
            onClick={() => openApp({ kind: "downloads", title: "Downloads", icon: "downloads" })}
          >
            Open DOWNLOADS
          </button>
        </div>
      </div>
    </Window>
  );
}
