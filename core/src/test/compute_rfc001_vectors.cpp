// Standalone program to compute RFC-001 golden test vectors (Sigma-PoW-01: parent block hash seed).
#include <chainparams.h>
#include <consensus/rigor_math.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergence_root.h>
#include <consensus/draw_digest.h>
#include <crypto/common.h>
#include <hash.h>
#include <uint256.h>
#include <arith_uint256.h>
#include <util/translation.h>
#include <cstring>
#include <iostream>

// Required by libbitcoin_util (clientversion.cpp) when linking without rigord.
const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

int main() {
    SelectParams(CBaseChainParams::MAIN);
    
    std::cout << "RFC-001 Golden Test Vector Computation" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // CASE-002: single_tx_winner
    {
        std::cout << "\nCASE-002: single_tx_winner" << std::endl;
        std::vector<uint256> txids = {uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")};
        int height = 1;
        uint256 prev_hash = uint256S("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
        uint256 C = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
        std::cout << "C (Convergence Root): " << C.GetHex() << std::endl;
        
        uint256 sigma = SelectionDigest(txids[0], prev_hash, height, C);
        std::cout << "Sigma: " << sigma.GetHex() << std::endl;
    }
    
    // CASE-004: no_winner
    {
        std::cout << "\nCASE-004: no_winner" << std::endl;
        std::vector<uint256> txids = {
            uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
            uint256S("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
        };
        std::vector<CAmount> fees = {1, 1};
        int height = 5;
        uint256 prev_hash = uint256S("cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc");
        uint256 C = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
        std::cout << "C: " << C.GetHex() << std::endl;
        
        for (size_t i = 0; i < txids.size(); ++i) {
            uint256 h = SelectionDigest(txids[i], prev_hash, height, C);
            // RFC-014 Conv-Decouple-01: inequality RHS = constitutional constant
            // (this is a vector generator; the constitutional constant produces the
            // new RFC-012 golden vectors that the consensus path now expects).
            bool eligible = Consensus::ConvergenceDrawInequalityHolds(h, Params().GetConsensus().RES_PROBABILITY_TARGET, fees[i]);
            std::cout << "txid[" << i << "]: " << txids[i].GetHex() << std::endl;
            std::cout << "  Sigma: " << h.GetHex() << std::endl;
            std::cout << "  Eligible: " << (eligible ? "true" : "false") << std::endl;
        }
    }
    
    // CASE-005: weight_priority
    {
        std::cout << "\nCASE-005: weight_priority" << std::endl;
        std::vector<uint256> txids = {
            uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
            uint256S("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
        };
        std::vector<CAmount> fees = {1, 1000};
        int height = 7;
        uint256 prev_hash = uint256S("dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
        uint256 C = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
        std::cout << "C: " << C.GetHex() << std::endl;
        
        uint256 h0 = SelectionDigest(txids[0], prev_hash, height, C);
        uint256 h1 = SelectionDigest(txids[1], prev_hash, height, C);
        std::cout << "txid[0] Sigma: " << h0.GetHex() << std::endl;
        std::cout << "txid[1] Sigma: " << h1.GetHex() << std::endl;
        
        arith_uint256 H0 = UintToArith256(h0);
        arith_uint256 H1 = UintToArith256(h1);
        Consensus::uint512 lhs = Consensus::rigor_math_detail::Mul256x256Full(H0, arith_uint256(fees[1]));
        Consensus::uint512 rhs = Consensus::rigor_math_detail::Mul256x256Full(H1, arith_uint256(fees[0]));
        int cmp = lhs.Compare(rhs);
        std::cout << "Winner comparison (H0*f1 vs H1*f0): " << cmp << " (<0: txid[0] wins, >0: txid[1] wins)" << std::endl;
    }
    
    // CASE-006: tie_break
    {
        std::cout << "\nCASE-006: tie_break" << std::endl;
        std::vector<uint256> txids = {
            uint256S("1111111111111111111111111111111111111111111111111111111111111111"),
            uint256S("2222222222222222222222222222222222222222222222222222222222222222")
        };
        int cmp = std::memcmp(txids[0].begin(), txids[1].begin(), 32);
        std::cout << "txid[0]: " << txids[0].GetHex() << std::endl;
        std::cout << "txid[1]: " << txids[1].GetHex() << std::endl;
        std::cout << "txid[0] < txid[1]: " << (cmp < 0 ? "true" : "false") << std::endl;
        std::cout << "Winner: txid[" << (cmp < 0 ? "0" : "1") << "]" << std::endl;
    }
    
    // CASE-007: big_int
    {
        std::cout << "\nCASE-007: big_int" << std::endl;
        std::vector<uint256> txids = {
            uint256::ONE,
            uint256S("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee")
        };
        std::vector<CAmount> fees = {CAmount(18446744073709551615ULL), 1};
        int height = 100;
        uint256 prev_hash = uint256S("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        uint256 C = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
        std::cout << "C: " << C.GetHex() << std::endl;
        
        uint256 h0 = SelectionDigest(txids[0], prev_hash, height, C);
        uint256 h1 = SelectionDigest(txids[1], prev_hash, height, C);
        std::cout << "txid[0] Sigma: " << h0.GetHex() << std::endl;
        std::cout << "txid[1] Sigma: " << h1.GetHex() << std::endl;
        
        arith_uint256 H0 = UintToArith256(h0);
        arith_uint256 H1 = UintToArith256(h1);
        Consensus::uint512 lhs = Consensus::rigor_math_detail::Mul256x256Full(H0, arith_uint256(fees[1]));
        Consensus::uint512 rhs = Consensus::rigor_math_detail::Mul256x256Full(H1, arith_uint256(fees[0]));
        int cmp = lhs.Compare(rhs);
        std::cout << "Winner comparison (H0*f1 vs H1*f0): " << cmp << " (<0: txid[0] wins, >0: txid[1] wins)" << std::endl;
    }
    
    return 0;
}
