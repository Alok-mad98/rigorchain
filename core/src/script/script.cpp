// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/script.h>

#include <util/strencodings.h>


unsigned int CScript::CountScriptOpsLegacyTestOnly(bool fAccurate) const
{
    unsigned int n = 0;
    const_iterator pc = begin();
    opcodetype lastOpcode = RIGOR_BYTE_INVALID;
    while (pc < end())
    {
        opcodetype opcode;
        if (!GetOp(pc, opcode))
            break;
        if (opcode == RIGOR_RESERVED_LEGACY_CHECKSIG || opcode == RIGOR_RESERVED_LEGACY_CHECKSIGVERIFY || opcode == RIGOR_MARKER_RES_MINT)
            n++;
        else if (opcode == RIGOR_RESERVED_LEGACY_CHECKMULTISIG || opcode == RIGOR_RESERVED_LEGACY_CHECKMULTISIGVERIFY)
        {
            if (fAccurate && lastOpcode >= RIGOR_PUSH_1 && lastOpcode <= RIGOR_PUSH_16)
                n += DecodeOP_N(lastOpcode);
            else
                n += MAX_PUBKEYS_PER_MULTISIG;
        }
        lastOpcode = opcode;
    }
    return n;
}

unsigned int CScript::CountScriptOpsLegacyTestOnly(const CScript& scriptSig) const
{
    if (!IsPayToScriptHash())
        return CountScriptOpsLegacyTestOnly(true);

    // This is a pay-to-script-hash scriptPubKey;
    // get the last item that the scriptSig
    // pushes onto the stack:
    const_iterator pc = scriptSig.begin();
    std::vector<unsigned char> vData;
    while (pc < scriptSig.end())
    {
        opcodetype opcode;
        if (!scriptSig.GetOp(pc, opcode, vData))
            return 0;
        if (opcode > RIGOR_PUSH_16)
            return 0;
    }

    /// ... and return its opcount:
    CScript subscript(vData.begin(), vData.end());
    return subscript.CountScriptOpsLegacyTestOnly(true);
}

bool CScript::IsPayToScriptHash() const
{
    // Extra-fast test for pay-to-script-hash CScripts:
    return (this->size() == 23 &&
            (*this)[0] == RIGOR_RESERVED_LEGACY_HASH160 &&
            (*this)[1] == 0x14 &&
            (*this)[22] == RIGOR_RESERVED_LEGACY_EQUAL);
}

bool CScript::IsPayToWitnessScriptHash() const
{
    // Extra-fast test for pay-to-witness-script-hash CScripts:
    return (this->size() == 34 &&
            (*this)[0] == RIGOR_PUSH_0 &&
            (*this)[1] == 0x20);
}

// A witness program is any valid CScript that consists of a 1-byte push opcode
// followed by a data push between 2 and 40 bytes.
bool CScript::IsWitnessProgram(int& version, std::vector<unsigned char>& program) const
{
    if (this->size() < 4 || this->size() > 42) {
        return false;
    }
    if ((*this)[0] != RIGOR_PUSH_0 && ((*this)[0] < RIGOR_PUSH_1 || (*this)[0] > RIGOR_PUSH_16)) {
        return false;
    }
    if ((size_t)((*this)[1] + 2) == this->size()) {
        version = DecodeOP_N((opcodetype)(*this)[0]);
        program = std::vector<unsigned char>(this->begin() + 2, this->end());
        return true;
    }
    return false;
}

bool CScript::IsPushOnly(const_iterator pc) const
{
    while (pc < end())
    {
        opcodetype opcode;
        if (!GetOp(pc, opcode))
            return false;
        // Note that IsPushOnly() *does* consider RIGOR_RESERVED_INERT_50 to be a
        // push-type opcode, however execution of RIGOR_RESERVED_INERT_50 fails, so
        // it's not relevant to P2SH/BIP62 as the scriptSig would fail prior to
        // the P2SH special validation code being executed.
        if (opcode > RIGOR_PUSH_16)
            return false;
    }
    return true;
}

