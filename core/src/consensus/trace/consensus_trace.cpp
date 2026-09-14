// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/trace/consensus_trace.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {

std::atomic<bool> g_enabled{false};
std::string g_out_dir;
std::mutex g_write_mutex;

struct NetworkContext {
    int peer_count{-1};
    std::string source;
    int64_t peer_id{-1};
    int64_t noted_ms{0};
};

std::mutex g_env_mutex;
std::unordered_map<std::string, NetworkContext> g_env;
std::deque<std::string> g_env_fifo;
constexpr size_t ENV_CACHE_MAX = 4096;

void RememberNetworkContext(const std::string& block_hash, NetworkContext ctx)
{
    std::lock_guard<std::mutex> lock(g_env_mutex);
    if (g_env.find(block_hash) == g_env.end()) {
        g_env_fifo.push_back(block_hash);
        if (g_env_fifo.size() > ENV_CACHE_MAX) {
            g_env.erase(g_env_fifo.front());
            g_env_fifo.pop_front();
        }
    }
    g_env[block_hash] = std::move(ctx);
}

bool LookupNetworkContext(const std::string& block_hash, NetworkContext& out)
{
    std::lock_guard<std::mutex> lock(g_env_mutex);
    auto it = g_env.find(block_hash);
    if (it == g_env.end()) return false;
    out = it->second;
    return true;
}

struct TxRow {
    std::string txid;
    std::string function;
    bool inputs_checked{false};
    bool utxo_result{false};
    bool script_result{false};
    bool amount_result{false};
    int64_t fee_result{0};
    std::string reason;
};

struct Session {
    std::string block_hash;
    int height{-1};
    bool header_ok{true};
    std::string header_function;
    std::string header_reason;
    bool pow_ok{false};
    bool pow_set{false};
    std::string pow_reason;
    bool asert_ok{false};
    bool asert_set{false};
    unsigned asert_expected{0};
    unsigned asert_got{0};
    bool checkblock_ok{true};
    std::string checkblock_reason;
    bool contextual_block_ok{true};
    std::string contextual_block_reason;
    std::vector<TxRow> txs;
};

thread_local Session* g_session = nullptr;
thread_local bool g_sticky_pow_ok = false;
thread_local bool g_sticky_pow_set = false;
thread_local std::string g_sticky_pow_reason;
thread_local bool g_sticky_asert_ok = false;
thread_local bool g_sticky_asert_set = false;
thread_local unsigned g_sticky_asert_expected = 0;
thread_local unsigned g_sticky_asert_got = 0;
thread_local bool g_sticky_header_ok = true;
thread_local std::string g_sticky_header_fn;
thread_local std::string g_sticky_header_reason;

std::string JsonEscape(const std::string& s)
{
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            o.push_back('\\');
            o.push_back(static_cast<char>(c));
        } else if (c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            o += buf;
        } else {
            o.push_back(static_cast<char>(c));
        }
    }
    return o;
}

const char* BoolJson(bool v) { return v ? "true" : "false"; }

std::string KernelCandidateJson(const Session& s)
{
    bool utxo = true;
    bool script = true;
    bool amount = true;
    int64_t fees = 0;
    for (const TxRow& t : s.txs) {
        if (t.function == "CheckTxInputs" && t.inputs_checked && !t.utxo_result) utxo = false;
        if (t.function == "VerifyRigorPredicate" && !t.script_result) script = false;
        if (t.function == "CheckTransaction" && !t.script_result) script = false;
        if (t.function == "CheckTransaction" && !t.amount_result) amount = false;
        fees += t.fee_result;
    }
    const bool pow = s.pow_set ? s.pow_ok : true;
    const bool asert = s.asert_set ? s.asert_ok : true;
    std::ostringstream o;
    o << "{\n"
      << "  \"status\": \"oracle_only\",\n"
      << "  \"note\": \"Reconstructed from C++ trace. Not MakeKernelInput. Not Coq KernelInput.\",\n"
      << "  \"ki_utxo_ok\": " << BoolJson(utxo) << ",\n"
      << "  \"ki_script_ok\": " << BoolJson(script && amount) << ",\n"
      << "  \"ki_pow_ok\": " << BoolJson(pow) << ",\n"
      << "  \"ki_asert_ok\": " << BoolJson(asert) << ",\n"
      << "  \"ki_fees\": " << fees << ",\n"
      << "  \"ki_incoming\": null,\n"
      << "  \"ki_outgoing\": null,\n"
      << "  \"ki_burn\": null,\n"
      << "  \"ki_subsidy\": null,\n"
      << "  \"ki_res_mint\": null\n"
      << "}\n";
    return o.str();
}

