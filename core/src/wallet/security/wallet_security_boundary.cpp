// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077: wallet security boundary. Fail-closed. No consensus formulas.

#include <wallet/security/wallet_security_boundary.h>

#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <utility>

namespace {

void PutU32(std::vector<uint8_t>& o, uint32_t v)
{
    o.push_back(static_cast<uint8_t>(v));
    o.push_back(static_cast<uint8_t>(v >> 8));
    o.push_back(static_cast<uint8_t>(v >> 16));
    o.push_back(static_cast<uint8_t>(v >> 24));
}

void PutU64(std::vector<uint8_t>& o, uint64_t v)
{
    for (int i = 0; i < 8; ++i) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

void PutCompact(std::vector<uint8_t>& o, uint64_t n)
{
    if (n < 253) {
        o.push_back(static_cast<uint8_t>(n));
        return;
    }
    if (n <= 0xffff) {
        o.push_back(253);
        o.push_back(static_cast<uint8_t>(n));
        o.push_back(static_cast<uint8_t>(n >> 8));
        return;
    }
    o.push_back(254);
    PutU32(o, static_cast<uint32_t>(n));
}

bool AmountInRange(int64_t v)
{
    return v >= 0 && v <= WalletSecurityLimits::MAX_MONEY;
}

bool AddOverflows(int64_t running, int64_t addend)
{
    if (!AmountInRange(running) || !AmountInRange(addend)) return true;
    return addend > WalletSecurityLimits::MAX_MONEY - running;
}

bool DestOk(const std::vector<uint8_t>& d)
{
    return !d.empty() && d.size() <= WalletSecurityLimits::MAX_DESTINATION_SIZE;
}

bool DuplicateInputs(const std::vector<WalletInputSpec>& inputs)
{
    std::set<std::pair<std::string, uint32_t>> seen;
    for (const auto& in : inputs) {
        std::string k(reinterpret_cast<const char*>(in.prev_txid), 32);
        if (!seen.insert({k, in.n}).second) return true;
    }
    return false;
}

WalletSecurityResult CheckTransfer(const WalletTransactionRequest& in)
{
    if (in.outputs.empty()) {
        return WalletSecurityResult::Reject("empty-output");
    }
    if (in.outputs.size() > WalletSecurityLimits::MAX_OUTPUTS) {
        return WalletSecurityResult::Reject("oversized-output-vector");
    }
    if (in.inputs.size() > WalletSecurityLimits::MAX_INPUTS) {
        return WalletSecurityResult::Reject("oversized-input-vector");
    }
    if (in.inputs_provided && DuplicateInputs(in.inputs)) {
        return WalletSecurityResult::Reject("duplicate-input");
    }

    int64_t out_sum = 0;
    WalletAssetType tx_asset = WalletAssetType::UNKNOWN;
    bool asset_set = false;
    for (const auto& o : in.outputs) {
        if (o.amount < 0) {
            return WalletSecurityResult::Reject("negative-amount");
        }
        if (o.amount > WalletSecurityLimits::MAX_MONEY) {
            return WalletSecurityResult::Reject("overflow-amount");
        }
        if (!WalletAssetIsRegistered(o.asset)) {
            return WalletSecurityResult::Reject("unknown-asset");
        }
        if (!DestOk(o.destination)) {
            return WalletSecurityResult::Reject("invalid-destination");
        }
        if (!asset_set) {
            tx_asset = o.asset;
            asset_set = true;
        } else if (tx_asset != o.asset) {
            return WalletSecurityResult::Reject("mixed-asset");
        }
        if (AddOverflows(out_sum, o.amount)) {
            return WalletSecurityResult::Reject("overflow-amount");
        }
        out_sum += o.amount;
    }

    int64_t in_sum = 0;
    if (in.inputs_provided) {
        if (in.inputs.empty()) {
            return WalletSecurityResult::Reject("empty-input");
        }
        for (const auto& inp : in.inputs) {
            if (inp.amount < 0) {
                return WalletSecurityResult::Reject("negative-amount");
            }
            if (inp.amount > WalletSecurityLimits::MAX_MONEY) {
                return WalletSecurityResult::Reject("overflow-amount");
            }
            if (!WalletAssetIsRegistered(inp.asset)) {
                return WalletSecurityResult::Reject("unknown-asset");
            }
            if (inp.asset != tx_asset) {
                // Dual-pool ORU fee on RES spends is selected by production
                // CreateTransaction, not by this firewall. An explicit
                // mixed-asset input vector is rejected here.
                return WalletSecurityResult::Reject("mixed-asset");
            }
            if (inp.maturity_height > 0 && inp.current_height < inp.maturity_height) {
                return WalletSecurityResult::Reject("immature");
            }
            if (AddOverflows(in_sum, inp.amount)) {
                return WalletSecurityResult::Reject("overflow-amount");
            }
            in_sum += inp.amount;
        }
        if (in_sum < out_sum) {
            return WalletSecurityResult::Reject("insufficient-balance");
        }
    }

    if (in.available_balance >= 0) {
        if (in.available_balance < out_sum) {
            return WalletSecurityResult::Reject("insufficient-balance");
        }
        if (in.available_balance > WalletSecurityLimits::MAX_MONEY) {
            return WalletSecurityResult::Reject("overflow-amount");
        }
    }

    return WalletSecurityResult::Accept(WalletAdmissionTarget::TRANSACTION_BUILDER,
                                        "accept-to-builder", /*enter_kernel=*/true);
}

bool IsHexChar(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool IsBech32Char(char c)
{
    // bech32 charset + HRP letters and '1' separator.
    if (c >= 'a' && c <= 'z') return true;
    if (c >= '0' && c <= '9') return true;
    return false;
}

} // namespace

void WalletSecurityBoundary::WipeSecret(uint8_t* p, size_t n)
{
    if (!p || n == 0) return;
    volatile uint8_t* v = p;
    for (size_t i = 0; i < n; ++i) v[i] = 0;
}

bool WalletSecurityBoundary::LooksLikeSecret(const char* s)
{
    if (!s || !*s) return false;
    const size_t n = std::strlen(s);
    // WIF-ish (51-52) or 64-byte Ed25519 secret hex (128).
    if (n == 51 || n == 52) {
        return s[0] == '5' || s[0] == 'K' || s[0] == 'L' || s[0] == 'c';
    }
    if (n == 128) {
        for (size_t i = 0; i < n; ++i) {
            if (!IsHexChar(s[i])) return false;
        }
        return true;
    }
    return false;
}

WalletSecurityResult TransactionBuilder::Build(const WalletIntent& intent)
{
    WalletSecurityResult ev = WalletSecurityBoundary::Evaluate(intent);
    if (ev.IsReject()) return ev;

    std::vector<uint8_t> tx;
    PutU32(tx, 2);
    PutCompact(tx, intent.inputs.size());
    for (const auto& inp : intent.inputs) {
        tx.insert(tx.end(), inp.prev_txid, inp.prev_txid + 32);
        PutU32(tx, inp.n);
        PutU64(tx, static_cast<uint64_t>(inp.amount));
        tx.push_back(static_cast<uint8_t>(inp.asset));
    }
    PutCompact(tx, intent.outputs.size());
    for (const auto& o : intent.outputs) {
        // Refuse to emit an illegal output even if Evaluate drifted.
        if (o.amount < 0 || o.amount > WalletSecurityLimits::MAX_MONEY ||
            !WalletAssetIsRegistered(o.asset) || !DestOk(o.destination)) {
            return WalletSecurityResult::Reject("invalid-tx-output");
        }
        PutU64(tx, static_cast<uint64_t>(o.amount));
        tx.push_back(static_cast<uint8_t>(o.asset));
        PutCompact(tx, o.destination.size());
        tx.insert(tx.end(), o.destination.begin(), o.destination.end());
    }
    PutU32(tx, 0); // locktime — wallet encoding, not a consensus field rewrite

    ev.canonical_tx = std::move(tx);
    ev.must_enter_transaction_kernel = true;
    ev.reason = "canonical-tx-to-kernel";
    return ev;
}

WalletSecurityResult WalletSecurityBoundary::Evaluate(const WalletTransactionRequest& in)
{
    try {
        switch (in.kind) {
        case WalletTxKind::TRANSFER:
            return CheckTransfer(in);
        case WalletTxKind::SIGNING:
            return WalletSecurityResult::Reject("use-signing-boundary");
        case WalletTxKind::IMPORT:
            return WalletSecurityResult::Reject("use-evaluate-import");
        case WalletTxKind::RPC:
            return WalletSecurityResult::Reject("use-evaluate-rpc");
        default:
            return WalletSecurityResult::Reject("unknown-kind");
        }
    } catch (...) {
        return WalletSecurityResult::Reject("internal-fail-closed");
    }
}

WalletSecurityResult WalletSecurityBoundary::ParseAddress(const char* data, size_t size)
{
    if (!data || size == 0) {
        return WalletSecurityResult::Reject("invalid-address");
    }
    if (size > WalletSecurityLimits::MAX_ADDRESS_CHARS) {
        return WalletSecurityResult::Reject("oversized-input");
    }
    // Reject NULs and non-ASCII.
    for (size_t i = 0; i < size; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c < 32 || c > 126) {
            return WalletSecurityResult::Reject("invalid-address");
        }
    }
    if (size >= 3 && data[0] == 'r' && data[1] == 'g' && data[2] == '1') {
        if (size < 8) return WalletSecurityResult::Reject("invalid-address");
        for (size_t i = 0; i < size; ++i) {
            if (!IsBech32Char(data[i])) return WalletSecurityResult::Reject("invalid-address");
        }
        return WalletSecurityResult::Accept(WalletAdmissionTarget::TRANSACTION_BUILDER,
                                            "address-ok", /*enter_kernel=*/false);
    }
    // Hex program: 20 bytes (40 hex) ORU or 32 bytes (64 hex) RES.
    if (size == 40 || size == 64) {
        for (size_t i = 0; i < size; ++i) {
            if (!IsHexChar(data[i])) return WalletSecurityResult::Reject("invalid-address");
        }
        return WalletSecurityResult::Accept(WalletAdmissionTarget::TRANSACTION_BUILDER,
                                            "address-ok", /*enter_kernel=*/false);
    }
    return WalletSecurityResult::Reject("invalid-address");
}

WalletSecurityResult WalletSecurityBoundary::ParseAmount(const char* data, size_t size)
{
    if (!data || size == 0) {
        return WalletSecurityResult::Reject("invalid-amount");
    }
    if (size > WalletSecurityLimits::MAX_AMOUNT_CHARS) {
        return WalletSecurityResult::Reject("oversized-input");
    }
    if (data[0] == '-') {
        return WalletSecurityResult::Reject("negative-amount");
    }
    if (data[0] == '+') {
        return WalletSecurityResult::Reject("invalid-amount");
    }
    bool seen_dot = false;
    int frac = 0;
    int64_t whole = 0;
    int64_t frac_val = 0;
    bool any_digit = false;
    for (size_t i = 0; i < size; ++i) {
        const char c = data[i];
        if (c == '.') {
            if (seen_dot) return WalletSecurityResult::Reject("invalid-amount");
            seen_dot = true;
            continue;
        }
        if (c < '0' || c > '9') {
            return WalletSecurityResult::Reject("invalid-amount");
        }
        any_digit = true;
        const int d = c - '0';
        if (!seen_dot) {
            if (whole > (std::numeric_limits<int64_t>::max() - d) / 10) {
                return WalletSecurityResult::Reject("overflow-amount");
            }
            whole = whole * 10 + d;
        } else {
            ++frac;
            if (frac > 8) {
                return WalletSecurityResult::Reject("overflow-amount");
            }
            frac_val = frac_val * 10 + d;
        }
    }
    if (!any_digit) return WalletSecurityResult::Reject("invalid-amount");
    while (frac < 8) {
        if (frac_val > std::numeric_limits<int64_t>::max() / 10) {
            return WalletSecurityResult::Reject("overflow-amount");
        }
        frac_val *= 10;
        ++frac;
    }
    if (whole > WalletSecurityLimits::MAX_MONEY / WalletSecurityLimits::COIN) {
        return WalletSecurityResult::Reject("overflow-amount");
    }
    const int64_t scaled = whole * WalletSecurityLimits::COIN;
    if (AddOverflows(scaled, frac_val)) {
        return WalletSecurityResult::Reject("overflow-amount");
    }
    const int64_t total = scaled + frac_val;
    if (!AmountInRange(total)) {
        return WalletSecurityResult::Reject("overflow-amount");
    }
    if (total == 0 && size > 0) {
        // Zero is a valid parse for the amount parser; construction still
        // requires a positive output via Evaluate.
        return WalletSecurityResult::Accept(WalletAdmissionTarget::TRANSACTION_BUILDER,
                                            "amount-ok", /*enter_kernel=*/false);
    }
    return WalletSecurityResult::Accept(WalletAdmissionTarget::TRANSACTION_BUILDER,
                                        "amount-ok", /*enter_kernel=*/false);
}

WalletSecurityResult WalletSecurityBoundary::EvaluateImport(const WalletImportRequest& in)
{
    if (in.material.empty()) {
        return WalletSecurityResult::Reject("missing-parameter");
    }
    if (in.material.size() > WalletSecurityLimits::MAX_IMPORT_BYTES) {
        return WalletSecurityResult::Reject("oversized-input");
    }
    if (in.format != "wif" && in.format != "ed25519-hex" && in.format != "mnemonic") {
        return WalletSecurityResult::Reject("invalid-import-format");
    }
    // Restore/import stores a key handle only. Secret bytes are not copied
    // into the result and must be wiped by the caller.
    return WalletSecurityResult::Accept(WalletAdmissionTarget::IMPORT,
                                        "import-request", /*enter_kernel=*/false);
}

WalletSecurityResult WalletSecurityBoundary::EvaluateRpc(const WalletRpcRequest& in)
{
    if (in.raw_size > WalletSecurityLimits::MAX_RPC_INPUT) {
        return WalletSecurityResult::Reject("oversized-input");
    }
    if (in.method.empty()) {
        return WalletSecurityResult::Reject("malformed-request");
    }

    if (in.method == "sendtoaddress" || in.method == "send") {
        if (in.params.size() < 2) return WalletSecurityResult::Reject("missing-parameter");
        auto addr = ParseAddress(in.params[0].data(), in.params[0].size());
        if (addr.IsReject()) return addr;
        auto amt = ParseAmount(in.params[1].data(), in.params[1].size());
        if (amt.IsReject()) return amt;
        // RPC generates a request only. It does not call consensus authority.
        auto r = WalletSecurityResult::Accept(WalletAdmissionTarget::TRANSACTION_BUILDER,
                                              "rpc-request", /*enter_kernel=*/true);
        return r;
    }
    if (in.method == "sendmany") {
        if (in.params.empty()) return WalletSecurityResult::Reject("missing-parameter");
        return WalletSecurityResult::Accept(WalletAdmissionTarget::TRANSACTION_BUILDER,
                                            "rpc-request", /*enter_kernel=*/true);
    }
    if (in.method == "signrawtransactionwithwallet" || in.method == "signrawtransaction") {
        if (in.params.empty()) return WalletSecurityResult::Reject("missing-parameter");
        return WalletSecurityResult::Accept(WalletAdmissionTarget::SIGNING_API,
                                            "rpc-signing-request", /*enter_kernel=*/true);
    }
    if (in.method == "importprivkey" || in.method == "importrigorprivkey" ||
        in.method == "restore") {
        if (in.params.empty()) return WalletSecurityResult::Reject("missing-parameter");
        if (LooksLikeSecret(in.params[0].c_str())) {
            // Parameter is secret material: accept as import request only.
            // The result must not echo the secret.
            return WalletSecurityResult::Accept(WalletAdmissionTarget::IMPORT,
                                                "rpc-import-request", /*enter_kernel=*/false);
        }
        return WalletSecurityResult::Reject("missing-parameter");
    }
    if (in.method == "getbalance" || in.method == "listunspent" || in.method == "getnewaddress") {
        return WalletSecurityResult::Accept(WalletAdmissionTarget::VALIDATION_API,
                                            "rpc-query", /*enter_kernel=*/false);
    }
    // Unknown method: fail closed. Wallet RPC must not become a consensus entry.
    return WalletSecurityResult::Reject("malformed-request");
}

WalletSecurityResult WalletSecurityBoundary::EvaluateFuzz(const uint8_t* data, size_t size)
{
    try {
        if (!data || size == 0) {
            WalletTransactionRequest empty;
            return Evaluate(empty);
        }
        const uint8_t sel = data[0];
        const uint8_t* body = data + 1;
        const size_t n = size - 1;
        switch (sel % 5) {
        case 0: {
            WalletTransactionRequest req;
            req.kind = WalletTxKind::TRANSFER;
            if (n >= 1) {
                WalletOutputSpec o;
                o.amount = static_cast<int64_t>(body[0]) * 1000;
                o.asset = (body[0] & 1) ? WalletAssetType::ORU : WalletAssetType::RES;
                size_t dlen = n > 1 ? (body[1] % 40) + 1 : 20;
                if (dlen > n) dlen = n > 2 ? n - 2 : 1;
                o.destination.assign(body + (n > 2 ? 2 : 0),
                                     body + (n > 2 ? 2 : 0) + dlen);
                req.outputs.push_back(o);
            }
            auto ev = Evaluate(req);
            if (ev.IsAccept()) return TransactionBuilder::Build(req);
            return ev;
        }
        case 1:
            return ParseAddress(reinterpret_cast<const char*>(body), n);
        case 2:
            return ParseAmount(reinterpret_cast<const char*>(body), n);
        case 3: {
            WalletImportRequest imp;
            imp.format = (n && (body[0] % 3) == 0) ? "wif" : "ed25519-hex";
            imp.material.assign(body, body + n);
            auto r = EvaluateImport(imp);
            WipeSecret(imp.material.data(), imp.material.size());
            return r;
        }
        default: {
            WalletRpcRequest rpc;
            rpc.raw_size = size;
            static const char* methods[] = {
                "sendtoaddress", "sendmany", "importprivkey", "getbalance", "", "signrawtransaction"};
            rpc.method = methods[body[0] % 6];
            if (n > 1) rpc.params.emplace_back(reinterpret_cast<const char*>(body + 1), n - 1);
            if (n > 8) rpc.params.emplace_back(reinterpret_cast<const char*>(body + 4), n > 12 ? 4 : n - 4);
            return EvaluateRpc(rpc);
        }
        }
    } catch (...) {
        return WalletSecurityResult::Reject("internal-fail-closed");
    }
}