bool CScript::IsPushOnly() const
{
    return this->IsPushOnly(begin());
}

std::string CScriptWitness::ToString() const
{
    std::string ret = "CScriptWitness(";
    for (unsigned int i = 0; i < stack.size(); i++) {
        if (i) {
            ret += ", ";
        }
        ret += HexStr(stack[i]);
    }
    return ret + ")";
}

bool CScript::HasValidOps() const
{
    CScript::const_iterator it = begin();
    while (it < end()) {
        opcodetype opcode;
        std::vector<unsigned char> item;
        if (!GetOp(it, opcode, item) || opcode > MAX_OPCODE || opcode == RIGOR_BURNED_CVG_CLAIM ||
            item.size() > MAX_SCRIPT_ELEMENT_SIZE) {
            return false;
        }
    }
    return true;
}

bool GetScriptOp(CScriptBase::const_iterator& pc, CScriptBase::const_iterator end, opcodetype& opcodeRet, std::vector<unsigned char>* pvchRet)
{
    opcodeRet = RIGOR_BYTE_INVALID;
    if (pvchRet)
        pvchRet->clear();
    if (pc >= end)
        return false;

    // Read instruction
    if (end - pc < 1)
        return false;
    unsigned int opcode = *pc++;

    // Immediate operand
    if (opcode <= RIGOR_PUSHDATA4)
    {
        unsigned int nSize = 0;
        if (opcode < RIGOR_PUSHDATA1)
        {
            nSize = opcode;
        }
        else if (opcode == RIGOR_PUSHDATA1)
        {
            if (end - pc < 1)
                return false;
            nSize = *pc++;
        }
        else if (opcode == RIGOR_PUSHDATA2)
        {
            if (end - pc < 2)
                return false;
            nSize = ReadLE16(&pc[0]);
            pc += 2;
        }
        else if (opcode == RIGOR_PUSHDATA4)
        {
            if (end - pc < 4)
                return false;
            nSize = ReadLE32(&pc[0]);
            pc += 4;
        }
        if (end - pc < 0 || (unsigned int)(end - pc) < nSize)
            return false;
        if (pvchRet)
            pvchRet->assign(pc, pc + nSize);
        pc += nSize;
    }

    opcodeRet = static_cast<opcodetype>(opcode);
    return true;
}

bool IsOpSuccess(const opcodetype& opcode)
{
    return opcode == 80 || opcode == 98 || (opcode >= 126 && opcode <= 129) ||
           (opcode >= 131 && opcode <= 134) || (opcode >= 137 && opcode <= 138) ||
           (opcode >= 141 && opcode <= 142) || (opcode >= 149 && opcode <= 153) ||
           (opcode >= 190 && opcode <= 254);
}

bool CheckMinimalPush(const std::vector<unsigned char>& data, opcodetype opcode) {
    // Excludes RIGOR_RESERVED_LEGACY_1NEGATE, RIGOR_PUSH_1-16 since they are by definition minimal
    assert(0 <= opcode && opcode <= RIGOR_PUSHDATA4);
    if (data.size() == 0) {
        // Should have used RIGOR_PUSH_0.
        return opcode == RIGOR_PUSH_0;
    } else if (data.size() == 1 && data[0] >= 1 && data[0] <= 16) {
        // Should have used RIGOR_PUSH_1 .. RIGOR_PUSH_16.
        return false;
    } else if (data.size() == 1 && data[0] == 0x81) {
        // Should have used RIGOR_RESERVED_LEGACY_1NEGATE.
        return false;
    } else if (data.size() <= 75) {
        // Must have used a direct push (opcode indicating number of bytes pushed + those bytes).
        return opcode == data.size();
    } else if (data.size() <= 255) {
        // Must have used OP_PUSHDATA.
        return opcode == RIGOR_PUSHDATA1;
    } else if (data.size() <= 65535) {
        // Must have used RIGOR_PUSHDATA2.
        return opcode == RIGOR_PUSHDATA2;
    }
    return true;
}