std::string RejectionLayer(const Session& s, bool accept, const std::string& reject)
{
    if (accept) return "";
    if (!s.header_ok && !s.header_function.empty()) return s.header_function;
    if (s.pow_set && !s.pow_ok) return "CheckProofOfWork";
    if (s.asert_set && !s.asert_ok) return "GetNextWorkRequired";
    if (!s.checkblock_ok) return "CheckBlock";
    if (!s.contextual_block_ok) return "ContextualCheckBlock";
    for (const TxRow& t : s.txs) {
        if (t.function == "CheckTxInputs" && t.inputs_checked && !t.utxo_result) return "CheckTxInputs";
        if (t.function == "VerifyRigorPredicate" && !t.script_result) return "VerifyRigorPredicate";
        if (t.function == "CheckTransaction" && (!t.script_result || !t.amount_result)) return "CheckTransaction";
    }
    return reject.empty() ? "ConnectBlock" : reject;
}

std::string SessionJson(const Session& s, bool accept, const std::string& reject)
{
    const std::string layer = RejectionLayer(s, accept, reject);
    std::ostringstream o;
    o << "{\n"
      << "  \"block_hash\": \"" << JsonEscape(s.block_hash) << "\",\n"
      << "  \"height\": " << s.height << ",\n"
      << "  \"header_result\": \"" << (s.header_ok ? "accept" : "reject") << "\",\n"
      << "  \"header_function\": \"" << JsonEscape(s.header_function) << "\",\n"
      << "  \"header_reason\": \"" << JsonEscape(s.header_reason) << "\",\n"
      << "  \"pow_result\": " << (s.pow_set ? BoolJson(s.pow_ok) : "null") << ",\n"
      << "  \"pow_reason\": \"" << JsonEscape(s.pow_reason) << "\",\n"
      << "  \"asert_result\": " << (s.asert_set ? BoolJson(s.asert_ok) : "null") << ",\n"
      << "  \"asert_expected_nbits\": " << s.asert_expected << ",\n"
      << "  \"asert_got_nbits\": " << s.asert_got << ",\n"
      << "  \"checkblock_result\": \"" << (s.checkblock_ok ? "accept" : "reject") << "\",\n"
      << "  \"checkblock_reason\": \"" << JsonEscape(s.checkblock_reason) << "\",\n"
      << "  \"contextual_checkblock_result\": \"" << (s.contextual_block_ok ? "accept" : "reject") << "\",\n"
      << "  \"tx_results\": [\n";
    for (size_t i = 0; i < s.txs.size(); ++i) {
        const TxRow& t = s.txs[i];
        o << "    {\n"
          << "      \"txid\": \"" << JsonEscape(t.txid) << "\",\n"
          << "      \"function\": \"" << JsonEscape(t.function) << "\",\n"
          << "      \"inputs_checked\": " << BoolJson(t.inputs_checked) << ",\n"
          << "      \"utxo_result\": " << BoolJson(t.utxo_result) << ",\n"
          << "      \"script_result\": " << BoolJson(t.script_result) << ",\n"
          << "      \"amount_result\": " << BoolJson(t.amount_result) << ",\n"
          << "      \"fee_result\": " << t.fee_result << ",\n"
          << "      \"reason\": \"" << JsonEscape(t.reason) << "\"\n"
          << "    }" << (i + 1 < s.txs.size() ? "," : "") << "\n";
    }
    o << "  ],\n"
      << "  \"block_result\": \"" << (accept ? "accept" : "reject") << "\",\n"
      << "  \"block_reason\": \"" << JsonEscape(reject) << "\",\n"
      << "  \"rejection_layer\": " << (layer.empty() ? "null" : ("\"" + JsonEscape(layer) + "\"")) << ",\n"
      << "  \"kernel_candidate\": ";
    std::string kc = KernelCandidateJson(s);
    if (!kc.empty() && kc.back() == '\n') kc.pop_back();
    o << kc << ",\n";
    NetworkContext env;
    const bool have_env = LookupNetworkContext(s.block_hash, env);
    o << "  \"environment_observation\": {\n"
      << "    \"class\": \"environment_observation\",\n"
      << "    \"not_consensus\": true,\n";
    if (have_env) {
        o << "    \"network_context\": {\n"
          << "      \"peer_count\": " << env.peer_count << ",\n"
          << "      \"source\": \"" << JsonEscape(env.source) << "\",\n"
          << "      \"peer_id\": " << env.peer_id << ",\n"
          << "      \"noted_ms\": " << env.noted_ms << "\n"
          << "    }\n";
    } else {
        o << "    \"network_context\": null\n";
    }
    o << "  }\n}\n";
    return o.str();
}

