// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/predicate_verify.h>
#include <test/util/script.h>

bool IsValidFlagCombination(unsigned flags)
{
    // P2SH is not a RIGOR consensus rule. CLEANSTACK still requires WITNESS so
    // native outputs cannot be evaluated as anyone-can-spend.
    if (flags & SCRIPT_VERIFY_CLEANSTACK && ~flags & SCRIPT_VERIFY_WITNESS) return false;
    return true;
}
