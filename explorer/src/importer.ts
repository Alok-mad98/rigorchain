import type { Config } from "./config";
import type { Pool } from "pg";
import {
  clearImporterRecovery,
  getImporterRecoveryState,
  recordImporterFailure,
  recordRecoveryRetry,
  shouldRetryImport,
} from "./importerRecovery";
import { logMismatchEvent } from "./mismatchLog";
import { rpc } from "./rpcCli";
import { classifyScriptPubKey } from "./script";
import { oruStringToAtoms, resStringToAtoms } from "./amount";
import { hasPcorbProofFields } from "./witnessPanel";

type TxVinWitness = string[] | null;
type OutputMeta = {
  vout_index: number;
  value_atoms: bigint;
  asset_kind: "ORU" | "RES" | "NONE";
  script_pubkey: string;
  address: string | null;
  witness_version: number | null;
  program_hex: string | null;
};

type TxVinMeta = {
  vin_index: number;
  prev_txid: string | null;
  prev_vout: number | null;
  sequence: bigint | null;
  coinbase: boolean;
  witness: TxVinWitness; // txinwitness or final_scriptwitness
  script_sig: any;
};

type ParsedTx = {
  txid: string;
  wtxid: string | null;
  is_coinbase: boolean;
  version: number | null;
  locktime: bigint | null;
  block_tx_index: number;
  vins: TxVinMeta[];
  vouts: OutputMeta[];
};

const TESTNET_BECH32_HRP = "trg";

// Must equal Consensus::RES_PROBABILITY_TARGET_CONST in core/src/consensus/params.h.
//
// RFC-014 v1.1: floor(2^240 / MIN_CONVERGENCE_AMOUNT). The previous value here was 2^240,
// which RFC-014 v1.0 derived at f = 1 while the consensus weight is denominated in
// satoshis; see SECURITY-001-A finding A-6.
const FROZEN_RES_PROBABILITY_TARGET_HEX =
  "00000000002af31dc4611873bf3f70834acdae9f0f4f534f5d60585a5f1c1a3c";

function asString(x: unknown): string {
  if (x === null || x === undefined) return "";
  return typeof x === "string" ? x : String(x);
}

/** node-pg sends JS arrays as PG array literals; JSONB columns need JSON text. */
function jsonbParam(value: unknown): string | null {
  if (value === null || value === undefined) return null;
  return JSON.stringify(value);
}

function leHexToBigIntLE(hex: string): bigint {
  const s = hex.toLowerCase().replace(/^0x/, "");
  if (s.length % 2 !== 0) throw new Error(`leHexToBigInt: odd hex length: ${hex}`);
  const bytes = Buffer.from(s, "hex");
  const be = Buffer.from(bytes).reverse();
  const beHex = be.toString("hex") || "00";
  return BigInt("0x" + beHex);
}

function leHexToNumberLE(hex: string): number {
  const b = leHexToBigIntLE(hex);
  if (b > BigInt(Number.MAX_SAFE_INTEGER)) throw new Error(`leHexToNumberLE overflow: ${hex}`);
  return Number(b);
}

function extractWitnessStack(vin: any): string[] | null {
  // Core RPC naming varies with verbosity and decoding path.
  if (Array.isArray(vin?.txinwitness)) return vin.txinwitness as string[];
  if (Array.isArray(vin?.final_scriptwitness)) return vin.final_scriptwitness as string[];
  return null;
}

