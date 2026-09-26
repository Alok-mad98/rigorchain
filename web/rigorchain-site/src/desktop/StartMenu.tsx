import { OPENS, useDesktop } from "./context/DesktopContext";
import { PixelIcon } from "./icons";

export function StartMenu() {
  const { startOpen, programsOpen, setStartOpen, setProgramsOpen, openApp } = useDesktop();
  if (!startOpen) return null;

  return (
    <div className="start-menu bevel-window" onMouseDown={(e) => e.stopPropagation()}>
      <div className="start-banner">RIGOR</div>
      <div className="start-items">
        <button
          type="button"
          className={`start-item${programsOpen ? " is-open" : ""}`}
          onMouseEnter={() => setProgramsOpen(true)}
          onClick={() => setProgramsOpen(!programsOpen)}
        >
          <PixelIcon name="folder" size={24} />
          Programs
          <span className="caret">▶</span>
          {programsOpen ? (
            <div className="start-sub bevel-window">
              <button
                type="button"
                className="start-item"
                onClick={() => openApp(OPENS.browser)}
              >
                <PixelIcon name="explorer" size={24} />
                Rigor Browser
              </button>
              <button type="button" className="start-item" onClick={() => openApp(OPENS.miner)}>
                <PixelIcon name="miner" size={24} />
                RIGOR Miner
              </button>
              <button type="button" className="start-item" onClick={() => openApp(OPENS.wallet)}>
                <PixelIcon name="wallet" size={24} />
                RIGOR Wallet
              </button>
              <button
                type="button"
                className="start-item"
                onClick={() => openApp(OPENS.documents)}
              >
                <PixelIcon name="documents" size={24} />
                Documentation
              </button>
            </div>
          ) : null}
        </button>
        <button
          type="button"
          className="start-item"
          onMouseEnter={() => setProgramsOpen(false)}
          onClick={() => openApp(OPENS.documents)}
        >
          <PixelIcon name="documents" size={24} />
          Documents
        </button>
        <button
          type="button"
          className="start-item"
          onClick={() => openApp(OPENS.network)}
        >
          <PixelIcon name="network" size={24} />
          Network
        </button>
        <button
          type="button"
          className="start-item"
          onClick={() => openApp(OPENS.downloads)}
        >
          <PixelIcon name="downloads" size={24} />
          Downloads
        </button>
        <button type="button" className="start-item" onClick={() => openApp(OPENS.source)}>
          <PixelIcon name="source" size={24} />
          Source
        </button>
        <button type="button" className="start-item" onClick={() => openApp(OPENS.control)}>
          <PixelIcon name="control" size={24} />
          Control Panel
        </button>
        <button type="button" className="start-item" onClick={() => openApp(OPENS.about)}>
          <PixelIcon name="help" size={24} />
          About RIGOR
        </button>
        <div className="start-sep" />
        <button
          type="button"
          className="start-item"
          onClick={() => {
            setStartOpen(false);
            openApp(OPENS.shutdown);
          }}
        >
          <PixelIcon name="computer" size={24} />
          Shut Down...
        </button>
      </div>
    </div>
  );
}
