import { bech32, bech32m } from "bech32";

export type AssetKind = "ORU" | "RES" | "NONE";

export type ScriptClassification = {
  assetKind: AssetKind;
  witnessVersion: number | null;
  programHex: string | null;
  address: string | null;
};

// ASCII prefixes per explorer witness v1 display rules.
// - CVG2 convergence program prefix: "CVG2"
// - RES2 program prefix: "RES2"
const CVG2_PREFIX_HEX = Buffer.from("CVG2", "ascii").toString("hex"); // 43564732
const RES2_PREFIX_HEX = Buffer.from("RES2", "ascii").toString("hex"); // 52455332

function tryEncodeSegwitAddress(hrp: string, witnessVersion: number, program: Buffer): string | null {
  try {
    const progWords = bech32.toWords(Uint8Array.from(program));
    const words = [witnessVersion, ...progWords];
    if (witnessVersion === 0) {
      return bech32.encode(hrp, words, 90);
    }
    return bech32m.encode(hrp, words, 90);
  } catch {
    return null;
  }
}

/**
 * Minimal deterministic scriptPubKey classification:
 * - v0 witness (P2WPKH/P2WSH) => ORU
 * - v1 witness:
 *     - prefix "CVG2" => ORU (convergence fee output)
 *     - otherwise => RES
 * - OP_RETURN => NONE
 *
 * This is intentionally narrow (V0/V1 only) per RIGOR explorer constraints.
 */
export function classifyScriptPubKey(scriptPubKeyHex: string, bech32Hrp: string): ScriptClassification {
  const hex = scriptPubKeyHex.toLowerCase();
  const bytes = Buffer.from(hex, "hex");

  // OP_RETURN
  if (bytes.length >= 1 && bytes[0] === 0x6a) {
    return { assetKind: "NONE", witnessVersion: null, programHex: null, address: null };
  }

  // Witness v0: OP_0 <20|32>
  if (bytes.length === 22 && bytes[0] === 0x00 && bytes[1] === 0x14) {
    const program = bytes.subarray(2, 22);
    const programHex = Buffer.from(program).toString("hex");
    const address = tryEncodeSegwitAddress(bech32Hrp, 0, Buffer.from(program));
    return { assetKind: "ORU", witnessVersion: 0, programHex, address };
  }
  if (bytes.length === 34 && bytes[0] === 0x00 && bytes[1] === 0x20) {
    const program = bytes.subarray(2, 34);
    const programHex = Buffer.from(program).toString("hex");
    const address = tryEncodeSegwitAddress(bech32Hrp, 0, Buffer.from(program));
    return { assetKind: "ORU", witnessVersion: 0, programHex, address };
  }

  // Witness v1: OP_1 <32>
  if (bytes.length === 34 && bytes[0] === 0x51 && bytes[1] === 0x20) {
    const program = bytes.subarray(2, 34);
    const programHex = Buffer.from(program).toString("hex");

    const assetKind: AssetKind = programHex.startsWith(CVG2_PREFIX_HEX) ? "ORU" : "RES";
    const address = tryEncodeSegwitAddress(bech32Hrp, 1, Buffer.from(program));
    return { assetKind, witnessVersion: 1, programHex, address };
  }

  // Unknown/nonstandard script: treat as NONE for safe UI.
  return { assetKind: "NONE", witnessVersion: null, programHex: null, address: null };
}

