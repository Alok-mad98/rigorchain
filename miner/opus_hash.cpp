#include "opus_hash.h"

#include <stdint.h>
#include <string.h>
#include <cstdio>

extern "C" {
void sha256d(unsigned char *hash, const unsigned char *data, int len);
void bin2hex(char *s, const unsigned char *p, size_t len);
void applog(int prio, const char *fmt, ...);
}

extern bool opt_debug_pow;


static inline uint32_t rotl32(uint32_t v, int n)
{
	return (v << n) | (v >> (32 - n));
}

static inline uint32_t le32_load(const uint8_t *p)
{
	return ((uint32_t)p[0]) |
	       ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

static inline void le64_store(uint8_t *p, uint64_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
	p[4] = (uint8_t)(v >> 32);
	p[5] = (uint8_t)(v >> 40);
	p[6] = (uint8_t)(v >> 48);
	p[7] = (uint8_t)(v >> 56);
}

static inline uint64_t le64_load(const uint8_t *p)
{
	return ((uint64_t)p[0]) |
	       ((uint64_t)p[1] << 8) |
	       ((uint64_t)p[2] << 16) |
	       ((uint64_t)p[3] << 24) |
	       ((uint64_t)p[4] << 32) |
	       ((uint64_t)p[5] << 40) |
	       ((uint64_t)p[6] << 48) |
	       ((uint64_t)p[7] << 56);
}

static inline uint32_t be32_load(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) |
	       ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) |
	       ((uint32_t)p[3]);
}

static inline void be32_store(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static inline void le32_store(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static inline void chacha20_qr(uint32_t &a, uint32_t &b, uint32_t &c, uint32_t &d)
{
	a += b; d ^= a; d = rotl32(d, 16);
	c += d; b ^= c; b = rotl32(b, 12);
	a += b; d ^= a; d = rotl32(d, 8);
	c += d; b ^= c; b = rotl32(b, 7);
}

static void chacha20_block(uint32_t state[16], uint8_t out[64])
{
	uint32_t x[16];
	int i;

	memcpy(x, state, sizeof(x));

	for (i = 0; i < 10; i++) {
		chacha20_qr(x[0], x[4], x[8], x[12]);
		chacha20_qr(x[1], x[5], x[9], x[13]);
		chacha20_qr(x[2], x[6], x[10], x[14]);
		chacha20_qr(x[3], x[7], x[11], x[15]);
		chacha20_qr(x[0], x[5], x[10], x[15]);
		chacha20_qr(x[1], x[6], x[11], x[12]);
		chacha20_qr(x[2], x[7], x[8], x[13]);
		chacha20_qr(x[3], x[4], x[9], x[14]);
	}

	for (i = 0; i < 16; i++) {
		x[i] += state[i];
		le32_store(out + 4 * i, x[i]);
	}

	state[12]++;
	if (state[12] == 0)
		state[13]++;
}

static void chacha20_fill_64k(const uint8_t key[32], uint8_t out[65536])
{
	uint32_t state[16];
	int i;

	state[0] = 0x61707865U;
	state[1] = 0x3320646eU;
	state[2] = 0x79622d32U;
	state[3] = 0x6b206574U;
	for (i = 0; i < 8; i++)
		state[4 + i] = le32_load(key + 4 * i);
	state[12] = 0;
	state[13] = 0;
	state[14] = 0;
	state[15] = 0;

	for (i = 0; i < 1024; i++)
		chacha20_block(state, out + i * 64);
}

void opus_hash_from_base_hash(const uint8_t *base_hash, uint8_t *output32)
{
	uint8_t current[32];
	uint8_t buffer[65536];
	uint8_t msg40[40];
	uint8_t msg64[64];
	uint8_t tmp32[32];
	uint64_t mem_val;
	uint32_t val, idx;
	int round;

	chacha20_fill_64k(base_hash, buffer);
	memcpy(current, base_hash, 32);

	for (round = 0; round < 256; round++) {
		val = le32_load(current);
		idx = (val ^ ((uint32_t)round << 24)) & (65536U - 1U);
		if (idx > 65536U - 8U)
			idx = 65536U - 8U;

		mem_val = le64_load(buffer + idx);

		memcpy(msg40, current, 32);
		le64_store(msg40 + 32, mem_val);

		/* Debug: print buf40 for round 0 */
		if (round == 0 && opt_debug_pow) {
			char hexbuf[81];
			bin2hex(hexbuf, msg40, 40);
			applog(4, "MINER buf40 (round 0): %s", hexbuf);
		}

		/* Node uses CHashWriter (double-SHA256) each round. */
		sha256d(tmp32, msg40, 40);
		memcpy(current, tmp32, 32);
	}

	memcpy(msg64, base_hash, 32);
	memcpy(msg64 + 32, current, 32);
	/* Node uses CHashWriter for final hash too */
	/* Bitcoin CHashWriter serializes uint256 as 32 raw bytes, same as memcpy */
	sha256d(output32, msg64, 64);
}

void opus_hash(const uint8_t *input80, uint8_t *output32)
{
	uint8_t base_hash[32];

	sha256d(base_hash, input80, 80);

	/* Debug: print input80 (80-byte header) for comparison with node */
	if (opt_debug_pow) {
		char hexbuf[161];
		bin2hex(hexbuf, input80, 80);
		applog(4, "MINER opus_hash input80 (HEADER): %s", hexbuf);
	}

	opus_hash_from_base_hash(base_hash, output32);
}
