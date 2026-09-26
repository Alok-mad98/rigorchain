import { useDesktop } from "../context/DesktopContext";
import { MenuBar, Window } from "../Window";
import type { WindowRecord } from "../types";

export function RecycleBin({ win }: { win: WindowRecord }) {
  const { closeWindow } = useDesktop();
  return (
    <Window
      win={win}
      gray
      menu={
        <MenuBar
          menus={[
            { label: "File", items: [{ label: "Close", action: () => closeWindow(win.id) }] },
            { label: "Edit", items: [{ label: "Empty Recycle Bin" }] },
          ]}
        />
      }
      status="0 object(s)"
    >
      <div className="recycle-empty">
        Recycle Bin
        {"\n\n"}
        No deleted blocks.
        {"\n\n"}
        RIGOR does not forget.
      </div>
    </Window>
  );
}
