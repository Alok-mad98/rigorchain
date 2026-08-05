#!/usr/bin/env bash
# TASK-PROOF-009: End-to-end golden vector runtime verification on materialized regtest.
#
# Usage:
#   golden_vector_verify.sh [--datadir=PATH] [--verify-fvr] [--require-full] [--no-save] [vector_id]
#
# Batch mode: when summary.established < 12 (partial chain), only ESTABLISHED vectors
# are validated; GVR_NOT_EXECUTED / DRAFT vectors are skipped. Use --require-full to
# enforce 12/12 after golden_vector_materializer.sh completes.
#
# Prerequisites:
#   - regtest rigord running with TASK-PROOF-008 fixtures materialized
#   - golden_vector_fixtures.json present in datadir
#
# Examples:
#   ./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}"
#   ./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}" --verify-fvr batch
#   ./contrib/tools/golden_vector_verify.sh --datadir="${DATADIR}" GV-RES-0003

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORE_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILT_CLI="${CORE_DIR}/src/rigor-cli"

DATADIR="${DATADIR:-${HOME}/.rigor/golden-vector-regtest}"
VERIFY_FVR=false
REQUIRE_FULL=false
SAVE_REPORT=true
TARGET="batch"
REPORT_NAME="golden_vector_harness_report.json"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --datadir=*)
      DATADIR="${1#*=}"
      shift
      ;;
    --datadir)
      DATADIR="$2"
      shift 2
      ;;
    --verify-fvr)
      VERIFY_FVR=true
      shift
      ;;
    --require-full)
      REQUIRE_FULL=true
      shift
      ;;
    --no-save)
      SAVE_REPORT=false
      shift
      ;;
    -h|--help)
      sed -n '2,15p' "$0"
      exit 0
      ;;
    batch|all|GV-*)
      TARGET="$1"
      shift
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ -x "${BUILT_CLI}" ]]; then
  RIGOR_CLI="${BUILT_CLI}"
elif command -v rigor-cli >/dev/null 2>&1; then
  RIGOR_CLI="$(command -v rigor-cli)"
else
  echo "rigor-cli not found." >&2
  exit 127
fi

mkdir -p "${DATADIR}"
CLI=( "${RIGOR_CLI}" -regtest -rpcclienttimeout=120 -datadir="${DATADIR}" )
MAX_VECTOR_HEIGHT=2100001

if ! "${CLI[@]}" getblockchaininfo >/dev/null 2>&1; then
  echo "golden_vector_verify: cannot reach regtest node (datadir=${DATADIR})" >&2
  echo "  ensure rigord is running; use: rigor-cli -regtest -datadir=... -rpcwait getblockcount" >&2
  exit 1
fi

CHAIN_HEIGHT="$("${CLI[@]}" getblockcount)"
if [[ "${REQUIRE_FULL}" == true && "${CHAIN_HEIGHT}" -lt "${MAX_VECTOR_HEIGHT}" ]]; then
  echo "golden_vector_verify: --require-full needs height >= ${MAX_VECTOR_HEIGHT}, got ${CHAIN_HEIGHT}" >&2
  echo "  finish mining: ./contrib/tools/golden_vector_materializer.sh --datadir=${DATADIR}" >&2
  echo "  partial verify meanwhile: ./contrib/tools/golden_vector_verify.sh --datadir=${DATADIR} batch" >&2
  exit 1
fi

FIXTURE_REGISTRY="${DATADIR}/golden_vector_fixtures.json"
if [[ ! -f "${FIXTURE_REGISTRY}" && -f "${DATADIR}/regtest/golden_vector_fixtures.json" ]]; then
  FIXTURE_REGISTRY="${DATADIR}/regtest/golden_vector_fixtures.json"
fi
if [[ ! -f "${FIXTURE_REGISTRY}" ]]; then
  echo "golden_vector_verify: missing golden_vector_fixtures.json under ${DATADIR} (or regtest/) — run golden_vector_materializer.sh first" >&2
  exit 1
fi

RPC_ARGS=( getruntimevectorstatus "${TARGET}" )
if [[ "${VERIFY_FVR}" == true ]]; then
  RPC_ARGS+=( true )
fi

echo "golden_vector_verify: target=${TARGET} verify_fvr=${VERIFY_FVR}"
REPORT_JSON="$("${CLI[@]}" "${RPC_ARGS[@]}")"

if [[ "${SAVE_REPORT}" == true && ( "${TARGET}" == "batch" || "${TARGET}" == "all" ) ]]; then
  REPORT_PATH="${DATADIR}/${REPORT_NAME}"
  printf '%s\n' "${REPORT_JSON}" | python3 -m json.tool > "${REPORT_PATH}"
  echo "golden_vector_verify: wrote ${REPORT_PATH}"
fi

VERIFY_FVR="${VERIFY_FVR}" REQUIRE_FULL="${REQUIRE_FULL}" REPORT_JSON="${REPORT_JSON}" python3 - <<'PY'
import json, os, sys

data = json.loads(os.environ["REPORT_JSON"])
verify_fvr = os.environ["VERIFY_FVR"].lower() == "true"
require_full = os.environ["REQUIRE_FULL"].lower() == "true"

