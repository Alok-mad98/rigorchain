import type { IconName } from "./types";

const ICON_FILE: Record<IconName, string> = {
  computer: "/icons/my-computer.png",
  folder: "/icons/folder.png",
  documents: "/icons/documents.png",
  explorer: "/icons/browser.png",
  miner: "/icons/miner.png",
  downloads: "/icons/downloads.png",
  control: "/icons/control-panel.png",
  recycle: "/icons/recycle-bin.png",
  text: "/icons/readme.png",
  pdf: "/icons/pdf.png",
  network: "/icons/network.png",
  source: "/icons/source.png",
  wallet: "/icons/wallet.png",
  node: "/icons/node.png",
  help: "/icons/help.png",
  start: "/icons/start.png",
};

export function PixelIcon({
  name,
  size = 32,
  className,
}: {
  name: IconName;
  size?: number;
  className?: string;
}) {
  return (
    <img
      src={ICON_FILE[name]}
      width={size}
      height={size}
      className={className}
      alt=""
      draggable={false}
      style={{ imageRendering: name === "start" ? "auto" : "pixelated" }}
    />
  );
}
