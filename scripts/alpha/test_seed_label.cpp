#include <qt/seedlabel.h>

#include <cstdio>
#include <string>

static int fails = 0;

static void expect(const char* label, const SeedNameResult& got, SeedNameMatch match, const std::string& name)
{
    if (got.match != match || got.name != name) {
        std::fprintf(stderr, "FAIL %s match=%d name=%s\n", label, static_cast<int>(got.match), got.name.c_str());
        ++fails;
    }
}

int main()
{
    const ConfiguredSeedLookup node_a{"seed.rigorchain.com", {"167.88.172.182"}, true};
    const ConfiguredSeedLookup seed1{"seed1.rigorchain.com", {"80.96.108.2"}, true};
    const ConfiguredSeedLookup pending{"seed.rigorchain.com", {}, false};
    const ConfiguredSeedLookup failed{"seed1.rigorchain.com", {}, true};
    const std::vector<ConfiguredSeedLookup> both{node_a, seed1};

    expect("both-node-a", UniqueConfiguredSeedName("167.88.172.182", both), SeedNameMatch::Unique, "seed.rigorchain.com");
    expect("both-seed1", UniqueConfiguredSeedName("80.96.108.2", both), SeedNameMatch::Unique, "seed1.rigorchain.com");
    expect("only-node-a", UniqueConfiguredSeedName("167.88.172.182", {node_a}), SeedNameMatch::Unique, "seed.rigorchain.com");
    expect("only-node-a-other-absent", UniqueConfiguredSeedName("80.96.108.2", {node_a}), SeedNameMatch::None, "");
    expect("only-seed1", UniqueConfiguredSeedName("80.96.108.2", {seed1}), SeedNameMatch::Unique, "seed1.rigorchain.com");
    expect("none", UniqueConfiguredSeedName("167.88.172.182", {}), SeedNameMatch::None, "");
    expect("discovered", UniqueConfiguredSeedName("203.0.113.9", both), SeedNameMatch::None, "");
    expect("lookup-failed", UniqueConfiguredSeedName("80.96.108.2", {node_a, failed}), SeedNameMatch::None, "");
    expect("lookup-pending", UniqueConfiguredSeedName("167.88.172.182", {pending, seed1}), SeedNameMatch::None, "");
    expect("disconnected", UniqueConfiguredSeedName("198.51.100.10", both), SeedNameMatch::None, "");
    expect("ambiguous", UniqueConfiguredSeedName("167.88.172.182", {node_a, ConfiguredSeedLookup{"other.example", {"167.88.172.182"}, true}}), SeedNameMatch::Ambiguous, "");
    expect("port-is-not-identity", UniqueConfiguredSeedName("167.88.172.182:8760", both), SeedNameMatch::None, "");
    expect("empty-ip", UniqueConfiguredSeedName("", both), SeedNameMatch::None, "");

    if (fails) return 1;
    std::puts("SEED_LABEL_OK");
    return 0;
}