void WriteSession(const Session& s, bool accept, const std::string& reject)
{
    if (g_out_dir.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(g_out_dir, ec);
    const std::string tag = accept ? "accept" : "reject";
    const std::string stem = "h" + std::to_string(s.height) + "-" + s.block_hash.substr(0, 16) + "-" + tag;
    const std::string trace_path = g_out_dir + "/" + stem + "-consensus_trace.json";
    const std::string cand_path = g_out_dir + "/" + stem + "-kernel_candidate.json";
    std::lock_guard<std::mutex> lock(g_write_mutex);
    {
        std::ofstream f(trace_path);
        if (f) f << SessionJson(s, accept, reject);
    }
    {
        std::ofstream f(cand_path);
        if (f) f << KernelCandidateJson(s);
    }
    NetworkContext env;
    if (LookupNetworkContext(s.block_hash, env)) {
        const std::string env_path = g_out_dir + "/" + stem + "-network_context.json";
        std::ofstream f(env_path);
        if (f) {
            f << "{\n"
              << "  \"class\": \"environment_observation\",\n"
              << "  \"not_consensus\": true,\n"
              << "  \"block_hash\": \"" << JsonEscape(s.block_hash) << "\",\n"
              << "  \"peer_count\": " << env.peer_count << ",\n"
              << "  \"source\": \"" << JsonEscape(env.source) << "\",\n"
              << "  \"peer_id\": " << env.peer_id << ",\n"
              << "  \"noted_ms\": " << env.noted_ms << "\n"
              << "}\n";
        }
    }
}

} // namespace

void RigorConsensusTraceInit(bool enabled, const std::string& out_dir)
{
    g_enabled.store(enabled, std::memory_order_relaxed);
    g_out_dir = out_dir;
}

bool RigorConsensusTraceEnabled()
{
    return g_enabled.load(std::memory_order_relaxed);
}

void RigorConsensusTraceNotePow(bool ok, const char* reason)
{
    if (!RigorConsensusTraceEnabled()) return;
    g_sticky_pow_ok = ok;
    g_sticky_pow_set = true;
    g_sticky_pow_reason = reason ? reason : "";
    if (g_session) {
        g_session->pow_ok = ok;
        g_session->pow_set = true;
        g_session->pow_reason = g_sticky_pow_reason;
    }
}

void RigorConsensusTraceNoteAsertExpected(unsigned expected_nbits)
{
    if (!RigorConsensusTraceEnabled()) return;
    g_sticky_asert_expected = expected_nbits;
}

