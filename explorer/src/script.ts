import { bech32, bech32m } from "bech32";

export type AssetKind = "ORU" | "RES" | "NONE";

export const OP_CONVERGE_OPCODE = 0xbd;
export const OP_CONVERGE_SCRIPT_NAME = "OP_CONVERGE";
export const CONVERGENCE_BURN_SCRIPT_LABEL = "Convergence burn script";
export const CONVERGENCE_OUTPUT_STATE = "Unspendable burn output / draw participation";

/**
 * Historical RFC-014 / retired design: Witness v1 program prefix ASCII "CVG2".
 * Not a live consensus script. Consensus `IsAllowedScript` rejects it.
 */
const RETIRED_CVG2_PREFIX_HEX = Buffer.from("CVG2", "ascii").toString("hex"); // 43564732
export const RETIRED_CVG2_V1_SCRIPT_PREFIX_HEX = "5120" + RETIRED_CVG2_PREFIX_HEX;

export type ScriptClassification = {
  assetKind: AssetKind;
  witnessVersion: number | null;
  programHex: string | null;
  address: string | null;
  isConvergence: boolean;
  scriptName: string;
  scriptState: string | null;
};

function normalizeScriptHex(scriptPubKeyHex: string): string {
  return scriptPubKeyHex.trim().toLowerCase().replace(/\s+/g, "");
}

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

/** Live consensus Convergence lock: `OP_PUSHBYTES_20 || 20-byte HASH160 || OP_CONVERGE` (22 bytes). */
export function isOpConvergeScriptPubKey(scriptPubKeyHex: string): boolean {
  const hex = normalizeScriptHex(scriptPubKeyHex);
  if (!/^[0-9a-f]*$/.test(hex) || hex.length !== 44) return false;
  const bytes = Buffer.from(hex, "hex");
  return bytes.length === 22 && bytes[0] === 0x14 && bytes[21] === OP_CONVERGE_OPCODE;
}

/** Historical RFC-014 / retired Witness v1 CVG2 program. Not OP_CONVERGE. */
export function isRetiredCvg2WitnessV1(scriptPubKeyHex: string): boolean {
  return normalizeScriptHex(scriptPubKeyHex).startsWith(RETIRED_CVG2_V1_SCRIPT_PREFIX_HEX);
}

/**
 * True iff the script is a live Convergence burn / draw-entry lock.
 * Mirrors node `IsConvergeScript` / `IsConvergenceScript` for explorer indexing.
 */
export function isConvergenceScriptPubKey(scriptPubKeyHex: string): boolean {
  return isOpConvergeScriptPubKey(scriptPubKeyHex);
}

export function convergenceOutputLabels(): {
  script: string;
  asset: "ORU";
  state: string;
} {
  return {
    script: OP_CONVERGE_SCRIPT_NAME,
    asset: "ORU",
    state: CONVERGENCE_OUTPUT_STATE,
  };
}

/**
 * Minimal deterministic scriptPubKey classification (explorer only; not consensus):
 * - OP_CONVERGE (22-byte burn lock) => ORU, unspendable draw-entry
 * - v0 witness (P2WPKH/P2WSH) => ORU
 * - v1 witness: RES (not CVG2). Retired CVG2 prefix => NONE (not a third asset)
 * - OP_RETURN => NONE
 */
export function classifyScriptPubKey(scriptPubKeyHex: string, bech32Hrp: string): ScriptClassification {
  const hex = normalizeScriptHex(scriptPubKeyHex);
  const unknown: ScriptClassification = {
    assetKind: "NONE",
    witnessVersion: null,
    programHex: null,
    address: null,
    isConvergence: false,
    scriptName: "unknown",
    scriptState: null,
  };
  if (!hex || hex.length % 2 !== 0 || !/^[0-9a-f]*$/.test(hex)) {
    return unknown;
  }
  const bytes = Buffer.from(hex, "hex");

  if (bytes.length >= 1 && bytes[0] === 0x6a) {
    return {
      assetKind: "NONE",
      witnessVersion: null,
      programHex: null,
      address: null,
      isConvergence: false,
      scriptName: "OP_RETURN",
      scriptState: null,
    };
  }

  if (isOpConvergeScriptPubKey(hex)) {
    const program = bytes.subarray(1, 21);
    return {
      assetKind: "ORU",
      witnessVersion: null,
      programHex: Buffer.from(program).toString("hex"),
      address: null,
      isConvergence: true,
      scriptName: OP_CONVERGE_SCRIPT_NAME,
      scriptState: CONVERGENCE_OUTPUT_STATE,
    };
  }

  // Witness v0: OP_0 <20|32>
  if (bytes.length === 22 && bytes[0] === 0x00 && bytes[1] === 0x14) {
    const program = bytes.subarray(2, 22);
    const programHex = Buffer.from(program).toString("hex");
    const address = tryEncodeSegwitAddress(bech32Hrp, 0, Buffer.from(program));
    return {
      assetKind: "ORU",
      witnessVersion: 0,
      programHex,
      address,
      isConvergence: false,
      scriptName: "Witness v0",
      scriptState: null,
    };
  }
  if (bytes.length === 34 && bytes[0] === 0x00 && bytes[1] === 0x20) {
    const program = bytes.subarray(2, 34);
    const programHex = Buffer.from(program).toString("hex");
    const address = tryEncodeSegwitAddress(bech32Hrp, 0, Buffer.from(program));
    return {
      assetKind: "ORU",
      witnessVersion: 0,
      programHex,
      address,
      isConvergence: false,
      scriptName: "Witness v0",
      scriptState: null,
    };
  }

  // Witness v1: OP_1 <32> — RES only. Retired CVG2 prefix is not a live asset.
  if (bytes.length === 34 && bytes[0] === 0x51 && bytes[1] === 0x20) {
    const program = bytes.subarray(2, 34);
    const programHex = Buffer.from(program).toString("hex");
    const address = tryEncodeSegwitAddress(bech32Hrp, 1, Buffer.from(program));
    if (programHex.startsWith(RETIRED_CVG2_PREFIX_HEX)) {
      return {
        assetKind: "NONE",
        witnessVersion: 1,
        programHex,
        address,
        isConvergence: false,
        scriptName: "retired Witness v1 CVG2",
        scriptState: "Historical RFC-014 / retired design",
      };
    }
    return {
      assetKind: "RES",
      witnessVersion: 1,
      programHex,
      address,
      isConvergence: false,
      scriptName: "Witness v1 RES",
      scriptState: null,
    };
  }

  return unknown;
}
