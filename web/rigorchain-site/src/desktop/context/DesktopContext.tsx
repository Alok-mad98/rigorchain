import {
  createContext,
  useCallback,
  useContext,
  useMemo,
  useState,
  type ReactNode,
} from "react";
import { OPENS } from "../data/filesystem";
import type { AppKind, AppOpen, WindowRecord } from "../types";

type DesktopApi = {
  windows: WindowRecord[];
  focusedId: string | null;
  startOpen: boolean;
  programsOpen: boolean;
  selectedIcon: string | null;
  setSelectedIcon: (id: string | null) => void;
  setStartOpen: (open: boolean) => void;
  setProgramsOpen: (open: boolean) => void;
  openApp: (open: AppOpen, opts?: { place?: "center" }) => void;
  closeWindow: (id: string) => void;
  focusWindow: (id: string) => void;
  minimizeWindow: (id: string) => void;
  toggleMaximize: (id: string) => void;
  toggleTask: (id: string) => void;
  moveWindow: (id: string, x: number, y: number) => void;
  focused: WindowRecord | null;
};

const DesktopContext = createContext<DesktopApi | null>(null);

function keyOf(open: AppOpen): string {
  if (open.kind === "folder") return `folder:${open.path ?? ""}`;
  if (open.kind === "text") return `text:${open.docId ?? open.path ?? ""}`;
  if (open.kind === "pdf") return `pdf:${open.docId ?? ""}`;
  if (open.kind === "control") return `control`;
  return open.kind;
}

function defaultSize(kind: AppKind): { w: number; h: number } {
  switch (kind) {
    case "browser":
      return { w: 720, h: 480 };
    case "folder":
    case "downloads":
      return { w: 560, h: 380 };
    case "control":
    case "source":
      return { w: 540, h: 400 };
    case "text":
      return { w: 480, h: 360 };
    case "pdf":
      return { w: 560, h: 440 };
    case "shutdown":
      return { w: 360, h: 180 };
    default:
      return { w: 520, h: 380 };
  }
}

const TASKBAR_H = 28;
/** Three desktop icon columns (8 + 92*3 + 18*2) plus a gap so windows sit clear of them. */
const ICON_RESERVE = 336;

function desktopBox() {
  const vw = typeof window !== "undefined" ? window.innerWidth : 1024;
  const vh = typeof window !== "undefined" ? window.innerHeight : 720;
  return { vw, vh };
}

function clampPlace(x: number, y: number, w: number, h: number) {
  const { vw, vh } = desktopBox();
  const reserve = Math.min(ICON_RESERVE, Math.max(8, vw - w - 12));
  const maxX = Math.max(reserve, vw - w - 12);
  const maxY = Math.max(16, vh - TASKBAR_H - h - 8);
  return {
    x: Math.min(Math.max(reserve, x), maxX),
    y: Math.min(Math.max(16, y), maxY),
  };
}

function centeredPlace(w: number, h: number) {
  const { vw, vh } = desktopBox();
  return clampPlace(Math.round((vw - w) / 2), Math.round((vh - TASKBAR_H - h) / 2), w, h);
}

function cascadePlace(w: number, h: number) {
  const offset = (cascade++ % 8) * 22;
  return clampPlace(ICON_RESERVE + offset, 24 + offset, w, h);
}

let cascade = 0;

