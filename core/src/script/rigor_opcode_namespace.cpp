// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/rigor_opcode_namespace.h>

RigorOpcodeClass ClassifyRigorOpcode(unsigned int b)
{
    if (b == static_cast<unsigned>(RIGOR_BURNED_CVG_CLAIM)) {
        return RigorOpcodeClass::BURNED;
    }
    switch (ClassifyRigorByte(b)) {
    case RigorByteClass::DATA_PUSH:
        return RigorOpcodeClass::PUSH_MARKER;
    case RigorByteClass::RES_MINT_MARKER:
    case RigorByteClass::CONVERGE_MARKER:
    case RigorByteClass::RETURN_MARKER:
        return RigorOpcodeClass::ASSET_MARKER;
    case RigorByteClass::RESERVED_INERT:
        return RigorOpcodeClass::RESERVED_INERT;
    case RigorByteClass::INVALID:
        return RigorOpcodeClass::INVALID;
    }
    return RigorOpcodeClass::INVALID;
}