# Expected verification_status per vector (PROOF-007 / FVR-M / PROOF-017).
EXPECT = {
    "GV-ORU-0001": "GVR_PASS",
    "GV-ORU-0002": "GVR_PASS",
    "GV-ORU-0003": "GVR_PASS",
    "GV-RES-0001": "GVR_PASS",
    "GV-RES-0002": "GVR_PASS",
    "GV-RES-0003": "GVR_PASS",
    "GV-DRAW-0001": "GVR_PARTIAL",
    "GV-DRAW-0002": "GVR_PARTIAL",
    "GV-DRAW-0003": "GVR_PARTIAL",
    "GV-STATE-0001": "GVR_PASS",
    "GV-STATE-0002": "GVR_PASS",
    "GV-STATE-0003": "GVR_PASS",
}


def check_report(report, root_equiv_false):
    errors = []
    vid = report["vector_id"] if "vector_id" in report else ""
    if root_equiv_false is not False:
        errors.append("equivalence_claimed must be false")
    if report.get("equivalence_claimed") is not False:
        errors.append("%s: equivalence_claimed must be false" % vid)
    if report.get("vector_status") != "ESTABLISHED":
        errors.append("%s: expected vector_status ESTABLISHED, got %s" % (vid, report.get("vector_status")))
    vv = report.get("vector_verification")
    if vv is not None and vv != "VERIFIED":
        errors.append("%s: expected vector_verification VERIFIED, got %s" % (vid, vv))
    exp = EXPECT.get(vid)
    got = report.get("verification_status")
    if exp and got != exp:
        errors.append("%s: expected %s, got %s" % (vid, exp, got))
    if report.get("mismatch_count", 0) != 0:
        errors.append("%s: mismatch_count must be 0" % vid)
    if verify_fvr:
        for row in report.get("field_results", []):
            if row.get("source") in ("FVR-F", "FVR-G") and row.get("required"):
                if row.get("result") not in ("MATCH", "SKIPPED"):
                    errors.append("%s: FVR row %s result %s" % (vid, row.get("field"), row.get("result")))
    return errors


errors = []
root_equiv = data.get("equivalence_claimed")
vectors = data.get("vectors")
if vectors is not None:
    summary = data.get("summary", {})
    total_n = summary.get("total", 0)
    pass_n = summary.get("pass", 0)
    partial_n = summary.get("partial", 0)
    fail_n = summary.get("fail", 0)
    not_exec_n = summary.get("not_executed", 0)
    established_n = summary.get("established", "?")
    verified_n = summary.get("verified", "?")
    print(
        "summary: total=%d pass=%d partial=%d fail=%d not_executed=%d established=%s verified=%s"
        % (total_n, pass_n, partial_n, fail_n, not_exec_n, established_n, verified_n)
    )

    established_count = summary.get("established", 0)
    full_batch = (
        require_full
        or (established_count == 12 and summary.get("not_executed", 0) == 0)
    )

    if summary.get("fail", 0) != 0:
        errors.append("fail count must be 0")

    if full_batch:
        if summary.get("established") != 12:
            errors.append("expected 12 established vectors, got %s" % summary.get("established"))
        if summary.get("verified") != 12:
            errors.append("expected 12 verified vectors, got %s" % summary.get("verified"))
        if summary.get("not_executed", 0) != 0:
            errors.append("not_executed must be 0")
        if summary.get("pass", 0) != 9:
            errors.append("expected 9 GVR_PASS, got %s" % summary.get("pass"))
        if summary.get("partial", 0) != 3:
            errors.append("expected 3 GVR_PARTIAL (draw), got %s" % summary.get("partial"))
    else:
        print(
            "partial chain mode: validating %d established vector(s) only "
            "(pending=%d)" % (established_count, summary.get("not_executed", 0))
        )
        if established_count == 0:
            errors.append("no established vectors to verify")
        if summary.get("verified") != established_count:
            errors.append(
                "verified must match established: verified=%s established=%s"
                % (summary.get("verified"), established_count)
            )

    seen = set()
    checked = 0
    for entry in vectors:
        vid = entry.get("vector_id")
        seen.add(vid)
        if entry.get("vector_status") != "ESTABLISHED":
            if full_batch:
                errors.extend(check_report(entry, root_equiv))
            continue
        checked += 1
        errors.extend(check_report(entry, root_equiv))

    if full_batch:
        missing = set(EXPECT) - seen
        if missing:
            errors.append("missing vectors: %s" % sorted(missing))
    elif checked != established_count:
        errors.append(
            "established vector check count mismatch: checked=%d summary.established=%s"
            % (checked, established_count)
        )
else:
    errors.extend(check_report(data, root_equiv))
    full_batch = True
    checked = 1

if errors:
    print("golden_vector_verify: FAILED", file=sys.stderr)
    for e in errors:
        print("  - %s" % e, file=sys.stderr)
    if vectors is not None and summary.get("fail", 0) > 0:
        print(
            "  hint: stale fixtures after reindex/mining interrupt — run:",
            file=sys.stderr,
        )
        print(
            "    ./contrib/tools/golden_vector_materializer.sh --datadir=<datadir> --rematerialize-only",
            file=sys.stderr,
        )
    sys.exit(1)

if full_batch:
    print("golden_vector_verify: all checks passed (12/12 VERIFIED, equivalence_claimed=false)")
else:
    print(
        "golden_vector_verify: partial checks passed "
        "(%d/%d established VERIFIED, equivalence_claimed=false)"
        % (checked, summary.get("total", 12))
    )
PY

echo "golden_vector_verify: done."
