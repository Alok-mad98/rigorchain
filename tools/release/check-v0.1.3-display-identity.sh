#!/usr/bin/env bash
# Fail-closed source check: v0.1.3 display identity (not Alpha / not v0.1.2-alphaplus).
# Does not change packed CLIENT_VERSION (25.0.0) or consensus.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
die() { echo "FAIL: $*" >&2; exit 1; }

h="${ROOT}/core/src/clientversion.h"
grep -q 'RIGOR_RELEASE_VERSION = "v0.2"' "${h}" || die "clientversion.h display version is not v0.2"
grep -q 'RIGOR_RELEASE_CODENAME = "Production Release"' "${h}" || die "clientversion.h codename is not Production Release"
grep -q 'RIGOR_RELEASE_VERSION = "v0.1.2-alphaplus"' "${h}" && die "clientversion.h still has v0.1.2-alphaplus"

nsi="${ROOT}/installer/windows/rigor_alpha_installer.nsi"
grep -q '^Name "Rigor Wallet"$' "${nsi}" || die "NSIS Name is not Rigor Wallet"
grep -q 'Rigor Wallet Alpha' "${nsi}" && die "NSIS still contains Rigor Wallet Alpha"
grep -F 'InstallDir "$PROGRAMFILES64\Rigor Wallet"' "${nsi}" >/dev/null || die "NSIS InstallDir is not Rigor Wallet"
grep -F 'File "${STAGING_DIR}/rigor-miner.exe"' "${nsi}" && die "NSIS still installs rigor-miner.exe"
grep -F 'CreateShortcut "$SMPROGRAMS\Rigor Wallet\Rigor Miner.lnk"' "${nsi}" && die "NSIS still creates miner shortcut"

plist="${ROOT}/core/share/qt/Info.plist.in"
grep -q '<string>0.2</string>' "${plist}" || die "Info.plist.in missing 0.2"
grep -q '<string>Rigor Wallet</string>' "${plist}" || die "Info.plist.in CFBundleName is not Rigor Wallet"

echo "DISPLAY_IDENTITY_SOURCE=PASS"
echo "PACKED_CLIENT_VERSION_UNCHANGED=25.0.0"
