#!/bin/sh
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Replay every fuzz_gbt_solo seed. Any abort, crash, or sanitizer report in the
# GBT copy-bytes path fails the build.
set -e
exec ./fuzz_gbt_solo "${FUZZ_CORPUS:-./fuzz_corpus/gbt_solo}"
