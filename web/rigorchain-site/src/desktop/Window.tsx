import { useRef, useState, type MouseEvent as ReactMouseEvent, type ReactNode } from "react";
import { useDesktop } from "./context/DesktopContext";
import { PixelIcon } from "./icons";
import type { WindowRecord } from "./types";

type MenuDef = { label: string; items: { label: string; action?: () => void }[] };

export function MenuBar({ menus }: { menus: MenuDef[] }) {
  const [open, setOpen] = useState<string | null>(null);
  return (
    <div className="win95-menubar" onMouseLeave={() => setOpen(null)}>
      {menus.map((m) => (
        <div key={m.label} style={{ position: "relative" }}>
          <button
            type="button"
            className={`win95-menu-item${open === m.label ? " is-open" : ""}`}
            onClick={() => setOpen(open === m.label ? null : m.label)}
          >
            {m.label}
          </button>
          {open === m.label ? (
            <div className="win95-menu-drop bevel-out">
              {m.items.map((item) => (
                <button
                  key={item.label}
                  type="button"
                  onClick={() => {
                    item.action?.();
                    setOpen(null);
                  }}
                >
                  {item.label}
                </button>
              ))}
            </div>
          ) : null}
        </div>
      ))}
    </div>
  );
}

export function Window({
  win,
  menu,
  toolbar,
  status,
  gray,
  children,
}: {
  win: WindowRecord;
  menu?: ReactNode;
  toolbar?: ReactNode;
  status?: ReactNode;
  gray?: boolean;
  children: ReactNode;
}) {
  const { focusedId, focusWindow, closeWindow, minimizeWindow, toggleMaximize, moveWindow } =
    useDesktop();
  const drag = useRef<{ dx: number; dy: number } | null>(null);
  const focused = focusedId === win.id;

  const onTitleDown = (e: ReactMouseEvent) => {
    if (e.button !== 0) return;
    if ((e.target as HTMLElement).closest("button")) return;
    focusWindow(win.id);
    drag.current = { dx: e.clientX - win.x, dy: e.clientY - win.y };
    const move = (ev: MouseEvent) => {
      if (!drag.current) return;
      moveWindow(win.id, ev.clientX - drag.current.dx, ev.clientY - drag.current.dy);
    };
    const up = () => {
      drag.current = null;
      window.removeEventListener("mousemove", move);
      window.removeEventListener("mouseup", up);
    };
    window.addEventListener("mousemove", move);
    window.addEventListener("mouseup", up);
  };

  return (
    <div
      className={`win95-window bevel-window${focused ? " is-focused" : ""}${
        win.maximized ? " is-max" : ""
      }${win.minimized ? " is-min" : ""}`}
      style={
        win.maximized
          ? { zIndex: win.z }
          : { left: win.x, top: win.y, width: win.w, height: win.h, zIndex: win.z }
      }
      onMouseDown={() => focusWindow(win.id)}
    >
      <div className="win95-titlebar" onMouseDown={onTitleDown} onDoubleClick={() => toggleMaximize(win.id)}>
        <span className="win95-titlebar-icon">
          <PixelIcon name={win.icon} size={16} />
        </span>
        <span className="win95-titlebar-text">{win.title}</span>
        <div className="win95-title-btns">
          <button type="button" className="win95-tb-btn" aria-label="Minimize" onClick={() => minimizeWindow(win.id)}>
            _
          </button>
          <button type="button" className="win95-tb-btn" aria-label="Maximize" onClick={() => toggleMaximize(win.id)}>
            {win.maximized ? "❐" : "□"}
          </button>
          <button type="button" className="win95-tb-btn" aria-label="Close" onClick={() => closeWindow(win.id)}>
            X
          </button>
        </div>
      </div>
      {menu}
      {toolbar}
      <div className={`win95-window-body bevel-in${gray ? " is-gray" : ""}`}>{children}</div>
      {status != null ? (
        <div className="win95-statusbar">
          <div className="win95-status-slot bevel-in">{status}</div>
        </div>
      ) : null}
    </div>
  );
}
