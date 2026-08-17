export type CheckStatus = "MATCH" | "MISMATCH" | "INCOMPLETE" | "NONE" | "UNKNOWN";

export type FinalStatus = "VALID" | "INVALID" | "INCOMPLETE";

export type PublicDataSource = "user" | "rpc" | "explorer" | "none";

export type AmountBreakdown = {
  oruAtoms: bigint;
  resAtoms: bigint;
  feeAtoms: bigint | null;
};

export type CandidateInput = {
  txidDisplay: string;
  weight: bigint;
};

export type BlockPublicInput = {
  hashDisplay: string;
  height: number;
  previousHashDisplay: string | null;
  txidsDisplay: string[];
  candidates: CandidateInput[];
  committedRootDisplay: string | null;
  publishedWinnerDisplay: string | null;
  source: PublicDataSource;
};

export type TxPublicInput = {
  txidDisplay: string;
  blockHashDisplay: string | null;
  height: number | null;
  amounts: AmountBreakdown;
  source: PublicDataSource;
};

export type PipelineTrace = {
  convergenceRootDisplay: string;
  sigmas: { txidDisplay: string; sigmaDisplay: string; eligible: boolean; weight: string }[];
  winnerTxidDisplay: string | null;
  previousHashDisplay: string | null;
};

export type TxVerificationResult = {
  mode: "tx";
  transaction: FinalStatus;
  blockHeight: number | null;
  oru: string;
  fee: string;
  res: string;
  final: FinalStatus;
  genesis: CheckStatus;
  parentHash: CheckStatus;
  convergenceRoot: CheckStatus;
  winner: CheckStatus;
  trace: PipelineTrace | null;
  notes: string[];
  publicSource: PublicDataSource;
};

export type BlockVerificationResult = {
  mode: "block";
  height: number | null;
  genesis: CheckStatus;
  parentHash: CheckStatus;
  convergenceRoot: CheckStatus;
  winner: CheckStatus;
  final: FinalStatus;
  trace: PipelineTrace | null;
  notes: string[];
  publicSource: PublicDataSource;
};

export type VerificationResult = TxVerificationResult | BlockVerificationResult;
