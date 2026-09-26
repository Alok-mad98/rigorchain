import { DESKTOP_COLUMNS, DESKTOP_ICONS } from "./data/filesystem";
import { useDesktop } from "./context/DesktopContext";
import { DesktopIcon } from "./DesktopIcon";
import { Taskbar } from "./Taskbar";
import { WindowManager } from "./WindowManager";

export function Desktop() {
  const { selectedIcon, setSelectedIcon, openApp, setStartOpen } = useDesktop();

  return (
    <div
      className="desktop-root is-desktop-only"
      tabIndex={-1}
      onKeyDown={(e) => {
        if (e.key !== "Enter" || !selectedIcon) return;
        const icon = DESKTOP_ICONS.find((i) => i.id === selectedIcon);
        if (icon) openApp(icon.open);
      }}
      onMouseDown={(e) => {
        const t = e.target as HTMLElement;
        if (t.closest(".desktop-icon, .win95-window, .taskbar, .start-menu")) {
          if (!t.closest(".start-btn, .start-menu")) setStartOpen(false);
          return;
        }
        setSelectedIcon(null);
        setStartOpen(false);
      }}
    >
      <div className="desktop-surface">
        <div className="desktop-icons">
          {DESKTOP_COLUMNS.map((col) => (
            <div key={col.id} className="desktop-col">
              {col.icons.map((icon) => (
                <DesktopIcon
                  key={icon.id}
                  id={icon.id}
                  name={icon.name}
                  icon={icon.icon}
                  selected={selectedIcon === icon.id}
                  onSelect={setSelectedIcon}
                  onOpen={() => openApp(icon.open)}
                />
              ))}
            </div>
          ))}
        </div>
        <WindowManager />
      </div>
      <Taskbar />
    </div>
  );
}
