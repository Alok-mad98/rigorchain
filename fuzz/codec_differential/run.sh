#!/usr/bin/env bash
# Codec differential (roadmap 1.3): extracted Coq decoder vs the C++ one.
#
# Runs the same bytes through both and fails on any divergence that is not
# already declared and bounded by a theorem in WireCodec.v.
#
# Usage (from anywhere):
#   bash fuzz/codec_differential/run.sh
#   CODEC_DIFF_MUTATIONS=2000 CODEC_DIFF_RANDOM=20000 bash fuzz/codec_differential/run.sh
#   CODEC_DIFF_SEED=1234 bash fuzz/codec_differential/run.sh
#
# Requires coqc, ocamlopt and a C++17 compiler. Nothing here is linked into
# rigord and nothing here is a proof: agreement on a corpus is evidence, and
# the package should not read it as anything more. What it does buy is that
# the Coq -> OCaml -> binary step, which no theorem covers, gets exercised
# on real bytes.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$ROOT/fuzz/codec_differential"
MODEL="$ROOT/formal_verification/consensus_model"
SRC="$ROOT/core/src"
OUT="${TMPDIR:-/tmp}/codec_differential"

SEED="${CODEC_DIFF_SEED:-20260908}"
MUTATIONS="${CODEC_DIFF_MUTATIONS:-400}"
RANDOM_N="${CODEC_DIFF_RANDOM:-2000}"

CXX="${CXX:-$(command -v g++ || command -v c++)}"

mkdir -p "$OUT"

# The dependency cone of ExtractCodec.v, in order. Deliberately not the whole
# _CoqProject: that cone permits Admitted, and this one must not.
MODULES=(ByteModel CompactSizeModel TransactionModel BlockModel
         TransactionCodec BlockCodec WireCodec)

echo "== 1. Coq model =="
for m in "${MODULES[@]}"; do
  if [[ ! -f "$MODEL/$m.vo" || "$MODEL/$m.v" -nt "$MODEL/$m.vo" ]]; then
    echo "   coqc $m.v"
    coqc -q -R "$MODEL" ConsensusModel "$MODEL/$m.v"
  fi
done

CONE_SOURCES=()
for m in "${MODULES[@]}"; do CONE_SOURCES+=("$MODEL/$m.v"); done
if grep -qE '^[[:space:]]*(Admitted|Axiom)\b' "${CONE_SOURCES[@]}"; then
  echo "FAIL: the extracted cone contains Admitted or Axiom"
  exit 1
fi

echo "== 2. Extract to OCaml =="
( cd "$OUT" && coqc -q -R "$MODEL" ConsensusModel "$MODEL/ExtractCodec.v" )

echo "== 3. Build the two oracles =="
cp "$HERE/oracle_main.ml" "$OUT/"
( cd "$OUT" \
  && ocamlopt -w -a -o coq_codec_oracle \
       codec_oracle_gen.mli codec_oracle_gen.ml oracle_main.ml )

"$CXX" -O2 -std=c++17 -I"$SRC" -o "$OUT/cpp_codec" \
  "$HERE/codec_cpp.cpp" \
  "$SRC/primitives/block.cpp" \
  "$SRC/primitives/transaction.cpp" \
  "$SRC/script/script.cpp" \
  "$SRC/script/rigor_classify.cpp" \
  "$SRC/support/cleanse.cpp" \
  "$SRC/uint256.cpp" \
  "$SRC/util/strencodings.cpp" \
  "$SRC/crypto/sha256.cpp" \
  "$SRC/crypto/ripemd160.cpp" \
  "$SRC/crypto/sha512.cpp" \
  "$SRC/crypto/hmac_sha512.cpp" \
  "$SRC/hash.cpp"

echo "== 4. Generate inputs (seed=$SEED) =="
python3 "$HERE/gen_inputs.py" \
  --seed "$SEED" --mutations "$MUTATIONS" --random "$RANDOM_N" \
  > "$OUT/inputs.tsv"
echo "   $(wc -l < "$OUT/inputs.tsv") inputs"

echo "== 5. Run both =="
"$OUT/coq_codec_oracle" < "$OUT/inputs.tsv" > "$OUT/coq.out"
"$OUT/cpp_codec"        < "$OUT/inputs.tsv" > "$OUT/cpp.out"

echo "== 6. Compare =="
python3 "$HERE/compare.py" \
  --inputs "$OUT/inputs.tsv" \
  --coq "$OUT/coq.out" \
  --cpp "$OUT/cpp.out" \
  --report "$OUT/divergences.tsv"
