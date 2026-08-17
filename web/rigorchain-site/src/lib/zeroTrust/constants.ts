/**
 * Frozen AlphaPlus constants used by the browser verifier.
 * These are published protocol constants, not live API results.
 */

import { displayHexToNative } from "./bytes";

/** RFC-016 AlphaPlus / Alpha1 mainnet genesis (display hex). */
export const ALPHAPLUS_GENESIS_HASH_DISPLAY =
  "1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758";

/** RES_PROBABILITY_TARGET_CONST display hex (params.h). */
export const RES_PROBABILITY_TARGET_DISPLAY =
  "0000000000015798ee2308c39df9fb841a566d74f87a7a9a7aeb02c2d2f8e0d1";

export const ORU_COIN = 100_000_000n;
export const RES_BASE = 100n;

/** Explorer observational short ID. This portal does not use it as a verdict. */
export const DRAW_FINGERPRINT_LABEL = "Draw Fingerprint";

export function alphaplusGenesisNative(): Uint8Array {
  return displayHexToNative(ALPHAPLUS_GENESIS_HASH_DISPLAY);
}

export function resProbabilityTargetNative(): Uint8Array {
  return displayHexToNative(RES_PROBABILITY_TARGET_DISPLAY);
}

export const ZERO_TRUST_PUBLIC_PATH = "/zero-trust-verification";

/** Official public portal URL on www.rigorchain.com */
export const ZERO_TRUST_PUBLIC_URL = "https://www.rigorchain.com/zero-trust-verification";
