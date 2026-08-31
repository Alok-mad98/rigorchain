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
  mode?: "tx" | "block";
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
  { to: "/wallet", label: "Wallet" },
  { to: "/mining", label: "Mining" },
  { to: "/explorer", label: "Explorer" },
  { to: "/documentation", label: "Documentation" },
  { to: "/zero-trust-verification", label: "Zero-Trust" },
  { to: "/about", label: "About" },
] as const;

export const LAB_NAV = [
  { to: "/protocol", label: "Protocol" },
  { to: "/science", label: "Science" },
  { to: "/lab", label: "Lab" },
  { to: "/status", label: "Status" },
  { to: "/experiment", label: "PROOF-034" },
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
        purpose: "ORU as execution and fee resource; subsidy floor 125.",
        preservation: "WIRED",
      },
      {
        path: "/wallet/maturity",
        title: "Maturity",
        purpose: "How confirmation and maturity are shown to users.",
        preservation: "PLACEHOLDER",
        planned: ["Confirmation rules", "When a reward is spendable", "Wallet UX mapping"],
      },
      {
        path: "/wallet/convergence",
        title: "Convergence",
        purpose: "Deterministic selection over candidate set F; live observer panel.",
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
        title: "Quarantined pool (not the test path)",
        purpose: "Optional read-only Alpha pool observer. Not the shipped SOLO mining path.",
        preservation: "PLACEHOLDER",
        planned: [
          "Not used for test-phase mining",
          "Hashrate / workers (display only)",
          "Not a consensus authority",
        ],
      },
      {
        path: "/mining/economics",
        title: "Mining Economics",
        purpose: "ORU subsidy schedule and why RES is not mined.",
        preservation: "PLACEHOLDER",
        planned: ["1000 → 500 → 250 → 125", "No premine", "RES via Convergence only"],
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
    id: "zero-trust",
    path: "/zero-trust-verification",
    title: "Zero-Trust Verification",
    children: [
      {
        path: "/zero-trust-verification/what-is-zero-trust",
        title: "What is Zero Trust?",
        purpose: "You do not need to trust a node. You can independently verify the result yourself.",
        preservation: "KEEP",
      },
      {
        path: "/zero-trust-verification/transaction",
        title: "Verify Transaction",
        purpose: "Browser recomputes TX consensus outputs from a TXID.",
        preservation: "KEEP",
        mode: "tx",
      },
      {
        path: "/zero-trust-verification/block",
        title: "Verify Block",
        purpose: "Browser recomputes block genesis, parent, ConvergenceRoot, winner.",
        preservation: "KEEP",
        mode: "block",
      },
      {
        path: "/zero-trust-verification/fingerprint",
        title: "Consensus Fingerprint",
        purpose: "Explain observational fingerprint vs locally computed ConvergenceRoot.",
        preservation: "PLACEHOLDER",
        planned: ["Input: block hash", "Local ConvergenceRoot", "Explorer fingerprint is not authority"],
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
        purpose: "Official RIGOR protocol whitepaper: dual assets, Convergence, fairness, mining, verification.",
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
        purpose: "Coq model, correspondence boundary, differential gate. equivalence_claimed = false.",
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
        planned: ["Repo map", "Read-only RPC", "zeroTrust library", "No consensus edits from this site"],
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
        purpose: "Index of frozen RFCs that define RIGOR.",
        preservation: "PLACEHOLDER",
        planned: ["RFC-000 constitution", "RFC-015 consensus model", "RFC-008 mathematics"],
      },
      {
        path: "/documentation/security",
        title: "Security Architecture",
        purpose: "Security through minimalism: UTXO verification, dual native assets, Zero Trust.",
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
