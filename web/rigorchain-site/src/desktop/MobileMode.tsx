import { OPENS } from "./data/filesystem";
import { useDesktop } from "./context/DesktopContext";
import { PixelIcon } from "./icons";
import { WindowManager } from "./WindowManager";
import type { AppOpen, IconName } from "./types";

const ROWS: { id: string; name: string; icon: IconName; open: AppOpen }[] = [
  { id: "computer", name: "My Computer", icon: "computer", open: OPENS.computer },
  { id: "documents", name: "Documents", icon: "documents", open: OPENS.documents },
  { id: "network", name: "Rigor Network", icon: "network", open: OPENS.network },
  { id: "browser", name: "Rigor Browser", icon: "explorer", open: OPENS.browser },
  { id: "downloads", name: "Downloads", icon: "downloads", open: OPENS.downloads },
  { id: "control", name: "Control Panel", icon: "control", open: OPENS.control },
];

export function MobileMode() {
  const { windows, openApp, closeWindow, focused } = useDesktop();
  const open = focused && !focused.minimized ? focused : windows.find((w) => !w.minimized) ?? null;

  return (
    <div className="mobile-root">
      <div className="mobile-header">RIGOR SYSTEM</div>
      {open ? (
        <div className="mobile-app">
          <WindowManager />
          <div className="taskbar bevel-out">
            <button type="button" className="win95-btn" onClick={() => closeWindow(open.id)}>
              Back
            </button>
            <span className="task-btn-label">{open.title}</span>
          </div>
        </div>
      ) : (
        <>
          <div className="mobile-prompt">&gt; select a program</div>
          <div className="mobile-list">
            {ROWS.map((row) => (
              <button
                key={row.id}
                type="button"
                className="mobile-row"
                onClick={() => openApp(row.open)}
              >
                <PixelIcon name={row.icon} />
                <span className="mobile-row-label">&gt; {row.name}</span>
              </button>
            ))}
          </div>
        </>
      )}
    </div>
  );
}
