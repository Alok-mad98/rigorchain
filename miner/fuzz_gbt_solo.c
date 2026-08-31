#include "gbt_solo.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * Fail-closed GBT fuzz: malformed JSON must reject, never construct a
 * legacy 2-push coinbase, and must not crash.
 *
 * Build: same objects as test_gbt_solo, entry fuzz_gbt_solo.c
 * LibFuzzer: -fsanitize=fuzzer (optional).
 */

static const uint8_t kPayoutV0[22] = {
	0x00, 0x14,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int fuzz_one(const uint8_t *data, size_t size)
{
	json_error_t e;
	json_t *gbt;
	struct gbt_solo_out out;
	char err[256];
	char *buf;

	if (size > 1 << 20)
		return 0;
	buf = malloc(size + 1);
	if (!buf)
		return 0;
	memcpy(buf, data, size);
	buf[size] = 0;
	gbt = json_loads(buf, &e);
	free(buf);
	if (!gbt)
		return 0;
	if (gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err))) {
		if (out.scriptsig_len < 2 || out.scriptsig_len > 100)
			abort();
		if (out.has_mint && out.mint_spk_len > 0 &&
		    out.mint_spk[0] >= 0x51 && out.mint_spk[0] <= 0x60)
			abort();
		gbt_solo_out_free(&out);
	} else {
		gbt_solo_out_free(&out);
	}
	json_decref(gbt);
	return 0;
}

#ifdef __AFL_COMPILER
int main(void)
{
	uint8_t *data = NULL;
	size_t n = 0;
	ssize_t r;
	FILE *f = stdin;
	uint8_t tmp[4096];
	size_t cap = 0;
	while ((r = fread(tmp, 1, sizeof(tmp), f)) > 0) {
		uint8_t *nbuf = realloc(data, cap + (size_t)r);
		if (!nbuf) {
			free(data);
			return 1;
		}
		data = nbuf;
		memcpy(data + cap, tmp, (size_t)r);
		cap += (size_t)r;
	}
	n = cap;
	fuzz_one(data, n);
	free(data);
	return 0;
}
#elif defined(LIBFUZZER)
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	return fuzz_one(data, size);
}
#else
static int run_file(const char *path)
{
	uint8_t buf[1 << 16];
	size_t n;
	FILE *f = fopen(path, "rb");

	if (!f) {
		fprintf(stderr, "fuzz_gbt_solo: cannot open %s\n", path);
		return 1;
	}
	n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	return fuzz_one(buf, n);
}

/*
 * Standalone driver. With a directory argument it replays every seed in the
 * corpus, which is what `make check` uses to keep the seeds honest.
 */
int main(int argc, char **argv)
{
	uint8_t buf[1 << 16];
	size_t n;
	struct stat st;

	if (argc > 1) {
		if (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) {
			DIR *d = opendir(argv[1]);
			struct dirent *ent;
			int count = 0;

			if (!d)
				return 1;
			while ((ent = readdir(d)) != NULL) {
				char path[4096];

				if (ent->d_name[0] == '.')
					continue;
				if (snprintf(path, sizeof(path), "%s/%s", argv[1],
					     ent->d_name) >= (int)sizeof(path))
					continue;
				if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
					continue;
				if (run_file(path) != 0) {
					closedir(d);
					return 1;
				}
				count++;
			}
			closedir(d);
			if (count == 0) {
				fprintf(stderr, "fuzz_gbt_solo: no seeds in %s\n", argv[1]);
				return 1;
			}
			printf("fuzz_gbt_solo: %d seeds replayed\n", count);
			return 0;
		}
		return run_file(argv[1]);
	}
	n = fread(buf, 1, sizeof(buf), stdin);
	return fuzz_one(buf, n);
}
#endif
