/**
 * Official RIGOR website information architecture.
 * Used by navigation, hub pages, and placeholders.
 * Existing live pages are KEEP. New nested routes are PLACEHOLDER until filled.
 */

export type Preservation = "KEEP" | "PLACEHOLDER" | "WIRED";

export type SiteNode = {
  path: string;
  title: string;
  purpose: string;
  preservation: Preservation;
  planned?: string[];
  explorerPath?: string;
};

export type SiteSection = {
  id: string;
  path: string;
  title: string;
  children: SiteNode[];
};

export const PRIMARY_NAV = [
  { to: "/", label: "Home" },
  { to: "/download", label: "Download" },
  { to: "/mining", label: "Mining" },
  { to: "/explorer", label: "Explorer" },
  { to: "/documentation", label: "Documentation" },
  { to: "/about", label: "About" },
] as const;

export const SECTIONS: SiteSection[] = [
  {
    id: "download",
    path: "/download",
    title: "Download",
    children: [
      {
        path: "/download/windows-wallet",
        title: "Windows Wallet",
        purpose: "Official Windows Qt wallet installer entry.",
        preservation: "PLACEHOLDER",
        planned: ["Checksummed Windows installer", "SHA256 verification steps", "First-run link to Wallet"],
      },
      {
        path: "/download/linux-wallet",
        title: "Linux Wallet",
        purpose: "Official Linux wallet / node package entry.",
        preservation: "PLACEHOLDER",
        planned: ["tarball / package links", "Checksum verification", "CLI companion"],
      },
      {
        path: "/download/cli",
        title: "CLI Tools",
        purpose: "rigord and rigor-cli download and usage entry.",
        preservation: "PLACEHOLDER",
        planned: ["rigord", "rigor-cli", "getblockhash / gettransaction examples"],
      },
      {
        path: "/download/miner",
        title: "Miner",
        purpose: "Windows and Linux miner artifacts for RIGOR PoW.",
        preservation: "PLACEHOLDER",
        planned: ["Windows miner package", "Pool connection defaults", "Link to Mining guide"],
      },
    ],
  },
  {
    id: "wallet",
    path: "/wallet",
    title: "Wallet",
    children: [
      {
        path: "/wallet/overview",
        title: "Wallet Overview",
        purpose: "Spending tool, not a consensus authority. How to obtain a TXID.",
        preservation: "KEEP",
      },
      {
        path: "/wallet/oru",
        title: "ORU",
        purpose: "ORU as execution and fee resource; R(0)=0 then 128 ORU/block forever.",
        preservation: "WIRED",
      },
      {
        path: "/wallet/maturity",
        title: "Maturity",
        purpose: "How confirmation and maturity are shown to users.",
        preservation: "KEEP",
      },
      {
        path: "/wallet/convergence",
        title: "Convergence",
        purpose: "Destroy ORU to mint RES at confirming height; live getconvergencestatus panel.",
        preservation: "WIRED",
      },
      {
        path: "/wallet/res",
        title: "RES",
        purpose: "RES mint lifecycle as a distinct native asset.",
        preservation: "WIRED",
      },
    ],
  },
  {
    id: "mining",
    path: "/mining",
    title: "Mining",
    children: [
      {
        path: "/mining/overview",
        title: "Mining Overview",
        purpose: "SOLO GBT against local rigord. RES allocation is not a pool policy.",
        preservation: "KEEP",
      },
      {
        path: "/mining/windows",
        title: "Windows SOLO Guide",
        purpose: "How a new miner starts on Windows: local GBT and submitblock.",
        preservation: "PLACEHOLDER",
        planned: [
          "Download miner",
          "Wallet rg1q address",
          "Local rigord getblocktemplate",
          "submitblock",
          "Verify a found block",
        ],
      },
      {
        path: "/mining/pool",
        title: "Quarantined pool (not the SOLO path)",
        purpose: "Optional read-only pool observer. Not the shipped SOLO mining path.",
        preservation: "PLACEHOLDER",
        planned: [
          "Not used for SOLO mining",
          "Hashrate / workers (display only)",
          "Not a consensus authority",
        ],
      },
      {
        path: "/mining/economics",
        title: "Mining Economics",
        purpose: "ORU subsidy schedule and why RES is not mined.",
        preservation: "PLACEHOLDER",
        planned: ["128 ORU/block forever", "No premine", "RES via Convergence only"],
      },
    ],
  },
  {
    id: "explorer",
    path: "/explorer",
    title: "Explorer",
    children: [
      {
        path: "/explorer/blocks",
        title: "Block Explorer",
        purpose: "Viewer for blocks. Not a validator.",
        preservation: "PLACEHOLDER",
        explorerPath: "/blocks",
      },
      {
        path: "/explorer/oru-supply",
        title: "ORU Supply",
        purpose: "Indexed ORU amounts. Defer to rigord on mismatch.",
        preservation: "PLACEHOLDER",
        explorerPath: "/oru.html",
      },
      {
        path: "/explorer/res",
        title: "RES Statistics",
        purpose: "Indexed RES amounts and mint events.",
        preservation: "PLACEHOLDER",
        explorerPath: "/res.html",
      },
      {
        path: "/explorer/fingerprint",
        title: "Consensus Fingerprint",
        purpose: "Observational Draw Fingerprint display — not a verdict.",
        preservation: "PLACEHOLDER",
        explorerPath: "/",
      },
      {
        path: "/explorer/status",
        title: "Network Status",
        purpose: "Explorer status dashboard (height, RPC, indexer).",
        preservation: "PLACEHOLDER",
        explorerPath: "/",
      },
    ],
  },
  {
    id: "documentation",
    path: "/documentation",
    title: "Documentation",
    children: [
      {
        path: "/documentation/whitepaper",
        title: "Whitepaper",
        purpose:
          "Official RIGOR protocol whitepaper: issuance, dual assets, Coq kernel boundary.",
        preservation: "KEEP",
      },
      {
        path: "/documentation/monetary-policy",
        title: "Deterministic Reserve Issuance",
        purpose:
          "RES schedule W(k), supply bound, cost floor. RES is not mined. equivalence_claimed = false.",
        preservation: "KEEP",
      },
      {
        path: "/documentation/formal-verification",
        title: "Formal Verification",
        purpose:
          "Formally verified critical consensus invariants inside a Coq kernel model. cpp_equivalence = false. Maturity 4.9.",
        preservation: "KEEP",
      },
      {
        path: "/documentation/user-guide",
        title: "User Guide",
        purpose: "How to download, run a wallet, mine, and verify.",
        preservation: "PLACEHOLDER",
        planned: ["New user path", "Wallet first-run", "Mining start", "Independent verify"],
      },
      {
        path: "/documentation/developer-guide",
        title: "Developer Guide",
        purpose: "Build, RPC, explorer API, verifier library.",
        preservation: "PLACEHOLDER",
        planned: ["Repo map", "Read-only RPC", "No consensus edits from this site"],
      },
      {
        path: "/documentation/architecture",
        title: "Architecture",
        purpose: "Three-layer model and official portal structure.",
        preservation: "PLACEHOLDER",
        planned: ["docs/architecture/ARCHITECTURE.md", "Website IA", "Explorer vs verifier split"],
      },
      {
        path: "/documentation/rfc",
        title: "RFC Overview",
        purpose: "Index of explanatory RFCs. Production consensus is core/src/.",
        preservation: "PLACEHOLDER",
        planned: ["RFC-020 issuance", "RFC-000 constitution", "RFC-008 mathematics", "historical RFC-001 / RFC-014 / RFC-017 / RFC-019"],
      },
      {
        path: "/documentation/security",
        title: "Security Architecture",
        purpose: "Security through minimalism: UTXO verification and dual native assets.",
        preservation: "KEEP",
      },
      {
        path: "/documentation/security/minimal-script-architecture",
        title: "Minimal Script Architecture",
        purpose:
          "Bitcoin UTXO foundation, script-defined asset identity, restricted witness families. Not a programmable VM.",
        preservation: "KEEP",
      },
    ],
  },
];

export function sectionByPath(pathname: string): SiteSection | undefined {
  return SECTIONS.find(
    (s) => pathname === s.path || pathname.startsWith(`${s.path}/`),
  );
}

export function nodeByPath(pathname: string): SiteNode | undefined {
  for (const section of SECTIONS) {
    const hit = section.children.find((c) => c.path === pathname);
    if (hit) return hit;
  }
  return undefined;
}
