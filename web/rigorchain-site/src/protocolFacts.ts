/**
 * Website display facts extracted from production `core/src/`.
 * Protocol specifications are normative SSOT. This file does not
 * define protocol and does not supersede RFC-020 / RFC-018.
 *
 * Sources:
 *   core/src/clientversion.h
 *   core/src/consensus/params.h
 *   core/src/consensus/rigor_consensus_lock.h
 *   core/src/kernel/chainparams.cpp
 *   core/src/rigor_kernel/pow_digest.h
 *   formal_verification/golden_lock.json
 */

export const PRODUCT = "RIGOR v0.1.3";
export const CODENAME = "Production Release";
export const CHAIN_NAME = "RIGOR";

export const MAINNET_GENESIS =
  "f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717";
export const MAINNET_MAGIC = "ce f1 db fa";
export const MAINNET_P2P_PORT = 8760;
export const MAINNET_RPC_PORT = 8761;
export const MAINNET_HRP = "rg";

export const OPUHASH = 'SHA3-256("RGRPOW01" || header_80)';
export const TARGET_SPACING_S = 600;
export const ASERT_HALF_LIFE_S = 43200;
export const ASERT_ANCHOR_HEIGHT = 1;
export const ASERT_CLAMP = "±12.5%";

export const DRAW_WINDOW = 32;
export const DRAW_E = 246;
export const DRAW_C_ORU = 1024;
export const RES_PROBABILITY_TARGET =
  "000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479";

export const SHARED_GOLDEN_VECTORS = 433;
export const COQ_ENFORCED_VECTORS = 226;
