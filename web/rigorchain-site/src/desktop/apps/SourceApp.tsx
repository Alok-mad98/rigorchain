import {
  GITHUB_CORE,
  GITHUB_DOCS,
  GITHUB_EXPLORER,
  GITHUB_MINER,
  GITHUB_REPO,
  GITHUB_WALLET,
} from "../data/links";
import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

const ITEMS = [
  { name: "CORE", href: GITHUB_CORE, note: "Node, consensus, wallet sources" },
  { name: "WALLET", href: GITHUB_WALLET, note: "Wallet lives in core/" },
  { name: "MINER", href: GITHUB_MINER, note: "miner/" },
  { name: "EXPLORER", href: GITHUB_EXPLORER, note: "explorer/" },
  { name: "DOCUMENTATION", href: GITHUB_DOCS, note: "docs/" },
] as const;

export function SourceApp({ win }: { win: WindowRecord }) {
  const { closeWindow } = useDesktop();
  return (
    <Window
      win={win}
      menu={
        <MenuBar
          menus={[{ label: "File", items: [{ label: "Close", action: () => closeWindow(win.id) }] }]}
        />
      }
      status="C:\\RIGOR\\SOURCE"
    >
      <div className="app-page">
        <h1>RIGOR SOURCE</h1>
        <p>Monorepo. One GitHub repository.</p>
        <table className="browser-table">
          <thead>
            <tr>
              <th>Name</th>
              <th>Path</th>
            </tr>
          </thead>
          <tbody>
            {ITEMS.map((item) => (
              <tr key={item.name}>
                <td>{item.name}</td>
                <td>
                  <a href={item.href} target="_blank" rel="noreferrer">
                    {item.note}
                  </a>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
        <div className="app-links">
          <a href={GITHUB_REPO} target="_blank" rel="noreferrer">
            OPEN GITHUB
          </a>
        </div>
      </div>
    </Window>
  );
}
