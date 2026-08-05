import { describe, it } from "node:test";
import assert from "node:assert/strict";
import type { Request } from "express";
import { wantsHtmlPage } from "./registerRoutes";

function mockReq(overrides: { headers?: Record<string, string>; query?: Record<string, string> }): Request {
  const headers = overrides.headers ?? {};
  const query = overrides.query ?? {};
  return {
    headers,
    query,
    get(name: string) {
      return headers[name.toLowerCase()];
    },
  } as unknown as Request;
}

describe("address canonical routing", () => {
  it("serves HTML for browser navigation to /address/{bech32}", () => {
    const req = mockReq({
      headers: { "sec-fetch-mode": "navigate", accept: "text/html" },
    });
    assert.equal(wantsHtmlPage(req), true);
  });

  it("serves JSON when page/limit query params present", () => {
    const req = mockReq({
      headers: { accept: "text/html,application/json" },
      query: { page: "2", limit: "50" },
    });
    assert.equal(wantsHtmlPage(req), false);
  });

  it("serves JSON when format=json", () => {
    const req = mockReq({
      headers: { "sec-fetch-mode": "navigate" },
      query: { format: "json" },
    });
    assert.equal(wantsHtmlPage(req), false);
  });
});
