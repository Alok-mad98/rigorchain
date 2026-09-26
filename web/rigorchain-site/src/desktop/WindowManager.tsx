import { AboutApp } from "./apps/AboutApp";
import { BrowserWindow } from "./apps/BrowserWindow";
import { DownloadManager } from "./apps/DownloadManager";
import { FileExplorer } from "./apps/FileExplorer";
import { MinerApp } from "./apps/MinerApp";
import { NodeApp } from "./apps/NodeApp";
import { PdfViewer } from "./apps/PdfViewer";
import { RecycleBin } from "./apps/RecycleBin";
import { SourceApp } from "./apps/SourceApp";
import { ShutdownDialog } from "./apps/ShutdownDialog";
import { SystemPanel } from "./apps/SystemPanel";
import { TextViewer } from "./apps/TextViewer";
import { WalletApp } from "./apps/WalletApp";
import { useDesktop } from "./context/DesktopContext";

export function WindowManager() {
  const { windows } = useDesktop();
  return (
    <div className="window-layer">
      {windows.map((win) => {
        switch (win.kind) {
          case "folder":
            return <FileExplorer key={win.id} win={win} />;
          case "browser":
            return <BrowserWindow key={win.id} win={win} />;
          case "text":
            return <TextViewer key={win.id} win={win} />;
          case "pdf":
            return <PdfViewer key={win.id} win={win} />;
          case "control":
            return <SystemPanel key={win.id} win={win} />;
          case "downloads":
            return <DownloadManager key={win.id} win={win} />;
          case "recycle":
            return <RecycleBin key={win.id} win={win} />;
          case "node":
            return <NodeApp key={win.id} win={win} />;
          case "wallet":
            return <WalletApp key={win.id} win={win} />;
          case "miner":
            return <MinerApp key={win.id} win={win} />;
          case "source":
            return <SourceApp key={win.id} win={win} />;
          case "about":
            return <AboutApp key={win.id} win={win} />;
          case "shutdown":
            return <ShutdownDialog key={win.id} win={win} />;
          default:
            return null;
        }
      })}
    </div>
  );
}