export function DesktopProvider({ children }: { children: ReactNode }) {
  const [windows, setWindows] = useState<WindowRecord[]>([]);
  const [focusedId, setFocusedId] = useState<string | null>(null);
  const [startOpen, setStartOpenRaw] = useState(false);
  const [programsOpen, setProgramsOpen] = useState(false);
  const [selectedIcon, setSelectedIcon] = useState<string | null>(null);
  const [zTop, setZTop] = useState(1);

  const setStartOpen = useCallback((open: boolean) => {
    setStartOpenRaw(open);
    if (!open) setProgramsOpen(false);
  }, []);

  const openApp = useCallback(
    (open: AppOpen, opts?: { place?: "center" }) => {
      const id = keyOf(open);
      setStartOpen(false);
      setWindows((prev) => {
        const existing = prev.find((w) => w.id === id);
        if (existing) {
          const z = zTop + 1;
          setZTop(z);
          setFocusedId(id);
          return prev.map((w) =>
            w.id === id
              ? {
                  ...w,
                  minimized: false,
                  z,
                  section: open.section ?? w.section,
                  title: open.title,
                }
              : w,
          );
        }
        const { w, h } = defaultSize(open.kind);
        const pos = opts?.place === "center" ? centeredPlace(w, h) : cascadePlace(w, h);
        const z = zTop + 1;
        setZTop(z);
        setFocusedId(id);
        const rec: WindowRecord = {
          id,
          kind: open.kind,
          title: open.title,
          icon: open.icon,
          x: pos.x,
          y: pos.y,
          w,
          h,
          restore: null,
          minimized: false,
          maximized: false,
          z,
          path: open.path,
          docId: open.docId,
          section: open.section,
          href: open.href,
        };
        return [...prev, rec];
      });
    },
    [setStartOpen, zTop],
  );

  const focusWindow = useCallback((id: string) => {
    setWindows((prev) => {
      const z = Math.max(0, ...prev.map((w) => w.z)) + 1;
      setZTop(z);
      setFocusedId(id);
      return prev.map((w) => (w.id === id ? { ...w, z, minimized: false } : w));
    });
  }, []);

  const closeWindow = useCallback((id: string) => {
    setWindows((prev) => {
      const next = prev.filter((w) => w.id !== id);
      const top = [...next].sort((a, b) => b.z - a.z)[0];
      setFocusedId(top && !top.minimized ? top.id : null);
      return next;
    });
  }, []);

  const minimizeWindow = useCallback((id: string) => {
    setWindows((prev) => {
      const next = prev.map((w) => (w.id === id ? { ...w, minimized: true } : w));
      const top = [...next].filter((w) => !w.minimized).sort((a, b) => b.z - a.z)[0];
      setFocusedId(top ? top.id : null);
      return next;
    });
  }, []);

  const toggleMaximize = useCallback((id: string) => {
    setWindows((prev) =>
      prev.map((w) => {
        if (w.id !== id) return w;
        if (w.maximized) {
          const r = w.restore ?? { x: 80, y: 40, w: w.w, h: w.h };
          return { ...w, maximized: false, restore: null, ...r };
        }
        return {
          ...w,
          maximized: true,
          restore: { x: w.x, y: w.y, w: w.w, h: w.h },
        };
      }),
    );
  }, []);

  const toggleTask = useCallback(
    (id: string) => {
      const win = windows.find((w) => w.id === id);
      if (!win) return;
      if (focusedId === id && !win.minimized) {
        minimizeWindow(id);
        return;
      }
      focusWindow(id);
    },
    [focusedId, focusWindow, minimizeWindow, windows],
  );

  const moveWindow = useCallback((id: string, x: number, y: number) => {
    setWindows((prev) =>
      prev.map((w) => {
        if (w.id !== id) return w;
        const nx = Math.max(-w.w + 80, x);
        const ny = Math.max(0, Math.min(window.innerHeight - 60, y));
        if (w.maximized) {
          return {
            ...w,
            maximized: false,
            restore: null,
            x: nx,
            y: ny,
            w: w.restore?.w ?? w.w,
            h: w.restore?.h ?? w.h,
          };
        }
        return { ...w, x: nx, y: ny };
      }),
    );
  }, []);

  const focused = windows.find((w) => w.id === focusedId) ?? null;

  const value = useMemo<DesktopApi>(
    () => ({
      windows,
      focusedId,
      startOpen,
      programsOpen,
      selectedIcon,
      setSelectedIcon,
      setStartOpen,
      setProgramsOpen,
      openApp,
      closeWindow,
      focusWindow,
      minimizeWindow,
      toggleMaximize,
      toggleTask,
      moveWindow,
      focused,
    }),
    [
      windows,
      focusedId,
      startOpen,
      programsOpen,
      selectedIcon,
      setStartOpen,
      openApp,
      closeWindow,
      focusWindow,
      minimizeWindow,
      toggleMaximize,
      toggleTask,
      moveWindow,
      focused,
    ],
  );

  return <DesktopContext.Provider value={value}>{children}</DesktopContext.Provider>;
}

export function useDesktop(): DesktopApi {
  const ctx = useContext(DesktopContext);
  if (!ctx) throw new Error("useDesktop outside provider");
  return ctx;
}

export { OPENS };
