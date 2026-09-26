import { MAINNET_GENESIS, PRODUCT } from "../../protocolFacts";
import { RIGOR_VERSION_FULL } from "../../version";
import { GITHUB_REPO, NETWORK_NAME } from "../data/links";
import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

export function AboutApp({ win }: { win: WindowRecord }) {
  const { closeWindow } = useDesktop();
  return (
    <Window
      win={win}
      gray
      menu={
        <MenuBar
          menus={[{ label: "File", items: [{ label: "Close", action: () => closeWindow(win.id) }] }]}
        />
      }
      status={PRODUCT}
    >
      <div className="app-page">
        <h1>About RIGOR</h1>
        <dl className="kv">
          <dt>Network</dt>
          <dd>{NETWORK_NAME}</dd>
          <dt>Product</dt>
          <dd>{RIGOR_VERSION_FULL}</dd>
          <dt>Consensus</dt>
          <dd>Proof of Work</dd>
          <dt>Ledger</dt>
          <dd>UTXO</dd>
          <dt>Hash</dt>
          <dd>OpuHash</dd>
          <dt>Header</dt>
          <dd>80 bytes</dd>
          <dt>Genesis</dt>
          <dd className="mono">{MAINNET_GENESIS}</dd>
        </dl>
        <p>
          <a href={GITHUB_REPO} target="_blank" rel="noreferrer">
            {GITHUB_REPO}
          </a>
        </p>
      </div>
    </Window>
  );
}
