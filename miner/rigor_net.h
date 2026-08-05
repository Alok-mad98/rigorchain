#ifndef RIGOR_NET_H
#define RIGOR_NET_H

/*
 * Rigor testnet: Bitcoin-style P2P message start (4-byte magic before each message).
 * Must match CTestNetParams::pchMessageStart[] in ../core/src/kernel/chainparams.cpp.
 *
 * Upstream minerd only uses JSON-RPC (HTTP) to talk to the node; it does not open
 * a raw P2P socket or prefix messages with this magic. This header is the canonical
 * constant for forks, tests, or external tools that must match rigord -testnet.
 */

#define RIGOR_TESTNET_MESSAGE_START_0    0x0c
#define RIGOR_TESTNET_MESSAGE_START_1    0x12
#define RIGOR_TESTNET_MESSAGE_START_2    0x0a
#define RIGOR_TESTNET_MESSAGE_START_3    0x08

static inline void rigor_testnet_message_start_fill(unsigned char out[4])
{
	out[0] = RIGOR_TESTNET_MESSAGE_START_0;
	out[1] = RIGOR_TESTNET_MESSAGE_START_1;
	out[2] = RIGOR_TESTNET_MESSAGE_START_2;
	out[3] = RIGOR_TESTNET_MESSAGE_START_3;
}

/** @return 1 if @a p matches Rigor testnet pchMessageStart, else 0 */
static inline int rigor_testnet_message_start_eq(const unsigned char p[4])
{
	return p[0] == RIGOR_TESTNET_MESSAGE_START_0 &&
	       p[1] == RIGOR_TESTNET_MESSAGE_START_1 &&
	       p[2] == RIGOR_TESTNET_MESSAGE_START_2 &&
	       p[3] == RIGOR_TESTNET_MESSAGE_START_3;
}

#endif /* RIGOR_NET_H */
