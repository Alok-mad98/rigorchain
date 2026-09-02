// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COMPRESSOR_H
#define BITCOIN_COMPRESSOR_H

#include <consensus/amount.h>
#include <prevector.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <serialize.h>
#include <span.h>

#include <ios>

/**
 * This saves us from making many heap allocations when serializing
 * and deserializing compressed scripts.
 *
 * This prevector size is determined by the largest .resize() in the
 * CompressScript function. The largest compressed script format is a
 * compressed public key, which is 33 bytes.
 */
using CompressedScript = prevector<33, unsigned char>;


bool CompressScript(const CScript& script, CompressedScript& out);
unsigned int GetSpecialScriptSize(unsigned int nSize);
bool DecompressScript(CScript& script, unsigned int nSize, const CompressedScript& in);

/**
 * Compress amount.
 *
 * nAmount is of type uint64_t and thus cannot be negative. If you're passing in
 * a CAmount (int64_t), make sure to properly handle the case where the amount
 * is negative before calling CompressAmount(...).
 *
 * @pre Function defined only for 0 <= nAmount <= MAX_MONEY.
 */
uint64_t CompressAmount(uint64_t nAmount);

uint64_t DecompressAmount(uint64_t nAmount);

/** Compact serializer for scripts.
 *
 *  It detects common cases and encodes them much more efficiently.
 *  3 special cases are defined:
 *  * Pay to pubkey hash (encoded as 21 bytes)
 *  * Pay to script hash (encoded as 21 bytes)
 *  * Pay to pubkey starting with 0x02, 0x03 or 0x04 (encoded as 33 bytes)
 *
 *  Other scripts up to 121 bytes require 1 byte + script length. Above
 *  that, scripts up to 16505 bytes require 2 bytes + script length.
 */
struct ScriptCompression
{
    /**
     * make this static for now (there are only 6 special scripts defined)
     * this can potentially be extended together with a new nVersion for
     * transactions, in which case this value becomes dependent on nVersion
     * and nHeight of the enclosing transaction.
     */
    static const unsigned int nSpecialScripts = 6;

    template<typename Stream>
    void Ser(Stream &s, const CScript& script) {
        CompressedScript compr;
        if (CompressScript(script, compr)) {
            s << Span{compr};
            return;
        }
        unsigned int nSize = script.size() + nSpecialScripts;
        s << VARINT(nSize);
        s << Span{script};
    }

    template<typename Stream>
    void Unser(Stream &s, CScript& script) {
        unsigned int nSize = 0;
        s >> VARINT(nSize);
        if (nSize < nSpecialScripts) {
            CompressedScript vch(GetSpecialScriptSize(nSize), 0x00);
            s >> Span{vch};
            if (!DecompressScript(script, nSize, vch)) {
                throw std::ios_base::failure("ScriptCompression: DecompressScript failed (DATABASE_CORRUPTION)");
            }
            return;
        }
        nSize -= nSpecialScripts;
        if (nSize > MAX_SCRIPT_SIZE) {
            throw std::ios_base::failure("ScriptCompression: script exceeds MAX_SCRIPT_SIZE (DATABASE_CORRUPTION)");
        } else {
            script.resize(nSize);
            s >> Span{script};
        }
    }
};

/** Plain script bytes. No Bitcoin P2PKH/P2SH/P2PK special cases (script-as-asset). */
struct ScriptPlainFormatter
{
    template<typename Stream>
    void Ser(Stream &s, const CScript& script) {
        unsigned int nSize = script.size();
        if (nSize > MAX_SCRIPT_SIZE) {
            throw std::ios_base::failure("ScriptPlainFormatter: script exceeds MAX_SCRIPT_SIZE (DATABASE_CORRUPTION)");
        }
        s << VARINT(nSize);
        s << Span{script};
    }

    template<typename Stream>
    void Unser(Stream &s, CScript& script) {
        unsigned int nSize = 0;
        s >> VARINT(nSize);
        if (nSize > MAX_SCRIPT_SIZE) {
            throw std::ios_base::failure("ScriptPlainFormatter: script exceeds MAX_SCRIPT_SIZE (DATABASE_CORRUPTION)");
        }
        script.resize(nSize);
        s >> Span{script};
    }
};

struct AmountCompression
{
    template<typename Stream, typename I> void Ser(Stream& s, I val)
    {
        s << VARINT(CompressAmount(val));
    }
    template<typename Stream, typename I> void Unser(Stream& s, I& val)
    {
        uint64_t v;
        s >> VARINT(v);
        const uint64_t n = DecompressAmount(v);
        if (n > static_cast<uint64_t>(MAX_MONEY)) {
            throw std::ios_base::failure("AmountCompression: nValue out of MoneyRange (DATABASE_CORRUPTION)");
        }
        val = static_cast<I>(n);
    }
};

/** wrapper for CTxOut that provides a compact serialization (amount compression + plain script). */
struct TxOutCompression
{
    FORMATTER_METHODS(CTxOut, obj)
    {
        READWRITE(Using<AmountCompression>(obj.nValue), Using<ScriptPlainFormatter>(obj.scriptPubKey));
    }
};

#endif // BITCOIN_COMPRESSOR_H
