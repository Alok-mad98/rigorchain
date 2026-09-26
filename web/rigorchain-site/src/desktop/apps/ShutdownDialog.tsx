import { useDesktop } from "../context/DesktopContext";
import { Window } from "../Window";
import type { WindowRecord } from "../types";

export function ShutdownDialog({ win }: { win: WindowRecord }) {
  const { closeWindow } = useDesktop();
  return (
    <Window win={win} gray status="Visual metaphor only">
      <div className="app-page">
        <h1>Shut Down RIGOR</h1>
        <p>This is a website. The protocol does not stop.</p>
        <p>It is now safe to close this browser tab if you want to leave the desktop.</p>
        <div className="app-links">
          <button type="button" className="win95-btn" onClick={() => closeWindow(win.id)}>
            Cancel
          </button>
        </div>
      </div>
    </Window>
  );
}
