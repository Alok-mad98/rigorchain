import { bytesToHex, hexToBytes } from "./bytes";
import { computeConvergenceRoot, computeSigma } from "./consensus";
import { RIGOR_GENESIS_HASH_DISPLAY } from "./constants";
import { hash256 } from "./hash";
import { hexEqual } from "./format";

export type SelfCheckRow = { name: string; ok: boolean; detail: string };

/**
 * Stateless primitive checks that run entirely in the browser.
 * hash256("hello") matches core/src/test/canonical_layer_tests.cpp (digest order).
 */
export async function runSelfCheck(): Promise<SelfCheckRow[]> {
  const rows: SelfCheckRow[] = [];

  const hello = await hash256(new TextEncoder().encode("hello"));
  const helloHex = bytesToHex(hello);
  const helloOk = helloHex === "9595c9df90075148eb06860365df33584b75bff782a510c6cd4883a419833d50";
  rows.push({
    name: "hash256(\"hello\")",
    ok: helloOk,
    detail: helloOk ? "digest-order match" : `got ${helloHex}`,
  });

  const emptyRoot = await computeConvergenceRoot([]);
  rows.push({
    name: "ConvergenceRoot(|F|<2)",
    ok: bytesToHex(emptyRoot) === "0".repeat(64),
    detail: "all-zero root boundary",
  });

  const a = hexToBytes("01".repeat(32));
  const b = hexToBytes("02".repeat(32));
  const root = await computeConvergenceRoot([b, a]);
  const rootSorted = await computeConvergenceRoot([a, b]);
  rows.push({
    name: "ConvergenceRoot sort stability",
    ok: bytesToHex(root) === bytesToHex(rootSorted),
    detail: "byte-order sort before SHA256d",
  });

  const sigma = await computeSigma(a, root, b, 103);
  rows.push({
    name: "Sigma 100-byte preimage",
    ok: sigma.length === 32,
    detail: "txid || C || prev || LE32(height)",
  });

  rows.push({
    name: "RIGOR genesis constant",
    ok: hexEqual(
      RIGOR_GENESIS_HASH_DISPLAY,
      "1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758",
    ),
    detail: "pinned RFC-016 genesis; chain identity is hashPrevBlock, not a rolling fingerprint",
  });

  return rows;
}
