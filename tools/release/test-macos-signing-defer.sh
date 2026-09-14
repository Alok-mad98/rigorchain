#!/usr/bin/env bash
# P0-095: missing secrets / non-Darwin must DEFER and must not block the release.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT="${ROOT}/tools/release/sign-macos-wallet-dmg.sh"
[[ -x "${SCRIPT}" ]] || { echo "FAIL: ${SCRIPT} is not executable" >&2; exit 1; }

unset MACOS_DEVELOPER_ID_APPLICATION_CERTIFICATE
unset MACOS_DEVELOPER_ID_APPLICATION_PASSWORD
unset MACOS_DEVELOPER_ID_APPLICATION_IDENTITY

out="$(bash "${SCRIPT}" --probe)"
echo "${out}"

need() {
  echo "${out}" | grep -qx "$1" || { echo "FAIL: missing token $1" >&2; exit 1; }
}

need "MACOS_SIGNING_STATUS=DEFERRED"
need "MACOS_SIGNING_DEFERRED_REASON=github_secrets_unavailable"
need "MACOS_SIGNING_VERIFICATION=SKIPPED"
need "MACOS_NOTARIZATION_READY=FALSE"
need "RELEASE_BLOCKED_BY_MACOS_SIGNING=FALSE"
need "CONSENSUS_CODE_CHANGED=FALSE"
need "DATABASE_FORMAT_CHANGED=FALSE"
need "FINAL_VERDICT=PASS"
echo "${out}" | grep -q 'MACOS_SIGNING_DEFERRED_REASON=' || {
  echo "FAIL: missing deferred reason" >&2
  exit 1
}

# Dummy secrets must still defer on Linux (cannot codesign).
export MACOS_DEVELOPER_ID_APPLICATION_CERTIFICATE="$(printf 'not-a-p12' | base64 -w0 2>/dev/null || printf 'not-a-p12' | base64)"
export MACOS_DEVELOPER_ID_APPLICATION_PASSWORD="dummy"
export MACOS_DEVELOPER_ID_APPLICATION_IDENTITY="Developer ID Application: Dummy (0000000000)"
out2="$(bash "${SCRIPT}" --probe)"
echo "${out2}"
if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "${out2}" | grep -qx "MACOS_SIGNING_STATUS=DEFERRED" || {
    echo "FAIL: dummy secrets on non-Darwin must DEFER" >&2
    exit 1
  }
  echo "${out2}" | grep -qx "MACOS_SIGNING_DEFERRED_REASON=host_not_darwin" || {
    echo "FAIL: dummy secrets on non-Darwin must report host_not_darwin" >&2
    exit 1
  }
  echo "${out2}" | grep -qx "RELEASE_BLOCKED_BY_MACOS_SIGNING=FALSE" || {
    echo "FAIL: dummy secrets must not block release" >&2
    exit 1
  }
fi

# Signing a missing DMG on Linux still defers before the file check.
out3="$(bash "${SCRIPT}" macos-arm64)"
echo "${out3}" | grep -qx "MACOS_SIGNING_STATUS=DEFERRED" || {
  echo "FAIL: missing DMG on non-Darwin must DEFER, not FAIL" >&2
  exit 1
}

echo "MACOS_SIGNING_DEFER_TEST=PASS"
