import type { AppOpen, FsNode, IconName } from "../types";

function folder(path: string, name: string, children: string[], icon: IconName = "folder"): FsNode {
  return {
    id: path,
    name,
    kind: "folder",
    icon,
    path,
    typeLabel: "File Folder",
    children,
    open: { kind: "folder", title: name, icon, path },
  };
}

function textFile(path: string, name: string, docId: string): FsNode {
  return {
    id: path,
    name,
    kind: "file",
    icon: "text",
    path,
    typeLabel: "Text Document",
    open: { kind: "text", title: name, icon: "text", path, docId },
  };
}

function pdfFile(path: string, name: string, docId: string): FsNode {
  return {
    id: path,
    name,
    kind: "file",
    icon: "pdf",
    path,
    typeLabel: "PDF Document",
    open: { kind: "pdf", title: name, icon: "pdf", path, docId },
  };
}

function app(path: string, name: string, open: AppOpen, icon: IconName, typeLabel: string): FsNode {
  return {
    id: path,
    name,
    kind: "app",
    icon,
    path,
    typeLabel,
    open,
  };
}

export const FS: Record<string, FsNode> = {
  "C:\\": folder("C:\\", "My Computer", ["C:\\RIGOR"], "computer"),
  "C:\\RIGOR": folder(
    "C:\\RIGOR",
    "C:\\RIGOR",
    [
      "C:\\RIGOR\\NODE",
      "C:\\RIGOR\\WALLET",
      "C:\\RIGOR\\MINER",
      "C:\\RIGOR\\PROTOCOL",
      "C:\\RIGOR\\NETWORK",
      "C:\\RIGOR\\RELEASES",
      "C:\\RIGOR\\SOURCE",
      "C:\\RIGOR\\DOCUMENTS",
      "C:\\RIGOR\\DOWNLOADS",
    ],
    "computer",
  ),
  "C:\\RIGOR\\NODE": app(
    "C:\\RIGOR\\NODE",
    "Node",
    { kind: "node", title: "RIGOR Node", icon: "node", path: "C:\\RIGOR\\NODE" },
    "node",
    "Application",
  ),
  "C:\\RIGOR\\WALLET": app(
    "C:\\RIGOR\\WALLET",
    "Wallet",
    { kind: "wallet", title: "RIGOR Wallet", icon: "wallet", path: "C:\\RIGOR\\WALLET" },
    "wallet",
    "Application",
  ),
  "C:\\RIGOR\\MINER": app(
    "C:\\RIGOR\\MINER",
    "Miner",
    { kind: "miner", title: "RIGOR Miner", icon: "miner", path: "C:\\RIGOR\\MINER" },
    "miner",
    "Application",
  ),
  "C:\\RIGOR\\PROTOCOL": folder("C:\\RIGOR\\PROTOCOL", "Protocol", [
    "C:\\RIGOR\\PROTOCOL\\BLOCK.TXT",
    "C:\\RIGOR\\PROTOCOL\\CONSENSUS.TXT",
    "C:\\RIGOR\\PROTOCOL\\OPUHASH.TXT",
    "C:\\RIGOR\\PROTOCOL\\TRANSACTION.TXT",
    "C:\\RIGOR\\PROTOCOL\\NETWORK.TXT",
  ]),
  "C:\\RIGOR\\PROTOCOL\\BLOCK.TXT": textFile("C:\\RIGOR\\PROTOCOL\\BLOCK.TXT", "Block.txt", "block"),
  "C:\\RIGOR\\PROTOCOL\\CONSENSUS.TXT": textFile(
    "C:\\RIGOR\\PROTOCOL\\CONSENSUS.TXT",
    "Consensus.txt",
    "consensus",
  ),
  "C:\\RIGOR\\PROTOCOL\\OPUHASH.TXT": textFile("C:\\RIGOR\\PROTOCOL\\OPUHASH.TXT", "OpuHash.txt", "opuhash"),
  "C:\\RIGOR\\PROTOCOL\\TRANSACTION.TXT": textFile(
    "C:\\RIGOR\\PROTOCOL\\TRANSACTION.TXT",
    "Transaction.txt",
    "transaction",
  ),
  "C:\\RIGOR\\PROTOCOL\\NETWORK.TXT": textFile("C:\\RIGOR\\PROTOCOL\\NETWORK.TXT", "Network.txt", "network"),
  "C:\\RIGOR\\NETWORK": app(
    "C:\\RIGOR\\NETWORK",
    "Rigor Network",
    { kind: "control", title: "Control Panel", icon: "network", section: "network" },
    "network",
    "System Folder",
  ),
  "C:\\RIGOR\\RELEASES": app(
    "C:\\RIGOR\\RELEASES",
    "Releases",
    { kind: "downloads", title: "Downloads", icon: "downloads", path: "C:\\RIGOR\\DOWNLOADS" },
    "downloads",
    "System Folder",
  ),
  "C:\\RIGOR\\SOURCE": app(
    "C:\\RIGOR\\SOURCE",
    "Source",
    { kind: "source", title: "RIGOR Source", icon: "source", path: "C:\\RIGOR\\SOURCE" },
    "source",
    "Application",
  ),
  "C:\\RIGOR\\DOCUMENTS": folder(
    "C:\\RIGOR\\DOCUMENTS",
    "Documents",
    [
      "C:\\RIGOR\\DOCUMENTS\\WHITEPAPER.PDF",
      "C:\\RIGOR\\DOCUMENTS\\CONSENSUS.TXT",
      "C:\\RIGOR\\DOCUMENTS\\OPUHASH.TXT",
      "C:\\RIGOR\\DOCUMENTS\\BLOCK.TXT",
      "C:\\RIGOR\\DOCUMENTS\\TRANSACTION.TXT",
      "C:\\RIGOR\\DOCUMENTS\\NETWORK.TXT",
      "C:\\RIGOR\\DOCUMENTS\\SECURITY.TXT",
      "C:\\RIGOR\\DOCUMENTS\\FAQ.TXT",
    ],
    "documents",
  ),
  "C:\\RIGOR\\DOCUMENTS\\WHITEPAPER.PDF": pdfFile(
    "C:\\RIGOR\\DOCUMENTS\\WHITEPAPER.PDF",
    "Whitepaper.pdf",
    "whitepaper",
  ),
  "C:\\RIGOR\\DOCUMENTS\\CONSENSUS.TXT": textFile(
    "C:\\RIGOR\\DOCUMENTS\\CONSENSUS.TXT",
    "Consensus.txt",
    "consensus",
  ),
  "C:\\RIGOR\\DOCUMENTS\\OPUHASH.TXT": textFile("C:\\RIGOR\\DOCUMENTS\\OPUHASH.TXT", "OpuHash.txt", "opuhash"),
  "C:\\RIGOR\\DOCUMENTS\\BLOCK.TXT": textFile("C:\\RIGOR\\DOCUMENTS\\BLOCK.TXT", "Block.txt", "block"),
  "C:\\RIGOR\\DOCUMENTS\\TRANSACTION.TXT": textFile(
    "C:\\RIGOR\\DOCUMENTS\\TRANSACTION.TXT",
    "Transaction.txt",
    "transaction",
  ),
  "C:\\RIGOR\\DOCUMENTS\\NETWORK.TXT": textFile("C:\\RIGOR\\DOCUMENTS\\NETWORK.TXT", "Network.txt", "network"),
  "C:\\RIGOR\\DOCUMENTS\\SECURITY.TXT": textFile(
    "C:\\RIGOR\\DOCUMENTS\\SECURITY.TXT",
    "Security.txt",
    "security",
  ),
  "C:\\RIGOR\\DOCUMENTS\\FAQ.TXT": textFile("C:\\RIGOR\\DOCUMENTS\\FAQ.TXT", "FAQ.txt", "faq"),
  "C:\\RIGOR\\DOWNLOADS": folder(
    "C:\\RIGOR\\DOWNLOADS",
    "Downloads",
    [
      "C:\\RIGOR\\DOWNLOADS\\NODE",
      "C:\\RIGOR\\DOWNLOADS\\WALLET",
      "C:\\RIGOR\\DOWNLOADS\\MINER",
      "C:\\RIGOR\\DOWNLOADS\\SOURCE",
      "C:\\RIGOR\\DOWNLOADS\\CHECKSUMS.TXT",
      "C:\\RIGOR\\DOWNLOADS\\RELEASE_NOTES.TXT",
    ],
    "downloads",
  ),
  "C:\\RIGOR\\DOWNLOADS\\NODE": app(
    "C:\\RIGOR\\DOWNLOADS\\NODE",
    "RIGOR Node",
    { kind: "node", title: "RIGOR Node", icon: "node" },
    "node",
    "Application",
  ),
  "C:\\RIGOR\\DOWNLOADS\\WALLET": app(
    "C:\\RIGOR\\DOWNLOADS\\WALLET",
    "RIGOR Wallet",
    { kind: "wallet", title: "RIGOR Wallet", icon: "wallet" },
    "wallet",
    "Application",
  ),
  "C:\\RIGOR\\DOWNLOADS\\MINER": app(
    "C:\\RIGOR\\DOWNLOADS\\MINER",
    "RIGOR Miner",
    { kind: "miner", title: "RIGOR Miner", icon: "miner" },
    "miner",
    "Application",
  ),
  "C:\\RIGOR\\DOWNLOADS\\SOURCE": app(
    "C:\\RIGOR\\DOWNLOADS\\SOURCE",
    "Source Code",
    { kind: "source", title: "RIGOR Source", icon: "source" },
    "source",
    "Application",
  ),
  "C:\\RIGOR\\DOWNLOADS\\CHECKSUMS.TXT": textFile(
    "C:\\RIGOR\\DOWNLOADS\\CHECKSUMS.TXT",
    "Checksums.txt",
    "checksums",
  ),
  "C:\\RIGOR\\DOWNLOADS\\RELEASE_NOTES.TXT": textFile(
    "C:\\RIGOR\\DOWNLOADS\\RELEASE_NOTES.TXT",
    "Release Notes.txt",
    "release_notes",
  ),
};

