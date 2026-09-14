import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { blockListPageMeta, parseBlockListQuery } from "./blocksList";

describe("block list pagination", () => {
  it("defaults to page 1 of 50", () => {
    assert.deepEqual(parseBlockListQuery({}), { limit: 50, page: 1, offset: 0 });
  });

  it("caps limit at 200 and floors page to 1", () => {
    assert.equal(parseBlockListQuery({ limit: 999 }).limit, 200);
    assert.equal(parseBlockListQuery({ page: 0 }).page, 1);
    assert.equal(parseBlockListQuery({ page: 3, limit: 50 }).offset, 100);
  });

  it("computes has_next / has_prev from total", () => {
    const q = parseBlockListQuery({ page: 2, limit: 50 });
    const meta = blockListPageMeta(q, 120);
    assert.equal(meta.pages, 3);
    assert.equal(meta.has_prev, true);
    assert.equal(meta.has_next, true);
    assert.equal(blockListPageMeta(parseBlockListQuery({ page: 1 }), 40).has_next, false);
  });
});
