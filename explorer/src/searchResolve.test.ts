import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  HASH_NOT_INDEXED,
  canonicalUrlForResolution,
  classifySearchInput,
  parseBlockHeightQuery,
  parseBlockRouteParam,
  resolveSearchInputWith,
  type SearchLookups,
} from "./searchResolve";

const TX = "a".repeat(64);
const BLOCK = "b".repeat(64);
const WTXID = "c".repeat(64);
const UNKNOWN = "d".repeat(64);

function lookups(overrides: Partial<SearchLookups> = {}): SearchLookups {
  return {
    getTxByTxid: async () => null,
    getBlockByHash: async () => null,
    getTxidByWtxid: async () => null,
    getBlockHashByHeight: async () => null,
    ...overrides,
  };
}

describe("parseBlockRouteParam", () => {
  it("accepts a 64-char hash case-insensitively", () => {
    assert.deepEqual(parseBlockRouteParam(BLOCK.toUpperCase()), { kind: "hash", hash: BLOCK });
  });

  it("accepts a decimal height", () => {
    assert.deepEqual(parseBlockRouteParam("1777"), { kind: "height", height: "1777" });
  });

  it("rejects empty and non-keys", () => {
    assert.equal(parseBlockRouteParam(""), null);
    assert.equal(parseBlockRouteParam("rg1qqqq"), null);
  });
});

describe("parseBlockHeightQuery", () => {
  it("accepts 1–12 digits only", () => {
    assert.equal(parseBlockHeightQuery("0"), "0");
    assert.equal(parseBlockHeightQuery("1777"), "1777");
    assert.equal(parseBlockHeightQuery("12".repeat(7)), null);
  });
});

describe("classifySearchInput", () => {
  it("does not pretreat 64-hex as a tx", () => {
    assert.equal(classifySearchInput(TX), null);
  });

  it("rejects unrecognized text", () => {
    const r = classifySearchInput("not-a-query!!!");
    assert.equal(r?.kind, "error");
  });
});

describe("resolveSearchInputWith", () => {
  it("prefers an indexed txid over a block hash", async () => {
    const r = await resolveSearchInputWith(
      lookups({
        getTxByTxid: async (h) => (h === TX ? { txid: TX } : null),
        getBlockByHash: async (h) => (h === TX ? { block: { block_hash: TX } } : null),
      }),
      TX,
    );
    assert.deepEqual(r, { kind: "tx", txid: TX });
  });

  it("resolves an indexed block hash when it is not a txid", async () => {
    const r = await resolveSearchInputWith(
      lookups({
        getBlockByHash: async (h) => (h === BLOCK ? { block: { block_hash: BLOCK } } : null),
      }),
      BLOCK,
    );
    assert.equal(r.kind, "block");
    if (r.kind === "block") assert.equal(r.hash, BLOCK);
  });

  it("does not invent a tx route for an unknown or orphaned hash", async () => {
    const r = await resolveSearchInputWith(lookups(), UNKNOWN);
    assert.deepEqual(r, { kind: "error", message: HASH_NOT_INDEXED });
    assert.equal(canonicalUrlForResolution("http://127.0.0.1:8080", r), null);
  });

  it("rejects wtxid without redirecting to /tx/{wtxid}", async () => {
    const r = await resolveSearchInputWith(
      lookups({
        getTxidByWtxid: async (h) => (h === WTXID ? TX : null),
      }),
      WTXID,
    );
    assert.equal(r.kind, "error");
    if (r.kind === "error") {
      assert.match(r.message, /wtxid is informational only/);
      assert.match(r.message, new RegExp(TX));
    }
    assert.equal(canonicalUrlForResolution("", r), null);
  });

  it("resolves a height to the canonical block-hash URL", async () => {
    const r = await resolveSearchInputWith(
      lookups({
        getBlockHashByHeight: async (h) => (h === "1777" ? BLOCK : null),
        getBlockByHash: async (h) => (h === BLOCK ? { block: { block_hash: BLOCK } } : null),
      }),
      "1777",
    );
    assert.equal(r.kind, "block");
    assert.equal(canonicalUrlForResolution("", r), `/block/${BLOCK}`);
  });
});
