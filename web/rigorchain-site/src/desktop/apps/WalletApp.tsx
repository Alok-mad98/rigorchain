import { GITHUB_RELEASES, NETWORK_NAME } from "../data/links";
import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

export function WalletApp({ win }: { win: WindowRecord }) {
  const { closeWindow, openApp } = useDesktop();
  return (
    <Window
      win={win}
      menu={
        <MenuBar
          menus={[{ label: "File", items: [{ label: "Close", action: () => closeWindow(win.id) }] }]}
        />
      }
      status="C:\\RIGOR\\WALLET"
    >
      <div className="app-page">
        <h1>RIGOR Wallet</h1>
        <p>
          A spending tool. It is not an authority for consensus. It holds keys and constructs
          transactions. Validate the chain with a local node.
        </p>
        <p>
          Network: {NETWORK_NAME}. Addresses start with rg1q.
        </p>
        <h2>Download</h2>
        <p>
          <a href={GITHUB_RELEASES} target="_blank" rel="noreferrer">
            RIGOR Wallet on GitHub Releases
          </a>
        </p>
        <h2>Documentation</h2>
        <div className="app-links">
          <button
            type="button"
            className="win95-btn"
            onClick={() =>
              openApp({
                kind: "text",
                title: "Transaction.txt",
                icon: "text",
                docId: "transaction",
              })
            }
          >
            Transaction.txt
          </button>
          <button
            type="button"
            className="win95-btn"
            onClick={() =>
              openApp({ kind: "folder", title: "Documents", icon: "documents", path: "C:\\RIGOR\\DOCUMENTS" })
            }
          >
            Documents
          </button>
        </div>
      </div>
    </Window>
  );
}
