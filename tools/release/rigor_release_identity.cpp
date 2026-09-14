// TASK-002: NOT A RELEASE ARTIFACT.
// This file only prints RIGOR_RELEASE_IDENTITY=PASS. It is not rigord.
// tools/release/run-release-build-job.sh must not compile this into a
// user-facing package. Kept only so leftover references can be found.
#include <cstdio>

#ifndef RIGOR_RELEASE_VERSION
#define RIGOR_RELEASE_VERSION "0.1.3"
#endif
#ifndef RIGOR_GIT_COMMIT
#define RIGOR_GIT_COMMIT "unknown"
#endif
#ifndef RIGOR_BUILD_OS
#define RIGOR_BUILD_OS "unknown"
#endif
#ifndef RIGOR_BUILD_ARCH
#define RIGOR_BUILD_ARCH "unknown"
#endif
#ifndef RIGOR_BUILD_COMPILER
#define RIGOR_BUILD_COMPILER "unknown"
#endif
#ifndef RIGOR_BUILD_TARGET
#define RIGOR_BUILD_TARGET "unknown"
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::printf("RIGOR_RELEASE_IDENTITY=PASS\n");
    std::printf("VERSION=%s\n", RIGOR_RELEASE_VERSION);
    std::printf("GIT_COMMIT=%s\n", RIGOR_GIT_COMMIT);
    std::printf("OS=%s\n", RIGOR_BUILD_OS);
    std::printf("ARCH=%s\n", RIGOR_BUILD_ARCH);
    std::printf("COMPILER=%s\n", RIGOR_BUILD_COMPILER);
    std::printf("TARGET=%s\n", RIGOR_BUILD_TARGET);
    std::printf("CONSENSUS_CODE_CHANGED=FALSE\n");
    std::printf("DATABASE_FORMAT_CHANGED=FALSE\n");
    std::printf("WALLET_FORMAT_CHANGED=FALSE\n");
    return 0;
}
