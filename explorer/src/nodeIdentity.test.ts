import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { formatNodeProduct, parseNodeReleaseIdentity } from "./nodeIdentity";

const SAMPLE = `RIGOR version v0.2
Production Release
BUILD_GIT_COMMIT=763e1ae84aa0
NETWORK=mainnet
`;

describe("node release identity", () => {
  it("parses FormatReleaseIdentity banner", () => {
    const id = parseNodeReleaseIdentity(SAMPLE);
    assert.deepEqual(id, {
      product: "RIGOR v0.2",
      release_version: "v0.2",
      codename: "Production Release",
      build_git_commit: "763e1ae84aa0",
      network: "mainnet",
    });
  });

  it("returns null when the banner is not a release identity", () => {
    assert.equal(parseNodeReleaseIdentity("error: could not locate RPC credentials"), null);
    assert.equal(parseNodeReleaseIdentity(""), null);
  });

  it("prefers the node banner over subversion and the static fallback", () => {
    const id = parseNodeReleaseIdentity(SAMPLE);
    assert.equal(formatNodeProduct(id, "/RigorCore:25.0.0/", "RIGOR v0.1.3"), "RIGOR v0.2");
    assert.equal(formatNodeProduct(null, "/RigorCore:25.0.0/", "RIGOR v0.2"), "/RigorCore:25.0.0/");
    assert.equal(formatNodeProduct(null, null, "RIGOR v0.2"), "RIGOR v0.2");
  });
});
