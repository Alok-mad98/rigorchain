/**
 * Node release identity. Display only — not a consensus parameter.
 *
 * Product version comes from the node binary (`rigor-cli -version`),
 * which prints FormatReleaseIdentity(). getnetworkinfo.subversion is
 * the packed CLIENT_VERSION string (/RigorCore:25.0.0/), not the
 * release tag.
 */

export type NodeReleaseIdentity = {
  product: string;
  release_version: string;
  codename: string | null;
  build_git_commit: string | null;
  network: string | null;
};

const VERSION_LINE = /^RIGOR version\s+(\S+)\s*$/m;
const COMMIT_LINE = /^BUILD_GIT_COMMIT=(\S+)\s*$/m;
const NETWORK_LINE = /^NETWORK=(\S+)\s*$/m;

export function parseNodeReleaseIdentity(banner: string): NodeReleaseIdentity | null {
  const versionMatch = banner.match(VERSION_LINE);
  if (!versionMatch) return null;
  const release_version = versionMatch[1];
  const commitMatch = banner.match(COMMIT_LINE);
  const networkMatch = banner.match(NETWORK_LINE);
  const lines = banner
    .split(/\r?\n/)
    .map((l) => l.trim())
    .filter((l) => l.length > 0);
  const versionLineIndex = lines.findIndex((l) => VERSION_LINE.test(l));
  const next = versionLineIndex >= 0 ? lines[versionLineIndex + 1] : undefined;
  const codename =
    next && !next.startsWith("BUILD_GIT_COMMIT=") && !next.startsWith("NETWORK=") ? next : null;
  return {
    product: `RIGOR ${release_version}`,
    release_version,
    codename,
    build_git_commit: commitMatch ? commitMatch[1] : null,
    network: networkMatch ? networkMatch[1] : null,
  };
}

/** Prefer the node release banner; then getnetworkinfo.subversion; then a static fallback. */
export function formatNodeProduct(
  identity: NodeReleaseIdentity | null,
  subversion?: string | null,
  fallback?: string,
): string {
  if (identity?.product) return identity.product;
  const sub = typeof subversion === "string" ? subversion.trim() : "";
  if (sub) return sub;
  return fallback ?? "RIGOR";
}
