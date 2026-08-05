// Rigor Consensus Standalone C++ Test Harness
// ==========================================
//
// This is a standalone implementation that matches the Python spec
// byte-for-byte, using OpenSSL SHA256 for cryptographic operations.
//
// CRITICAL: This implementation MUST match Python spec exactly.
// Any mismatch indicates a consensus-critical bug.

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <openssl/sha.h>

// =============================================================================
// SHA256 Implementation (OpenSSL - RFC 6234 compliant)
// =============================================================================

// SHA256d = SHA256(SHA256(data))
// CRITICAL: This uses OpenSSL SHA256 which is RFC 6234 compliant
// NO VARIANTS ALLOWED
void sha256d(const std::vector<unsigned char>& data, unsigned char* output) {
    unsigned char temp[SHA256_DIGEST_LENGTH];
    
    // First SHA256
    SHA256(data.data(), data.size(), temp);
    
    // Second SHA256
    SHA256(temp, SHA256_DIGEST_LENGTH, output);
}

// =============================================================================
// Helper Functions
// =============================================================================

std::string bytes_to_hex(const unsigned char* data, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return ss.str();
}

// memcmp-style comparison for sorting
bool memcmp_compare(const std::vector<unsigned char>& a, const std::vector<unsigned char>& b) {
    size_t min_len = std::min(a.size(), b.size());
    for (size_t i = 0; i < min_len; ++i) {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return a.size() < b.size();
}

// =============================================================================
// ConvergenceRoot Implementation
// =============================================================================

std::vector<unsigned char> compute_convergence_root(const std::vector<std::vector<unsigned char>>& txids) {
    // Rule 1: If |F| < 2, return 32 zero bytes
    if (txids.size() < 2) {
        return std::vector<unsigned char>(32, 0);
    }
    
    // Rule 2: Sort by memcmp byte order
    std::vector<std::vector<unsigned char>> sorted_txids = txids;
    std::sort(sorted_txids.begin(), sorted_txids.end(), memcmp_compare);
    
    // Rule 3: Concatenate sorted txids
    std::vector<unsigned char> concatenated;
    for (const auto& txid : sorted_txids) {
        concatenated.insert(concatenated.end(), txid.begin(), txid.end());
    }
    
    // Rule 4: SHA256d
    unsigned char root[32];
    sha256d(concatenated, root);
    
    return std::vector<unsigned char>(root, root + 32);
}

// =============================================================================
// PCORB Hash Implementation
// =============================================================================

std::vector<unsigned char> compute_pcobr_hash(
    const std::vector<unsigned char>& txid,
    const std::vector<unsigned char>& bind_hash,
    int height,
    uint32_t target_prev,
    const std::vector<unsigned char>& convergence_root)
{
    // Build preimage: txid || bind_hash || LE64(height) || LE32(target_prev) || convergence_root
    std::vector<unsigned char> preimage;
    
    preimage.insert(preimage.end(), txid.begin(), txid.end());
    preimage.insert(preimage.end(), bind_hash.begin(), bind_hash.end());
    
    // LE64(height) - little-endian 64-bit
    for (int i = 0; i < 8; ++i) {
        preimage.push_back(static_cast<unsigned char>((static_cast<uint64_t>(height) >> (i * 8)) & 0xFF));
    }
    
    // LE32(target_prev) - little-endian 32-bit
    for (int i = 0; i < 4; ++i) {
        preimage.push_back(static_cast<unsigned char>((target_prev >> (i * 8)) & 0xFF));
    }
    
    preimage.insert(preimage.end(), convergence_root.begin(), convergence_root.end());
    
    // SHA256d
    unsigned char hash[32];
    sha256d(preimage, hash);
    
    return std::vector<unsigned char>(hash, hash + 32);
}

// =============================================================================
// Test Vector Generation
// =============================================================================

std::vector<unsigned char> create_test_txid(uint32_t seed) {
    // Match Python: sha256d(seed.to_bytes(4, 'little'))
    std::vector<unsigned char> seed_bytes;
    for (int i = 0; i < 4; ++i) {
        seed_bytes.push_back(static_cast<unsigned char>((seed >> (i * 8)) & 0xFF));
    }
    
    unsigned char txid[32];
    sha256d(seed_bytes, txid);
    
    return std::vector<unsigned char>(txid, txid + 32);
}

void export_convergence_root_vector(const std::string& test_name, const std::vector<std::vector<unsigned char>>& txids, bool is_last = false) {
    std::vector<unsigned char> root = compute_convergence_root(txids);
    
    std::cout << "{\"test_name\": \"" << test_name << "\", "
              << "\"txids\": [";
    
    for (size_t i = 0; i < txids.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << "\"" << bytes_to_hex(txids[i].data(), txids[i].size()) << "\"";
    }
    
    std::cout << "], "
              << "\"convergence_root\": \"" << bytes_to_hex(root.data(), root.size()) << "\""
              << "}";
    
    if (!is_last) std::cout << ",";
    std::cout << std::endl;
}

void export_pcobr_hash_vector(
    const std::string& test_name,
    const std::vector<unsigned char>& txid,
    const std::vector<unsigned char>& bind_hash,
    int height,
    uint32_t target_prev,
    const std::vector<unsigned char>& convergence_root,
    bool is_last = false)
{
    std::vector<unsigned char> hash = compute_pcobr_hash(txid, bind_hash, height, target_prev, convergence_root);
    
    std::cout << "{\"test_name\": \"" << test_name << "\", "
              << "\"txid\": \"" << bytes_to_hex(txid.data(), txid.size()) << "\", "
              << "\"bind_hash\": \"" << bytes_to_hex(bind_hash.data(), bind_hash.size()) << "\", "
              << "\"height\": " << height << ", "
              << "\"target_prev\": " << target_prev << ", "
              << "\"convergence_root\": \"" << bytes_to_hex(convergence_root.data(), convergence_root.size()) << "\", "
              << "\"pcorb_hash\": \"" << bytes_to_hex(hash.data(), hash.size()) << "\""
              << "}";
    
    if (!is_last) std::cout << ",";
    std::cout << std::endl;
}

void generate_convergence_root_vectors() {
    // Vector 1: Empty list
    std::vector<std::vector<unsigned char>> empty;
    export_convergence_root_vector("ConvergenceRoot_empty", empty, false);
    
    // Vector 2: Single txid
    std::vector<std::vector<unsigned char>> single;
    single.push_back(create_test_txid(0));
    export_convergence_root_vector("ConvergenceRoot_single", single, false);
    
    // Vector 3: Two txids
    std::vector<std::vector<unsigned char>> two;
    two.push_back(create_test_txid(0));
    two.push_back(create_test_txid(1));
    export_convergence_root_vector("ConvergenceRoot_two", two, false);
    
    // Vector 4: Multiple txids
    std::vector<std::vector<unsigned char>> multiple;
    for (int i = 0; i < 5; ++i) {
        multiple.push_back(create_test_txid(i));
    }
    export_convergence_root_vector("ConvergenceRoot_multiple", multiple, false);
    
    // Vector 5: Order invariance
    std::vector<std::vector<unsigned char>> shuffled;
    shuffled.push_back(create_test_txid(3));
    shuffled.push_back(create_test_txid(1));
    shuffled.push_back(create_test_txid(4));
    shuffled.push_back(create_test_txid(0));
    shuffled.push_back(create_test_txid(2));
    export_convergence_root_vector("ConvergenceRoot_order_invariance", shuffled, false);
}

void generate_pcobr_hash_vectors() {
    auto txid = create_test_txid(0);
    auto bind_hash = create_test_txid(1);
    auto convergence_root = create_test_txid(2);
    int height = 100;
    uint32_t target_prev = 0x1d00ffff;
    
    export_pcobr_hash_vector("PCORBHash_basic", txid, bind_hash, height, target_prev, convergence_root, false);
    
    int height2 = 999999;
    export_pcobr_hash_vector("PCORBHash_different_height", txid, bind_hash, height2, target_prev, convergence_root, false);
    
    uint32_t target_prev2 = 0x1e012345;
    export_pcobr_hash_vector("PCORBHash_different_target", txid, bind_hash, height, target_prev2, convergence_root, true);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "[" << std::endl;
    
    generate_convergence_root_vectors();
    generate_pcobr_hash_vectors();
    
    std::cout << "]" << std::endl;
    
    return 0;
}
