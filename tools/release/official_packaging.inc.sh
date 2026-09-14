#!/usr/bin/env bash
# Shared paths for official packaging. Source after ROOT is set.
# shellcheck disable=SC2034
official_init() {
  RELEASE_VERSION="${RELEASE_VERSION:-$(
    python3 -c "import sys; sys.path.insert(0, '${ROOT}/tools/release'); from official_packaging import version_from_header; print(version_from_header())"
  )}"
  CHANNEL="${CHANNEL:-internal-mainnet-validation}"
  STAGING="${ROOT}/dist/release/v${RELEASE_VERSION}/staging"
  OFFICIAL="${ROOT}/dist/release/v${RELEASE_VERSION}/official"
  VISIBLE="${ROOT}/audit/wallet_packaging/official"
  WIN_EXE_NAME="rigor-v${RELEASE_VERSION}-windows-x86_64.exe"
  WIN_ZIP_NAME="rigor-v${RELEASE_VERSION}-windows-x86_64.zip"
  LINUX_TAR_NAME="rigor-v${RELEASE_VERSION}-linux-x86_64.tar.gz"
  MACOS_X64_DMG="rigor-v${RELEASE_VERSION}-macos-x86_64.dmg"
  MACOS_ARM_DMG="rigor-v${RELEASE_VERSION}-macos-arm64.dmg"
  echo "RELEASE_VERSION=${RELEASE_VERSION}"
  echo "CHANNEL=${CHANNEL}"
}

official_miner_init() {
  official_init
  MINER_STAGING="${ROOT}/dist/release/v${RELEASE_VERSION}/staging/miner"
  MINER_OFFICIAL="${ROOT}/dist/release/v${RELEASE_VERSION}/miner"
  MINER_VISIBLE="${ROOT}/audit/miner_packaging/official"
  MINER_WIN_EXE_NAME="rigor-v${RELEASE_VERSION}-miner-windows-x86_64.exe"
  MINER_WIN_ZIP_NAME="rigor-v${RELEASE_VERSION}-miner-windows-x86_64.zip"
  MINER_LINUX_TAR_NAME="rigor-v${RELEASE_VERSION}-miner-linux-x86_64.tar.gz"
}
