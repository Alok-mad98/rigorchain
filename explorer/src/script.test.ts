import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { classifyScriptPubKey } from "./script";

describe("scriptPubKey address encoding", () => {
  it("encodes AlphaPlus mainnet P2WPKH as rg1q…", () => {
    const hex = "001409ba7bdc9a1c2f6114431e483f26c1de0271893c";
    const cls = classifyScriptPubKey(hex, "rg");
    assert.equal(cls.assetKind, "ORU");
    assert.equal(cls.witnessVersion, 0);
    assert.equal(cls.address, "rg1qpxa8hhy6rshkz9zrreyr7fkpmcp8rzful9s3kn");
  });

  it("encodes the same program on regtest as trg1q…", () => {
    const hex = "001409ba7bdc9a1c2f6114431e483f26c1de0271893c";
    const cls = classifyScriptPubKey(hex, "trg");
    assert.equal(cls.assetKind, "ORU");
    assert.ok(cls.address?.startsWith("trg1q"));
  });
});