export async function importerTick(
  config: Config,
  pool: Pool,
  nowTag: string,
): Promise<{ imported: number; reorgHappened: boolean; stalled: boolean }> {
  // Single-thread determinism: importerTick must be serialized by caller.
  const chain = await (await import("./db")).getOrCreateChainState(pool, config);
  const recovery = getImporterRecoveryState();

  if (recovery.stalled_height != null && !shouldRetryImport(Date.now(), config.mismatchRetryIntervalMs)) {
    return { imported: 0, reorgHappened: false, stalled: true };
  }

  if (recovery.stalled_height != null) {
    recordRecoveryRetry(recovery.stalled_height);
  }

  // Tip tracking: use getblockchaininfo as stable contract surface.
  const info = await rpc.getblockchaininfo(config);
  const tipHeight = Number(info.blocks);
  const nodeBestHash = typeof info.bestblockhash === "string" ? info.bestblockhash : null;

  const toBigInt = (v: unknown): bigint => {
    if (typeof v === "bigint") return v;
    if (typeof v === "string") return BigInt(v);
    if (typeof v === "number") return BigInt(v);
    return BigInt(0);
  };

  let nodeLastHashAtChainHeight: string | null = null;
  const chainLastHeight = toBigInt(chain.last_height);
  if (chainLastHeight >= 0n) {
    nodeLastHashAtChainHeight = await rpc.getblockhash(config, chainLastHeight.toString());
  }

  // Reorg detection based on same-height hash mismatch.
  let reorgHappened = false;
  let nextStartHeight = Number(chainLastHeight + 1n);

  const chainLastHash = chain.last_block_hash;
  if (chainLastHeight >= 0n && chainLastHash && nodeLastHashAtChainHeight && nodeLastHashAtChainHeight !== chainLastHash) {
    reorgHappened = true;

    // Find fork height by walking back deterministically (bounded by maxSupportedReorgDepth).
    let h = Number(chainLastHeight);
    let forkHeight: number | null = null;
    for (let i = 0; i <= config.maxSupportedReorgDepth; i++) {
      const nodeHash = await rpc.getblockhash(config, String(h));
      const dbRows = await pool.query<{ block_hash: string }>(
        `SELECT block_hash FROM blocks WHERE height = $1 LIMIT 1`,
        [BigInt(h)],
      );
      const dbHash = dbRows.rows[0]?.block_hash ?? null;
      if (dbHash && dbHash === nodeHash) {
        forkHeight = h;
        break;
      }
      h = h - 1;
      if (h < 0) break;
    }

    if (forkHeight === null) {
      // Full reindex trigger (depth too large or DB missing).
      await (await import("./db")).truncateFromHeight(pool, 0n);
      await (await import("./db")).updateChainState(pool, config, { last_height: -1n, last_block_hash: null });
      nextStartHeight = 0;
    } else {
      const truncateFrom = Math.max(0, forkHeight - config.reorgSafetyWindow);
      await (await import("./db")).truncateFromHeight(pool, BigInt(truncateFrom));
      await (await import("./db")).updateChainState(pool, config, { last_height: BigInt(truncateFrom - 1), last_block_hash: null });
      nextStartHeight = truncateFrom;
    }
  }

  const endHeight = Math.min(tipHeight, nextStartHeight + config.importBatchMaxHeight - 1);
  if (nextStartHeight > endHeight) {
    return { imported: 0, reorgHappened, stalled: recovery.stalled_height != null };
  }

  let imported = 0;
  for (let h = nextStartHeight; h <= endHeight; h++) {
    try {
      const res = await importBlockAtHeight(config, pool, h);
      await (await import("./db")).updateChainState(pool, config, {
        last_height: BigInt(h),
        last_block_hash: res.blockHash,
      });
      imported++;
      clearImporterRecovery(h);
      if (!res.mismatch) {
        await (await import("./db")).updateChainState(pool, config, { critical_root_mismatch: false });
      }
    } catch (e) {
      const msg = String(e);
      recordImporterFailure(h, msg);
      if (config.stopOnRootMismatch) {
        return { imported, reorgHappened, stalled: true };
      }
      throw e;
    }
  }

  return { imported, reorgHappened, stalled: false };
}