export type DeskIcon = { id: string; name: string; icon: IconName; open: AppOpen };

const ICON = {
  computer: {
    id: "my-computer",
    name: "My Computer",
    icon: "computer" as const,
    open: { kind: "folder" as const, title: "My Computer", icon: "computer" as const, path: "C:\\RIGOR" },
  },
  readme: {
    id: "readme",
    name: "Readme.txt",
    icon: "text" as const,
    open: { kind: "text" as const, title: "Readme.txt - Notepad", icon: "text" as const, docId: "readme" },
  },
  documents: {
    id: "documents",
    name: "Documents",
    icon: "documents" as const,
    open: {
      kind: "folder" as const,
      title: "Documents",
      icon: "documents" as const,
      path: "C:\\RIGOR\\DOCUMENTS",
    },
  },
  control: {
    id: "control",
    name: "Control Panel",
    icon: "control" as const,
    open: { kind: "control" as const, title: "Control Panel", icon: "control" as const },
  },
  recycle: {
    id: "recycle",
    name: "Recycle Bin",
    icon: "recycle" as const,
    open: { kind: "recycle" as const, title: "Recycle Bin", icon: "recycle" as const },
  },
  network: {
    id: "network",
    name: "Rigor Network",
    icon: "network" as const,
    open: { kind: "control" as const, title: "Control Panel", icon: "network" as const, section: "network" },
  },
  browser: {
    id: "browser",
    name: "Rigor Browser",
    icon: "explorer" as const,
    open: { kind: "browser" as const, title: "Rigor Browser", icon: "explorer" as const },
  },
  miner: {
    id: "miner",
    name: "Miner",
    icon: "miner" as const,
    open: { kind: "miner" as const, title: "RIGOR Miner", icon: "miner" as const },
  },
  downloads: {
    id: "downloads",
    name: "Downloads",
    icon: "downloads" as const,
    open: {
      kind: "downloads" as const,
      title: "Downloads",
      icon: "downloads" as const,
      path: "C:\\RIGOR\\DOWNLOADS",
    },
  },
  source: {
    id: "source",
    name: "Source",
    icon: "source" as const,
    open: { kind: "source" as const, title: "RIGOR Source", icon: "source" as const },
  },
};

