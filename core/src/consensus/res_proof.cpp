#include <consensus/res_proof.h>

#include <crypto/ed25519_provider.h>
#include <pubkey.h>
#include <script/rigor_tags.h>
#include <script/script.h>

#include <algorithm>

// PCORB is a retired mint carrier. Production consensus only detects the
// tagged RIGOR_MARKER_RETURN so CheckTransaction / CheckTxInputs can reject it
// (`bad-pcorb-carrier`). The historical decoder lives in test/util/pcorb_carrier.cpp.

namespace Consensus {

bool IsPcorbCarrierScript(const CScript& script)
{
    // RFC-009 §0: one byte representation. The length test pins the whole script, so there is no
    // room for a trailing opcode, and 120 bytes has no minimal encoding other than PUSHDATA1.
    if (script.size() != RigorTags::PCORB_CARRIER_SCRIPT_BYTES) return false;
    if (script[0] != RIGOR_MARKER_RETURN) return false;
    if (script[1] != RIGOR_PUSHDATA1) return false;
    if (script[2] != RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES) return false;
    return std::equal(RigorTags::PCOR.begin(), RigorTags::PCOR.end(), script.begin() + 3);
}

bool IsPcorbCarrierOutput(const CTxOut& out)
{
    return out.nValue == 0 && IsPcorbCarrierScript(out.scriptPubKey);
}

size_t CountPcorbCarrierOutputs(const CTransaction& tx)
{
    size_t n = 0;
    for (const CTxOut& out : tx.vout) {
        if (IsPcorbCarrierOutput(out)) ++n;
    }
    return n;
}

bool DeriveResWitnessProgramFromPubKey(const CPubKey& pubkey, std::vector<unsigned char>& program32_out)
{
    program32_out.clear();
    const std::vector<unsigned char> pk32 = pubkey.GetEd25519Payload();
    if (pk32.size() != ed25519::PUBKEY_SIZE) return false;
    program32_out.resize(32);
    if (!ed25519::ResWitnessProgram(pk32, program32_out)) {
        program32_out.clear();
        return false;
    }
    return true;
}

} // namespace Consensus
