import { useCallback, useEffect, useState } from "react";
import { useLocation } from "react-router-dom";
import { BootScreen } from "./BootScreen";
import { DesktopProvider, OPENS, useDesktop } from "./context/DesktopContext";
import { Desktop } from "./Desktop";
import { MobileMode } from "./MobileMode";
import type { AppOpen } from "./types";

function useIsMobile() {
  const [mobile, setMobile] = useState(() =>
    typeof window !== "undefined" ? window.matchMedia("(max-width: 720px)").matches : false,
  );
  useEffect(() => {
    const mq = window.matchMedia("(max-width: 720px)");
    const onChange = () => setMobile(mq.matches);
    mq.addEventListener("change", onChange);
    return () => mq.removeEventListener("change", onChange);
  }, []);
  return mobile;
}

function openFromPath(pathname: string): AppOpen {
  const p = pathname.replace(/\/+$/, "") || "/";
  if (p === "/explorer" || p.startsWith("/explorer/")) return OPENS.browser;
  if (p === "/download" || p.startsWith("/download") || p === "/use/download") return OPENS.downloads;
  if (p === "/documentation" || p.startsWith("/documentation") || p === "/use") return OPENS.documents;
  if (p.includes("wallet")) return OPENS.wallet;
  if (p.includes("mining") || p.includes("miner")) return OPENS.miner;
  if (p.startsWith("/protocol")) return OPENS.protocol;
  if (p === "/about") return OPENS.about;
  return OPENS.readme;
}

function BootThenDesktop() {
  const location = useLocation();
  const { openApp } = useDesktop();
  const mobile = useIsMobile();
  const [booted, setBooted] = useState(false);
  const [opened, setOpened] = useState(false);

  const onDone = useCallback(() => setBooted(true), []);

  useEffect(() => {
    if (!booted || opened) return;
    if (mobile) {
      setOpened(true);
      return;
    }
    openApp(openFromPath(location.pathname), { place: "center" });
    setOpened(true);
  }, [booted, location.pathname, mobile, openApp, opened]);

  if (!booted) return <BootScreen onDone={onDone} />;

  return mobile ? <MobileMode /> : <Desktop />;
}

export function DesktopShell() {
  return (
    <DesktopProvider>
      <BootThenDesktop />
    </DesktopProvider>
  );
}