void RigorConsensusTraceNoteAsert(bool ok, unsigned expected_nbits, unsigned got_nbits)
{
    if (!RigorConsensusTraceEnabled()) return;
    g_sticky_asert_ok = ok;
    g_sticky_asert_set = true;
    g_sticky_asert_expected = expected_nbits;
    g_sticky_asert_got = got_nbits;
    if (g_session) {
        g_session->asert_ok = ok;
        g_session->asert_set = true;
        g_session->asert_expected = expected_nbits;
        g_session->asert_got = got_nbits;
    }
}

void RigorConsensusTraceNoteHeader(const char* function, bool ok, const char* reason)
{
    if (!RigorConsensusTraceEnabled()) return;
    g_sticky_header_ok = ok;
    g_sticky_header_fn = function ? function : "";
    g_sticky_header_reason = reason ? reason : "";
    if (g_session) {
        g_session->header_ok = ok;
        g_session->header_function = g_sticky_header_fn;
        g_session->header_reason = g_sticky_header_reason;
    }
}

void RigorConsensusTraceNoteCheckBlock(bool ok, const char* reason)
{
    if (!RigorConsensusTraceEnabled() || !g_session) return;
    g_session->checkblock_ok = ok;
    g_session->checkblock_reason = reason ? reason : "";
}

void RigorConsensusTraceNoteContextualCheckBlock(bool ok, const char* reason)
{
    if (!RigorConsensusTraceEnabled()) return;
    if (g_session) {
        g_session->contextual_block_ok = ok;
        g_session->contextual_block_reason = reason ? reason : "";
    }
}

void RigorConsensusTraceNoteNetworkContext(const uint256& block_hash, int peer_count,
                                           const char* source, int64_t peer_id, int64_t noted_ms)
{
    if (!RigorConsensusTraceEnabled()) return;
    NetworkContext ctx;
    ctx.peer_count = peer_count;
    ctx.source = source ? source : "unknown";
    ctx.peer_id = peer_id;
    ctx.noted_ms = noted_ms;
    RememberNetworkContext(block_hash.GetHex(), std::move(ctx));
}

void RigorConsensusTraceNoteTx(const uint256& txid, const char* function,
                               bool inputs_checked, bool utxo_ok, bool script_ok, bool amount_ok,
                               CAmount fee, const char* reason)
{
    if (!RigorConsensusTraceEnabled() || !g_session) return;
    TxRow row;
    row.txid = txid.GetHex();
    row.function = function ? function : "";
    row.inputs_checked = inputs_checked;
    row.utxo_result = utxo_ok;
    row.script_result = script_ok;
    row.amount_result = amount_ok;
    row.fee_result = fee;
    row.reason = reason ? reason : "";
    g_session->txs.push_back(std::move(row));
}

RigorConsensusTraceConnectGuard::RigorConsensusTraceConnectGuard(const uint256& block_hash, int height, const BlockValidationState& state)
    : m_state(state), m_active(false)
{
    if (!RigorConsensusTraceEnabled()) return;
    static thread_local Session session_storage;
    session_storage = Session{};
    session_storage.block_hash = block_hash.GetHex();
    session_storage.height = height;
    session_storage.pow_ok = g_sticky_pow_ok;
    session_storage.pow_set = g_sticky_pow_set;
    session_storage.pow_reason = g_sticky_pow_reason;
    session_storage.asert_ok = g_sticky_asert_ok;
    session_storage.asert_set = g_sticky_asert_set;
    session_storage.asert_expected = g_sticky_asert_expected;
    session_storage.asert_got = g_sticky_asert_got;
    session_storage.header_ok = g_sticky_header_ok;
    session_storage.header_function = g_sticky_header_fn;
    session_storage.header_reason = g_sticky_header_reason;
    g_session = &session_storage;
    m_active = true;
}

RigorConsensusTraceConnectGuard::~RigorConsensusTraceConnectGuard()
{
    if (!m_active || !g_session) return;
    const bool accept = !m_state.IsInvalid();
    WriteSession(*g_session, accept, m_state.GetRejectReason());
    g_session = nullptr;
}
