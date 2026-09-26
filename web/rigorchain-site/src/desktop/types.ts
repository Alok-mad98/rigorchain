export type IconName =
  | "computer"
  | "folder"
  | "documents"
  | "explorer"
  | "miner"
  | "downloads"
  | "control"
  | "recycle"
  | "text"
  | "pdf"
  | "network"
  | "source"
  | "wallet"
  | "node"
  | "help"
  | "start";

export type AppKind =
  | "folder"
  | "text"
  | "pdf"
  | "browser"
  | "control"
  | "downloads"
  | "recycle"
  | "node"
  | "wallet"
  | "miner"
  | "source"
  | "about"
  | "shutdown";

export type AppOpen = {
  kind: AppKind;
  title: string;
  icon: IconName;
  path?: string;
  docId?: string;
  section?: string;
  href?: string;
};

export type WindowRecord = {
  id: string;
  kind: AppKind;
  title: string;
  icon: IconName;
  x: number;
  y: number;
  w: number;
  h: number;
  restore: { x: number; y: number; w: number; h: number } | null;
  minimized: boolean;
  maximized: boolean;
  z: number;
  path?: string;
  docId?: string;
  section?: string;
  href?: string;
};

export type FsKind = "folder" | "file" | "app";

export type FsNode = {
  id: string;
  name: string;
  kind: FsKind;
  icon: IconName;
  path: string;
  typeLabel: string;
  children?: string[];
  open: AppOpen;
};
