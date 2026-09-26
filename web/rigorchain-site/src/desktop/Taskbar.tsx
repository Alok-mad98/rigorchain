import { useEffect, useState } from "react";
import { useSystemState } from "../context/SystemStateContext";
import { useDesktop } from "./context/DesktopContext";
import { PixelIcon } from "./icons";
import { StartMenu } from "./StartMenu";

function readLocalTime(): Date {
  return new Date();
}

function formatClock(d: Date): string {
  const h24 = d.getHours();
  const m = d.getMinutes().toString().padStart(2, "0");
  const ap = h24 >= 12 ? "PM" : "AM";
  let h = h24 % 12;
  if (h === 0) h = 12;
  return `${h}:${m} ${ap}`;
}

function localZoneName(): string {
  try {
    return Intl.DateTimeFormat().resolvedOptions().timeZone;
  } catch {
    return "local";
  }
}

export function Taskbar() {
  const { windows, focusedId, startOpen, setStartOpen, toggleTask } = useDesktop();
  const { state } = useSystemState();
  const [now, setNow] = useState(() => readLocalTime());

  useEffect(() => {
    const tick = () => setNow(readLocalTime());
    tick();
    const id = window.setInterval(tick, 1000);
    const onVisible = () => {
      if (document.visibilityState === "visible") tick();
    };
    document.addEventListener("visibilitychange", onVisible);
    window.addEventListener("focus", tick);
    return () => {
      window.clearInterval(id);
      document.removeEventListener("visibilitychange", onVisible);
      window.removeEventListener("focus", tick);
    };
  }, []);

  const led =
    state.global_status === "online"
      ? "is-online"
      : state.global_status === "degraded" || state.global_status === "syncing"
        ? "is-degraded"
        : "is-offline";

  return (
    <>
      <StartMenu />
      <div className="taskbar bevel-out">
        <button
          type="button"
          className={`win95-btn start-btn${startOpen ? " is-down" : ""}`}
          onClick={(e) => {
            e.stopPropagation();
            setStartOpen(!startOpen);
          }}
        >
          <PixelIcon name="start" size={16} className="start-logo" />
          Start
        </button>
        <div className="task-buttons">
          {windows.map((w) => (
            <button
              key={w.id}
              type="button"
              className={`win95-btn task-btn${focusedId === w.id && !w.minimized ? " is-active" : ""}`}
              onClick={() => toggleTask(w.id)}
            >
              <PixelIcon name={w.icon} size={16} />
              <span className="task-btn-label">{w.title}</span>
            </button>
          ))}
        </div>
        <div className="tray bevel-in">
          <div className="tray-net" title={`Network: RIGOR MAINNET (${state.global_status})`}>
            <span className={`led ${led}`} />
            MAINNET
          </div>
          <div className="tray-clock" title={`${now.toLocaleString()} (${localZoneName()})`}>
            {formatClock(now)}
          </div>
        </div>
      </div>
    </>
  );
}
