import { DOCUMENTS } from "../data/documents";
import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

export function TextViewer({ win }: { win: WindowRecord }) {
  const { closeWindow } = useDesktop();
  const docId = win.docId ?? "readme";
  const text = DOCUMENTS[docId] ?? "File not found.";
  const lines = text.split("\n").length;

  return (
    <Window
      win={{ ...win, title: `${win.title.replace(/ - Notepad$/, "")} - Notepad` }}
      menu={
        <MenuBar
          menus={[
            {
              label: "File",
              items: [{ label: "Close", action: () => closeWindow(win.id) }],
            },
            { label: "Edit", items: [{ label: "Word Wrap" }] },
            { label: "View", items: [{ label: "Status Bar" }] },
          ]}
        />
      }
      status={`Ln ${lines}`}
    >
      <pre className="notepad-body">{text}</pre>
    </Window>
  );
}
