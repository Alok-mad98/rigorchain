import { useState } from "react";
import { WHITEPAPER_PAGES } from "../data/documents";
import { GITHUB_WHITEPAPER } from "../data/links";
import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

export function PdfViewer({ win }: { win: WindowRecord }) {
  const { closeWindow } = useDesktop();
  const [page, setPage] = useState(0);
  const current = WHITEPAPER_PAGES[page];

  return (
    <Window
      win={{ ...win, title: "Whitepaper.pdf - RIGOR Viewer" }}
      menu={
        <MenuBar
          menus={[
            {
              label: "File",
              items: [
                {
                  label: "Open on GitHub",
                  action: () => window.open(GITHUB_WHITEPAPER, "_blank", "noreferrer"),
                },
                { label: "Close", action: () => closeWindow(win.id) },
              ],
            },
            { label: "View", items: [{ label: "Fit Width" }] },
          ]}
        />
      }
      toolbar={
        <div className="pdf-toolbar">
          <button
            type="button"
            className="win95-btn win95-btn-sm"
            disabled={page <= 0}
            onClick={() => setPage((p) => p - 1)}
          >
            Prev
          </button>
          <button
            type="button"
            className="win95-btn win95-btn-sm"
            disabled={page >= WHITEPAPER_PAGES.length - 1}
            onClick={() => setPage((p) => p + 1)}
          >
            Next
          </button>
          <span>
            Page {page + 1} / {WHITEPAPER_PAGES.length}
          </span>
          <a href={GITHUB_WHITEPAPER} target="_blank" rel="noreferrer">
            Full text on GitHub
          </a>
        </div>
      }
      status={current.title}
    >
      <article className="pdf-page">
        <h1>{current.title}</h1>
        <pre className="notepad-body" style={{ padding: 0 }}>
          {current.body}
        </pre>
      </article>
    </Window>
  );
}