export const DESKTOP_COLUMNS: { id: string; icons: DeskIcon[] }[] = [
  { id: "system", icons: [ICON.computer, ICON.readme, ICON.documents, ICON.control, ICON.recycle] },
  { id: "network", icons: [ICON.network, ICON.browser, ICON.miner, ICON.downloads] },
  { id: "developer", icons: [ICON.source] },
];

export const DESKTOP_ICONS: DeskIcon[] = DESKTOP_COLUMNS.flatMap((c) => c.icons);

export function childrenOf(path: string): FsNode[] {
  const node = FS[path];
  if (!node?.children) return [];
  return node.children.map((id) => FS[id]).filter(Boolean);
}

export function parentPath(path: string): string | null {
  if (path === "C:\\" || path === "C:\\RIGOR") return path === "C:\\RIGOR" ? "C:\\" : null;
  const i = path.lastIndexOf("\\");
  if (i <= 2) return "C:\\";
  return path.slice(0, i);
}

export const OPENS = {
  computer: ICON.computer.open,
  readme: ICON.readme.open,
  documents: ICON.documents.open,
  browser: ICON.browser.open,
  miner: ICON.miner.open,
  downloads: ICON.downloads.open,
  control: ICON.control.open,
  recycle: ICON.recycle.open,
  wallet: { kind: "wallet" as const, title: "RIGOR Wallet", icon: "wallet" as const },
  source: ICON.source.open,
  about: { kind: "about" as const, title: "About RIGOR", icon: "help" as const },
  network: ICON.network.open,
  protocol: { kind: "folder" as const, title: "Protocol", icon: "folder" as const, path: "C:\\RIGOR\\PROTOCOL" },
  shutdown: { kind: "shutdown" as const, title: "Shut Down RIGOR", icon: "computer" as const },
};
