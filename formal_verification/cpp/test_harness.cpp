// Rigor Consensus C++ Test Harness for Golden Vector Export
// =========================================================
//
// This test harness exports golden vectors from the C++ implementation
// for cross-validation with the Python executable spec.
//
// CRITICAL: This code MUST match the Python spec byte-for-byte.
// Any mismatch indicates a consensus-critical bug.
//
// FORBIDDEN:
// - Any hash function other than SHA256d (RFC 6234 compliant)
// - Endian conversions not explicitly specified
// - Sorting by anything other than memcmp byte order
// - Any optimization that changes byte-level behavior

#include <consensus/convergence_root.h>
#include <consensus/convergence_logic.h>
#include <uint256.h>
#include <hash.h>
#include <serialize.h>
#include <streams.h>

using namespace Consensus;

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

// =============================================================================
// JSON Export Helper Functions
// =============================================================================

std::string uint256_to_hex(const uint256& hash)
{
    std::stringstream ss;
    ss << hash.ToString();
    return ss.str();
}

std::string bytes_to_hex(const unsigned char* data, size_t len)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return ss.str();
}

// =============================================================================
// Golden Vector Export Functions
// =============================================================================

/**
 * Export ConvergenceRoot golden vector.
 * 
 * This function computes ConvergenceRoot from a list of txids and exports
 * the result in JSON format for comparison with Python spec.
 */
void export_convergence_root_vector(
    const std::string& test_name,
    const std::vector<uint256>& txids)
{
    // Compute ConvergenceRoot using C++ implementation
    uint256 cpp_root = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
    
    // Export to JSON
    std::cout << "{\"test_name\": \"" << test_name << "\", "
              << "\"txids\": [";
    
    for (size_t i = 0; i < txids.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << "\"" << uint256_to_hex(txids[i]) << "\"";
    }
    
    std::cout << "], "
              << "\"convergence_root\": \"" << uint256_to_hex(cpp_root) << "\""
              << "}" << std::endl;
}

/**
 * Export PCORB hash golden vector.
 * 
 * This function computes PCORB hash and exports the result in JSON format.
 * 
 * CRITICAL: This uses SHA256d exactly as specified in RFC 6234.
 * NO OPTIMIZATION VARIANTS ALLOWED.
 */
void export_pcobr_hash_vector(
    const std::string& test_name,
    const uint256& txid,
    const uint256& bind_hash,
    int height,
    uint32_t target_prev,
    const uint256& convergence_root)
{
    // Compute PCORB hash using C++ implementation
    // This calls Reconvergence which uses DrawDigest (SHA256d)
    uint256 cpp_hash = Reconvergence(
        txid, bind_hash, height, target_prev, convergence_root);
    
    // Export to JSON
    std::cout << "{\"test_name\": \"" << test_name << "\", "
              << "\"txid\": \"" << uint256_to_hex(txid) << "\", "
              << "\"bind_hash\": \"" << uint256_to_hex(bind_hash) << "\", "
              << "\"height\": " << height << ", "
              << "\"target_prev\": " << target_prev << ", "
              << "\"convergence_root\": \"" << uint256_to_hex(convergence_root) << "\", "
              << "\"pcorb_hash\": \"" << uint256_to_hex(cpp_hash) << "\""
              << "}" << std::endl;
}

// =============================================================================
// Test Vector Generation
// =============================================================================

/**
 * Generate deterministic test txid from seed.
 * Matches Python spec: sha256d(seed.to_bytes(4, 'little'))
 */
uint256 create_test_txid(uint32_t seed)
{
    // Serialize seed as little-endian 4 bytes
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << seed;
    
    // Double SHA-256 (SHA256d)
    CHash256 hasher;
    hasher.Write(MakeUCharSpan(ss));
    uint256 result;
    hasher.Finalize(result);
    
    // Second SHA-256
    CHash256 hasher2;
    hasher2.Write(MakeUCharSpan(result));
    uint256 final_result;
    hasher2.Finalize(final_result);
    
    return final_result;
}

/**
 * Generate all ConvergenceRoot golden vectors.
 * Matches Python spec test vectors exactly.
 */
void generate_convergence_root_vectors()
{
    // Vector 1: Empty list (< 2 txids) -> all-zero
    std::vector<uint256> empty_txids;
    export_convergence_root_vector("ConvergenceRoot_empty", empty_txids);
    
    // Vector 2: Single txid (< 2 txids) -> all-zero
    std::vector<uint256> single_txid;
    single_txid.push_back(create_test_txid(0));
    export_convergence_root_vector("ConvergenceRoot_single", single_txid);
    
    // Vector 3: Two txids (minimum for hashing)
    std::vector<uint256> two_txids;
    two_txids.push_back(create_test_txid(0));
    two_txids.push_back(create_test_txid(1));
    export_convergence_root_vector("ConvergenceRoot_two", two_txids);
    
    // Vector 4: Multiple txids
    std::vector<uint256> multiple_txids;
    for (int i = 0; i < 5; ++i) {
        multiple_txids.push_back(create_test_txid(i));
    }
    export_convergence_root_vector("ConvergenceRoot_multiple", multiple_txids);
    
    // Vector 5: Order invariance - different order, same result
    std::vector<uint256> shuffled_txids;
    shuffled_txids.push_back(create_test_txid(3));
    shuffled_txids.push_back(create_test_txid(1));
    shuffled_txids.push_back(create_test_txid(4));
    shuffled_txids.push_back(create_test_txid(0));
    shuffled_txids.push_back(create_test_txid(2));
    export_convergence_root_vector("ConvergenceRoot_order_invariance", shuffled_txids);
}

/**
 * Generate all PCORB hash golden vectors.
 * Matches Python spec test vectors exactly.
 */
void generate_pcobr_hash_vectors()
{
    uint256 txid = create_test_txid(0);
    uint256 bind_hash = create_test_txid(1);
    uint256 convergence_root = create_test_txid(2);
    int height = 100;
    uint32_t target_prev = 0x1d00ffff;
    
    // Vector 1: Basic case
    export_pcobr_hash_vector(
        "PCORBHash_basic",
        txid, bind_hash, height, target_prev, convergence_root);
    
    // Vector 2: Different height
    int height2 = 999999;
    export_pcobr_hash_vector(
        "PCORBHash_different_height",
        txid, bind_hash, height2, target_prev, convergence_root);
    
    // Vector 3: Different target
    uint32_t target_prev2 = 0x1e012345;
    export_pcobr_hash_vector(
        "PCORBHash_different_target",
        txid, bind_hash, height, target_prev2, convergence_root);
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    
    std::cout << "[" << std::endl;
    
    // Generate ConvergenceRoot vectors
    generate_convergence_root_vectors();
    
    // Generate PCORB hash vectors
    generate_pcobr_hash_vectors();
    
    std::cout << "]" << std::endl;
    
    return 0;
}