async function importBlockAtHeight(
  config: Config,
  pool: Pool,
  height: number,
): Promise<{ blockHash: string; mismatch: boolean }> {
  const blockHash = await rpc.getblockhash(config, String(height));
  // Verbosity 2 embeds full txs (genesis coinbase is not available via getrawtransaction).
  const block = await rpc.getblock(config, blockHash, 2);
  const cr = await rpc.getconvergenceresult(config, blockHash);

  // Frozen constant check
  const resProbTarget = asString(block.res_probability_target);
  if (resProbTarget !== FROZEN_RES_PROBABILITY_TARGET_HEX) {
    const msg = `res_probability_target mismatch at height=${height} rpc=${resProbTarget}`;
    await (await import("./db")).updateChainState(pool, config, { critical_root_mismatch: true });
    logMismatchEvent({
      kind: "res_probability_target",
      height,
      block_hash: blockHash,
      message: msg,
    });
    if (config.stopOnRootMismatch) throw new Error(msg);
  }

  const committed = asString(cr.draw_candidate_root_committed);
  const computed = asString(cr.draw_candidate_root_computed);
  const mismatch = committed !== computed;
  if (mismatch) {
    await (await import("./db")).updateChainState(pool, config, { critical_root_mismatch: true });
    logMismatchEvent({
      kind: "root_committed_computed",
      height,
      block_hash: blockHash,
      committed_root: committed,
      computed_root: computed,
      message: "draw_candidate_root_committed != draw_candidate_root_computed",
    });
    if (config.stopOnRootMismatch) throw new Error("root mismatch committed != computed");
  }

  const blockTxEntries: unknown[] = Array.isArray(block.tx) ? block.tx : [];
  const txids: string[] = blockTxEntries.map((t) => (typeof t === "string" ? t : asString((t as { txid?: unknown }).txid)));
  const txCount = txids.length;

  const blockInsert = {
    block_hash: blockHash,
    height: BigInt(block.height ?? height),
    time: BigInt(block.time),
    bits: asString(block.bits),
    res_probability_target: resProbTarget,
    merkleroot: asString(block.merkleroot),
    draw_candidate_root: asString(block.draw_candidate_root ?? block.draw_candidate_root_committed ?? committed),
    rigor_res_minted: Number(block.rigor_res_minted ?? 0),
    rigor_winner: asString(block.rigor_winner ?? cr.draw_winner_txid ?? ""),
    cumulative_convergence: block.cumulative_convergence != null ? asString(block.cumulative_convergence) : null,
    opus_hash: block.opus_hash != null ? asString(block.opus_hash) : null,
    tx_count: txCount,

    bind_hash: asString(cr.bind_hash),
    bind_height: BigInt(cr.bind_height),
    committed_root: committed,
    computed_root: computed,
    mismatch,
  };

  // Determinism: keep importer inside a single DB transaction per block.
  await pool.query("BEGIN");
  try {
    await pool.query(
      `
        INSERT INTO blocks(
          block_hash, height, time, bits, res_probability_target,
          merkleroot, draw_candidate_root,
          rigor_res_minted, rigor_winner,
          cumulative_convergence, opus_hash, tx_count,
          bind_hash, bind_height, committed_root, computed_root, mismatch
        )
        VALUES(
          $1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16,$17
        )
        ON CONFLICT (block_hash) DO UPDATE SET
          height = EXCLUDED.height,
          time = EXCLUDED.time,
          bits = EXCLUDED.bits,
          res_probability_target = EXCLUDED.res_probability_target,
          merkleroot = EXCLUDED.merkleroot,
          draw_candidate_root = EXCLUDED.draw_candidate_root,
          rigor_res_minted = EXCLUDED.rigor_res_minted,
          rigor_winner = EXCLUDED.rigor_winner,
          cumulative_convergence = EXCLUDED.cumulative_convergence,
          opus_hash = EXCLUDED.opus_hash,
          tx_count = EXCLUDED.tx_count,
          bind_hash = EXCLUDED.bind_hash,
          bind_height = EXCLUDED.bind_height,
          committed_root = EXCLUDED.committed_root,
          computed_root = EXCLUDED.computed_root,
          mismatch = EXCLUDED.mismatch
      `,
      [
        blockInsert.block_hash,
        blockInsert.height,
        blockInsert.time,
        blockInsert.bits,
        blockInsert.res_probability_target,
        blockInsert.merkleroot,
        blockInsert.draw_candidate_root,
        blockInsert.rigor_res_minted,
        blockInsert.rigor_winner,
        blockInsert.cumulative_convergence,
        blockInsert.opus_hash,
        blockInsert.tx_count,
        blockInsert.bind_hash,
        blockInsert.bind_height,
        blockInsert.committed_root,
        blockInsert.computed_root,
        blockInsert.mismatch,
      ],
    );

    const parsedTxs: ParsedTx[] = [];
    let mintTxid: string | null = null;
    let mintProofItems: string[] | null = null;
    let winnerMintAmountAtoms: bigint = 0n;

    const winnerConvergenceTxid = asString(cr.draw_winner_txid);
    const candidates: any[] = Array.isArray(cr.candidates) ? cr.candidates : [];
    const candidateByTxid = new Map<string, any>();
    for (const c of candidates) {
      candidateByTxid.set(asString(c.txid), c);
    }

    // Pass 1: parse transactions and insert outputs (and oru_utxos rows).
    for (let i = 0; i < blockTxEntries.length; i++) {
      const entry = blockTxEntries[i];
      const txid = txids[i];
      const tx =
        typeof entry === "string"
          ? await rpc.getrawtransaction(config, entry, true)
          : (entry as Record<string, unknown>);

      const wtxid = tx?.hash != null ? asString(tx.hash) : null;
      const isCoinbase = Array.isArray(tx?.vin) && tx.vin.some((v: any) => v && typeof v.coinbase === "string");
      const version = tx?.version != null ? Number(tx.version) : null;
      const locktime = tx?.locktime != null ? BigInt(tx.locktime) : null;

      // FK correctness: outputs/input rows reference transactions(txid).
      await pool.query(
        `
          INSERT INTO transactions(txid, wtxid, block_hash, block_height, block_tx_index, version, locktime, is_coinbase, raw_hex)
          VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9)
          ON CONFLICT (txid) DO UPDATE SET
            wtxid = EXCLUDED.wtxid,
            block_hash = EXCLUDED.block_hash,
            block_height = EXCLUDED.block_height,
            block_tx_index = EXCLUDED.block_tx_index,
            version = EXCLUDED.version,
            locktime = EXCLUDED.locktime,
            is_coinbase = EXCLUDED.is_coinbase,
            raw_hex = EXCLUDED.raw_hex
        `,
        [
          txid,
          wtxid,
          blockHash,
          BigInt(height),
          i,
          version,
          locktime,
          isCoinbase,
          tx?.hex ?? null,
        ],
      );

      const vins: TxVinMeta[] = [];
      for (let vin_index = 0; vin_index < (tx?.vin?.length ?? 0); vin_index++) {
        const vin = tx.vin[vin_index];
        const prev_txid = vin?.txid != null ? asString(vin.txid) : null;
        const prev_vout = vin?.vout != null ? Number(vin.vout) : null;
        const sequence = vin?.sequence != null ? BigInt(vin.sequence) : null;
        const coinbase = vin?.coinbase != null;
        const witnessStack = extractWitnessStack(vin);
        const script_sig = vin?.scriptSig ?? null;

        // Mint detection (PCORB proof witness): the proof binds to the draw winner convergence txid.
        // Matched on the shape of the six proof fields rather than on stack depth, because the
        // funding input pushes its own redeem script above them (depth 7 for the devtest P2WSH
        // bootstrap) — the same reason `Consensus::ParseConvergeProof` ignores trailing pushes.
        if (blockInsert.rigor_res_minted === 1 && mintTxid === null && witnessStack && hasPcorbProofFields(witnessStack)) {
          if (asString(witnessStack[0]) === winnerConvergenceTxid) {
            // Deterministic selection: first matching vin wins.
            mintTxid = txid;
            mintProofItems = witnessStack;

            // mint_amount atoms: sum RES outputs created in this tx (computed after we parse vouts below).
          }
        }

        vins.push({
          vin_index,
          prev_txid,
          prev_vout,
          sequence,
          coinbase: !!coinbase,
          witness: witnessStack,
          script_sig,
        });
      }

      const vouts: OutputMeta[] = [];
      for (let vout_index = 0; vout_index < (tx?.vout?.length ?? 0); vout_index++) {
        const v = tx.vout[vout_index];
        const scriptPubKeyHex = v?.scriptPubKey?.hex != null ? asString(v.scriptPubKey.hex) : asString(v?.scriptPubKey);
        const cls = classifyScriptPubKey(scriptPubKeyHex, TESTNET_BECH32_HRP);
        const asset_kind = cls.assetKind;

        const rpcValue = v?.value != null ? asString(v.value) : "0";

        let value_atoms: bigint = 0n;
        if (asset_kind === "ORU") value_atoms = oruStringToAtoms(rpcValue);
        else if (asset_kind === "RES") value_atoms = resStringToAtoms(rpcValue);
        else value_atoms = 0n;

        const outMeta: OutputMeta = {
          vout_index,
          value_atoms,
          asset_kind,
          script_pubkey: scriptPubKeyHex,
          address: cls.address,
          witness_version: cls.witnessVersion,
          program_hex: cls.programHex,
        };
        vouts.push(outMeta);

        await pool.query(
          `
            INSERT INTO outputs(txid, vout_index, value_atoms, asset_kind, script_pubkey, address, witness_version, program_hex)
            VALUES($1,$2,$3,$4,$5,$6,$7,$8)
            ON CONFLICT (txid, vout_index) DO UPDATE SET
              value_atoms = EXCLUDED.value_atoms,
              asset_kind = EXCLUDED.asset_kind,
              script_pubkey = EXCLUDED.script_pubkey,
              address = EXCLUDED.address,
              witness_version = EXCLUDED.witness_version,
              program_hex = EXCLUDED.program_hex
          `,
          [
            txid,
            vout_index,
            value_atoms,
            asset_kind,
            scriptPubKeyHex,
            cls.address,
            cls.witnessVersion,
            cls.programHex,
          ],
        );

        if (asset_kind === "ORU") {
          await pool.query(
            `
              INSERT INTO oru_utxos(txid, vout_index, height, amount_atoms, script_pubkey, address, spent, spending_txid, spending_vin_index)
              VALUES($1,$2,$3,$4,$5,$6,FALSE,NULL,NULL)
              ON CONFLICT (txid, vout_index) DO UPDATE SET
                height = EXCLUDED.height,
                amount_atoms = EXCLUDED.amount_atoms,
                script_pubkey = EXCLUDED.script_pubkey,
                address = EXCLUDED.address,
                spent = oru_utxos.spent,
                spending_txid = oru_utxos.spending_txid,
                spending_vin_index = oru_utxos.spending_vin_index
            `,
            [txid, vout_index, BigInt(height), value_atoms, scriptPubKeyHex, cls.address],
          );
        }
      }

      // Compute winner mint amount atoms after vouts parsed.
      if (mintTxid === txid) {
        winnerMintAmountAtoms = vouts.reduce((acc, o) => (o.asset_kind === "RES" ? acc + o.value_atoms : acc), 0n);
      }

      parsedTxs.push({
        txid,
        wtxid,
        is_coinbase: isCoinbase,
        version,
        locktime,
        block_tx_index: i,
        vins,
        vouts,
      });
    }

    // Pass 2: insert convergence candidates (convergence_events).
    for (const c of candidates.slice().sort((a, b) => asString(a.txid).localeCompare(asString(b.txid)))) {
      const convTxid = asString(c.txid);
      const drawEligible = Boolean(c.draw_eligible);

      // convergence_root: use committed root as canonical per witness binding display.
      const mintAmountAtoms = convTxid === winnerConvergenceTxid && blockInsert.rigor_res_minted === 1 ? winnerMintAmountAtoms : 0n;

      await pool.query(
        `
          INSERT INTO convergence_events(
            block_hash, block_height, convergence_txid,
            selection_digest_h, draw_eligible, convergence_type,
            bind_hash, bind_height,
            convergence_root, res_probability_target,
            committed_root, computed_root, mismatch,
              mint_amount
          )
          VALUES(
            $1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14
          )
          ON CONFLICT (block_hash, convergence_txid) DO UPDATE SET
            selection_digest_h = EXCLUDED.selection_digest_h,
            draw_eligible = EXCLUDED.draw_eligible,
            convergence_type = EXCLUDED.convergence_type,
            bind_hash = EXCLUDED.bind_hash,
            bind_height = EXCLUDED.bind_height,
            convergence_root = EXCLUDED.convergence_root,
            res_probability_target = EXCLUDED.res_probability_target,
            committed_root = EXCLUDED.committed_root,
            computed_root = EXCLUDED.computed_root,
            mismatch = EXCLUDED.mismatch,
              mint_amount = EXCLUDED.mint_amount
        `,
        [
          blockHash,
          BigInt(height),
          convTxid,
          asString(c.h ?? null),
          drawEligible,
          asString(c.convergence_type ?? null),
          asString(cr.bind_hash),
          BigInt(cr.bind_height),
          committed,
          asString(cr.res_probability_target),
          committed,
          computed,
          mismatch,
          mintAmountAtoms,
        ],
      );
    }

    // Pass 3: insert RES mint rows if present.
    if (blockInsert.rigor_res_minted === 1 && mintTxid && mintProofItems) {
      const winnerCandidate = candidateByTxid.get(winnerConvergenceTxid) ?? null;
      const winnerDrawEligible = winnerCandidate ? Boolean(winnerCandidate.draw_eligible) : false;

      // proof items per Witness V1 display rules:
      // [0] convergence_txid, [1] convergence_amount_sat (LE64), [2] bind_hash, [3] height (LE64), [4] target_prev (LE32), [5] convergence_root
      const proof = mintProofItems;
      const proofConvergenceTxid = asString(proof[0]);
      const proofConvergenceRoot = asString(proof[5]);
      const proofBindHash = asString(proof[2]);
      const proofHeightLe = proof[3];
      const proofBindHeight = leHexToBigIntLE(proofHeightLe);

      for (const o of parsedTxs.find((t) => t.txid === mintTxid)?.vouts ?? []) {
        if (o.asset_kind !== "RES") continue;

        // Deterministic replay correctness: store proof items verbatim (no reinterpretation).
        const proof_items_obj = {
          witness_stack: proof,
          parsed: {
            convergence_txid: proofConvergenceTxid,
            bind_hash: proofBindHash,
            bind_height: proofBindHeight.toString(),
            convergence_root: proofConvergenceRoot,
          },
        };

        await pool.query(
          `
            INSERT INTO res_mints(
              mint_txid, mint_vout_index,
              block_hash, block_height,
              winner_convergence_txid,
              mint_amount,
              bind_hash, bind_height,
              convergence_root,
              draw_eligible,
              res_probability_target,
              committed_root, computed_root,
              proof_items
            )
            VALUES(
              $1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14
            )
            ON CONFLICT (mint_txid, mint_vout_index) DO UPDATE SET
              block_hash = EXCLUDED.block_hash,
              block_height = EXCLUDED.block_height,
              winner_convergence_txid = EXCLUDED.winner_convergence_txid,
              mint_amount = EXCLUDED.mint_amount,
              bind_hash = EXCLUDED.bind_hash,
              bind_height = EXCLUDED.bind_height,
              convergence_root = EXCLUDED.convergence_root,
              draw_eligible = EXCLUDED.draw_eligible,
              res_probability_target = EXCLUDED.res_probability_target,
              committed_root = EXCLUDED.committed_root,
              computed_root = EXCLUDED.computed_root,
              proof_items = EXCLUDED.proof_items
          `,
          [
            mintTxid,
            o.vout_index,
            blockHash,
            BigInt(height),
            winnerConvergenceTxid,
            o.value_atoms,
            asString(cr.bind_hash),
            BigInt(cr.bind_height),
            proofConvergenceRoot || committed,
            winnerDrawEligible,
            asString(cr.res_probability_target),
            committed,
            computed,
            jsonbParam(proof_items_obj),
          ],
        );
      }
    }

    // Pass 4: insert inputs; mark ORU UTXOs spent; insert RES transfer rows.
    for (const tx of parsedTxs) {
      // Determine first RES input (for deterministic res_transfer source binding).
      let firstResInput: { vin_index: number; prev_txid: string; prev_vout: number; } | null = null;

      for (const vin of tx.vins) {
        await pool.query(
          `
            INSERT INTO inputs(txid, vin_index, prev_txid, prev_vout, sequence, coinbase, txinwitness, script_sig)
            VALUES($1,$2,$3,$4,$5,$6,$7,$8)
            ON CONFLICT (txid, vin_index) DO UPDATE SET
              prev_txid = EXCLUDED.prev_txid,
              prev_vout = EXCLUDED.prev_vout,
              sequence = EXCLUDED.sequence,
              coinbase = EXCLUDED.coinbase,
              txinwitness = EXCLUDED.txinwitness,
              script_sig = EXCLUDED.script_sig
          `,
          [
            tx.txid,
            vin.vin_index,
            vin.prev_txid,
            vin.prev_vout,
            vin.sequence,
            vin.coinbase,
            jsonbParam(vin.witness),
            jsonbParam(vin.script_sig),
          ],
        );

        if (!vin.prev_txid || vin.prev_vout === null) continue;
        const prev = await pool.query<{ asset_kind: string }>(
          `SELECT asset_kind FROM outputs WHERE txid = $1 AND vout_index = $2`,
          [vin.prev_txid, vin.prev_vout],
        );
        const prevAsset = prev.rows[0]?.asset_kind ?? null;

        if (prevAsset === "ORU") {
          await pool.query(
            `
              UPDATE oru_utxos
              SET spent = TRUE,
                  spending_txid = $1,
                  spending_vin_index = $2
              WHERE txid = $3 AND vout_index = $4 AND spent = FALSE
            `,
            [tx.txid, vin.vin_index, vin.prev_txid, vin.prev_vout],
          );
        }

        if (prevAsset === "RES" && firstResInput === null) {
          firstResInput = { vin_index: vin.vin_index, prev_txid: vin.prev_txid, prev_vout: vin.prev_vout };
        }
      }

      // Insert transfer outputs if tx spends a RES input and is not the mint tx.
      if (tx.txid !== mintTxid && firstResInput) {
        const resOutputs = tx.vouts.filter((o) => o.asset_kind === "RES");
        for (const o of resOutputs) {
          await pool.query(
            `
              INSERT INTO res_transfers(
                spend_txid, spend_vin_index, spend_block_hash,
                res_input_txid, res_input_vout_index,
                res_output_txid, res_output_vout_index,
                res_amount_atoms
              )
              VALUES($1,$2,$3,$4,$5,$6,$7,$8)
              ON CONFLICT (spend_txid, spend_vin_index, res_output_vout_index) DO UPDATE SET
                spend_block_hash = EXCLUDED.spend_block_hash,
                res_input_txid = EXCLUDED.res_input_txid,
                res_input_vout_index = EXCLUDED.res_input_vout_index,
                res_output_txid = EXCLUDED.res_output_txid,
                res_amount_atoms = EXCLUDED.res_amount_atoms
            `,
            [
              tx.txid,
              firstResInput.vin_index,
              blockHash,
              firstResInput.prev_txid,
              firstResInput.prev_vout,
              tx.txid,
              o.vout_index,
              o.value_atoms,
            ],
          );
        }
      }
    }

    await pool.query("COMMIT");
    return { blockHash, mismatch };
  } catch (e) {
    await pool.query("ROLLBACK");
    throw e;
  }
}

