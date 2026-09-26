export const GITHUB_REPO = "https://github.com/rigorchain/rigorchain";
export const GITHUB_RELEASES = "https://github.com/rigorchain/rigorchain/releases";
export const GITHUB_API_RELEASES = "https://api.github.com/repos/rigorchain/rigorchain/releases";
export const GITHUB_WHITEPAPER =
  "https://github.com/rigorchain/rigorchain/blob/main/docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md";
export const GITHUB_DOCS = "https://github.com/rigorchain/rigorchain/tree/main/docs";
export const GITHUB_CORE = "https://github.com/rigorchain/rigorchain/tree/main/core";
export const GITHUB_WALLET = "https://github.com/rigorchain/rigorchain/tree/main/core";
export const GITHUB_MINER = "https://github.com/rigorchain/rigorchain/tree/main/miner";
export const GITHUB_EXPLORER = "https://github.com/rigorchain/rigorchain/tree/main/explorer";

export const EXPLORER_HOST = "explorer.rigorchain.com";
export const EXPLORER_ORIGIN = "http://explorer.rigorchain.com";
export const EXPLORER_HOME = `${EXPLORER_ORIGIN}/`;

export const NETWORK_NAME = "RIGOR MAINNET";

export function isMainnetAssetName(name: string): boolean {
  const n = name.toLowerCase();
  return !/(testnet|test-net|sandbox|regtest|devtest)/.test(n);
}
