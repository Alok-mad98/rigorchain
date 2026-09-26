/**
 * Official RIGOR website information architecture.
 * Used by navigation, hub pages, and placeholders.
 * Phase 1 Step 4: primary navigation and section SubNav data.
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
  { to: "/protocol", label: "Protocol" },
  { to: "/documentation", label: "Documentation" },
  { to: "/use", label: "Use" },
  { to: "/explorer", label: "Explorer" },
] as const;

export const SECTIONS: SiteSection[] = [
  {
    id: "protocol",
    path: "/protocol",
    title: "Protocol",
    children: [
      {
        path: "/protocol/overview",
        title: "Overview",
        purpose: "Protocol identity and network constants.",
        preservation: "KEEP",
      },
      {
        path: "/machine-readable-protocol",
        title: "Machine-Verifiable Protocol",
        purpose:
          "Why the protocol is suited to machine verification, machine computation, and machine participation.",
        preservation: "KEEP",
      },
      {
        path: "/protocol/architecture",
        title: "Architecture",
        purpose: "Public protocol architecture overview.",
        preservation: "KEEP",
      },
      {
        path: "/protocol/consensus",
        title: "Consensus",
        purpose: "Consensus index. Not a specification.",
        preservation: "KEEP",
      },
      {
        path: "/protocol/assets",
        title: "Native Assets",
        purpose: "ORU and RES monetary roles.",
        preservation: "KEEP",
      },
      {
        path: "/protocol/script-as-asset",
        title: "Script-as-Asset",
        purpose: "Script-defined asset identity.",
        preservation: "KEEP",
      },
      {
        path: "/protocol/convergence",
        title: "Convergence",
        purpose: "Convergence protocol entry.",
        preservation: "KEEP",
      },
      {
        path: "/protocol/proof-of-work",
        title: "Proof-of-Work",
        purpose: "OpuHash and ASERT. RES is not a PoW reward.",
        preservation: "KEEP",
      },
      {
        path: "/protocol/verification-boundary",
        title: "Formal Verification",
        purpose: "Public verification overview of selected consensus properties.",
        preservation: "KEEP",
      },
    ],
  },
  {
    id: "documentation",
    path: "/documentation",
    title: "Documentation",
    children: [
      {
        path: "/documentation/architecture",
        title: "Architecture",
        purpose: "Implementation and technical architecture documentation entry.",
        preservation: "PLACEHOLDER",
      },
      {
        path: "/documentation/protocol",
        title: "Protocol",
        purpose: "RFC and specification document index.",
        preservation: "PLACEHOLDER",
      },
      {
        path: "/documentation/developer",
        title: "Developer",
        purpose: "Developer resources.",
        preservation: "PLACEHOLDER",
      },
      {
        path: "/documentation/verification",
        title: "Verification",
        purpose: "Verification resources. Links to /protocol/verification-boundary.",
        preservation: "KEEP",
      },
      {
        path: "/documentation/security",
        title: "Security",
        purpose: "Security architecture documentation.",
        preservation: "KEEP",
      },
      {
        path: "/documentation/user",
        title: "User",
        purpose: "User guides.",
        preservation: "PLACEHOLDER",
      },
    ],
  },
  {
    id: "use",
    path: "/use",
    title: "Use",
    children: [
      {
        path: "/use/download",
        title: "Download",
        purpose: "Wallet, node, and miner packages.",
        preservation: "KEEP",
      },
      {
        path: "/use/wallet",
        title: "Wallet",
        purpose: "Spending tool, not a consensus authority.",
        preservation: "KEEP",
      },
      {
        path: "/use/faucet",
        title: "Faucet",
        purpose:
          "TESTNET user/product service. Explainer only. Claim UI remains at /faucet/. Not a protocol rule.",
        preservation: "KEEP",
      },
      {
        path: "/use/mining",
        title: "Mining",
        purpose: "How to mine: SOLO GBT, miner download, operator instructions.",
        preservation: "KEEP",
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
        title: "Blocks",
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
        title: "RES",
        purpose: "Indexed RES amounts and mint events.",
        preservation: "PLACEHOLDER",
        explorerPath: "/res.html",
      },
      {
        path: "/explorer/status",
        title: "Status",
        purpose: "Explorer status dashboard (height, RPC, indexer).",
        preservation: "PLACEHOLDER",
        explorerPath: "/",
      },
    ],
  },
];

export function sectionByPath(pathname: string): SiteSection | undefined {
  const prefixed = SECTIONS.find(
    (s) => pathname === s.path || pathname.startsWith(`${s.path}/`),
  );
  if (prefixed) return prefixed;
  return SECTIONS.find((s) =>
    s.children.some((c) => pathname === c.path || pathname.startsWith(`${c.path}/`)),
  );
}

export function nodeByPath(pathname: string): SiteNode | undefined {
  for (const section of SECTIONS) {
    const hit = section.children.find((c) => c.path === pathname);
    if (hit) return hit;
  }
  return undefined;
}
