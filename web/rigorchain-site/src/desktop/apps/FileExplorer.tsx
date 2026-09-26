import { useMemo, useState } from "react";
import { childrenOf, FS, parentPath } from "../data/filesystem";
import { useDesktop } from "../context/DesktopContext";
import { PixelIcon } from "../icons";
import { MenuBar, Window } from "../Window";
import type { FsNode, WindowRecord } from "../types";

export function FileExplorer({ win }: { win: WindowRecord }) {
  const { openApp, closeWindow } = useDesktop();
  const [path, setPath] = useState(win.path ?? "C:\\RIGOR");
  const [selected, setSelected] = useState<string | null>(null);
  const [history, setHistory] = useState<string[]>([win.path ?? "C:\\RIGOR"]);
  const [histIndex, setHistIndex] = useState(0);
  const [view, setView] = useState<"icons" | "details">("icons");

  const node = FS[path] ?? FS["C:\\RIGOR"];
  const items = useMemo(() => childrenOf(path), [path]);

  const go = (next: string) => {
    const slice = history.slice(0, histIndex + 1);
    slice.push(next);
    setHistory(slice);
    setHistIndex(slice.length - 1);
    setPath(next);
    setSelected(null);
  };

  const openNode = (item: FsNode) => {
    if (item.kind === "folder") {
      go(item.path);
      return;
    }
    openApp(item.open);
  };

  const tapOpens = () =>
    typeof window !== "undefined" &&
    window.matchMedia("(max-width: 720px), (pointer: coarse)").matches;

  const onItemActivate = (item: FsNode) => {
    if (tapOpens() || selected === item.id) {
      openNode(item);
      return;
    }
    setSelected(item.id);
  };

  const up = parentPath(path);

  return (
    <Window
      win={{
        ...win,
        title:
          path === "C:\\" || path === "C:\\RIGOR" ? "My Computer" : (node?.name ?? "Explorer"),
      }}
      gray
      menu={
        <MenuBar
          menus={[
            {
              label: "File",
              items: [
                {
                  label: "Open",
                  action: () => {
                    const item = items.find((i) => i.id === selected);
                    if (item) openNode(item);
                  },
                },
                { label: "Close", action: () => closeWindow(win.id) },
              ],
            },
            {
              label: "Edit",
              items: [{ label: "Select All", action: () => setSelected(items[0]?.id ?? null) }],
            },
            {
              label: "View",
              items: [
                { label: "Large Icons", action: () => setView("icons") },
                { label: "Details", action: () => setView("details") },
              ],
            },
          ]}
        />
      }
      toolbar={
        <div className="win95-window-toolbar">
          <button
            type="button"
            className="win95-btn win95-btn-sm"
            disabled={histIndex <= 0}
            onClick={() => {
              const i = histIndex - 1;
              setHistIndex(i);
              setPath(history[i]);
            }}
          >
            Back
          </button>
          <button
            type="button"
            className="win95-btn win95-btn-sm"
            disabled={histIndex >= history.length - 1}
            onClick={() => {
              const i = histIndex + 1;
              setHistIndex(i);
              setPath(history[i]);
            }}
          >
            Forward
          </button>
          <button
            type="button"
            className="win95-btn win95-btn-sm"
            disabled={!up}
            onClick={() => up && go(up)}
          >
            Up
          </button>
          <span className="win95-label">Address</span>
          <input className="win95-field" readOnly value={path} style={{ flex: 1 }} />
        </div>
      }
      status={`${items.length} object(s)`}
    >
      {view === "icons" ? (
        <div className="file-grid" onClick={() => setSelected(null)}>
          {items.map((item) => (
            <button
              key={item.id}
              type="button"
              className={`file-item${selected === item.id ? " is-selected" : ""}`}
              onClick={(e) => {
                e.stopPropagation();
                onItemActivate(item);
              }}
              onDoubleClick={() => openNode(item)}
            >
              <PixelIcon name={item.icon} />
              <span className="file-item-label">{item.name}</span>
            </button>
          ))}
        </div>
      ) : (
        <table className="file-list">
          <thead>
            <tr>
              <th>Name</th>
              <th>Type</th>
              <th>Location</th>
            </tr>
          </thead>
          <tbody>
            {items.map((item) => (
              <tr
                key={item.id}
                className={selected === item.id ? "is-selected" : ""}
                onClick={() => onItemActivate(item)}
                onDoubleClick={() => openNode(item)}
              >
                <td>
                  <span style={{ display: "inline-flex", alignItems: "center", gap: 6 }}>
                    <PixelIcon name={item.icon} size={16} />
                    {item.name}
                  </span>
                </td>
                <td>{item.typeLabel}</td>
                <td className="mono">{item.path}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </Window>
  );
}
