// C++ side of the codec differential.
//
// Reads  "id \t kind \t hex"  lines on stdin.
// Writes "id \t status \t leftover \t reencoded_hex" on stdout.
//
// Same contract as fuzz/codec_differential/oracle_main.ml, so the two
// outputs can be compared line by line.
//
// This deliberately uses the production deserializer -- the same
// CDataStream >> CBlock path that DecodeHexBlk drives -- rather than a
// transcription of it. A transcription would only test the
// transcription. DecodeHexBlk itself is not called because it collapses
// "decoded with bytes to spare" and "decoded exactly" into one bool,
// and that distinction is the whole point of the strict/wire split on
// the Coq side.

#include <primitives/block.h>
#include <primitives/transaction.h>
#include <streams.h>
#include <util/strencodings.h>
#include <version.h>

#include <cstdio>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

void Emit(const std::string& id, const char* status, const std::string& leftover,
          const std::string& hex)
{
    std::printf("%s\t%s\t%s\t%s\n", id.c_str(), status, leftover.c_str(), hex.c_str());
}

template <typename T>
void Run(const std::string& id, const std::vector<unsigned char>& data)
{
    T obj;
    CDataStream ss(data, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ss >> obj;
    } catch (const std::exception&) {
        Emit(id, "fail", "-", "-");
        return;
    }
    const size_t leftover = ss.size();
    CDataStream out(SER_NETWORK, PROTOCOL_VERSION);
    out << obj;
    Emit(id, "ok", std::to_string(leftover), HexStr(MakeUCharSpan(out)));
}

} // namespace

int main()
{
    std::ios_base::sync_with_stdio(false);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        const size_t t1 = line.find('\t');
        if (t1 == std::string::npos) continue;
        const size_t t2 = line.find('\t', t1 + 1);
        if (t2 == std::string::npos) continue;

        const std::string id = line.substr(0, t1);
        const std::string kind = line.substr(t1 + 1, t2 - t1 - 1);
        const std::string hex = line.substr(t2 + 1);

        // Not IsHex(): it rejects the empty string, which would report the
        // empty input as a hex-layer problem on this side and as an ordinary
        // decode failure on the Coq side. The empty input is a legitimate
        // decoder input and both sides must be asked about it.
        if (!hex.empty() && !IsHex(hex)) {
            Emit(id, "badhex", "-", "-");
            continue;
        }
        const std::vector<unsigned char> data = ParseHex(hex);

        if (kind == "block") {
            Run<CBlock>(id, data);
        } else if (kind == "tx") {
            Run<CMutableTransaction>(id, data);
        } else {
            Emit(id, "badkind", "-", "-");
        }
    }
    std::fflush(stdout);
    return 0;
}
