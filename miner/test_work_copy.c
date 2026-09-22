/*
 * Regression for work_copy: dest must not alias src heap fields.
 * Mirrors the helpers in cpu-miner.c (keep the two in sync).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct work {
	uint32_t data[32];
	char *txs;
	char *workid;
	char *job_id;
	size_t xnonce2_len;
	unsigned char *xnonce2;
	char **convergence_txids;
	int convergence_txid_count;
	char *prevblockhash;
};

static void work_free(struct work *w)
{
	int i;

	if (!w)
		return;
	free(w->txs);
	w->txs = NULL;
	free(w->workid);
	w->workid = NULL;
	free(w->job_id);
	w->job_id = NULL;
	free(w->xnonce2);
	w->xnonce2 = NULL;
	if (w->convergence_txids) {
		for (i = 0; i < w->convergence_txid_count; i++)
			free(w->convergence_txids[i]);
		free(w->convergence_txids);
		w->convergence_txids = NULL;
		w->convergence_txid_count = 0;
	}
	free(w->prevblockhash);
	w->prevblockhash = NULL;
}

static bool work_copy(struct work *dest, const struct work *src)
{
	struct work tmp;

	if (!dest || !src)
		return false;

	memcpy(&tmp, src, sizeof(tmp));
	tmp.txs = NULL;
	tmp.workid = NULL;
	tmp.job_id = NULL;
	tmp.xnonce2 = NULL;
	tmp.convergence_txids = NULL;
	tmp.convergence_txid_count = 0;
	tmp.prevblockhash = NULL;

	if (src->txs) {
		tmp.txs = strdup(src->txs);
		if (!tmp.txs)
			goto fail;
	}
	if (src->workid) {
		tmp.workid = strdup(src->workid);
		if (!tmp.workid)
			goto fail;
	}
	if (src->job_id) {
		tmp.job_id = strdup(src->job_id);
		if (!tmp.job_id)
			goto fail;
	}
	if (src->prevblockhash) {
		tmp.prevblockhash = strdup(src->prevblockhash);
		if (!tmp.prevblockhash)
			goto fail;
	}
	if (src->xnonce2 && src->xnonce2_len) {
		tmp.xnonce2 = malloc(src->xnonce2_len);
		if (!tmp.xnonce2)
			goto fail;
		memcpy(tmp.xnonce2, src->xnonce2, src->xnonce2_len);
	}
	if (src->convergence_txids && src->convergence_txid_count > 0) {
		int i;

		tmp.convergence_txids = calloc((size_t)src->convergence_txid_count,
					       sizeof(char *));
		if (!tmp.convergence_txids)
			goto fail;
		tmp.convergence_txid_count = src->convergence_txid_count;
		for (i = 0; i < src->convergence_txid_count; i++) {
			if (!src->convergence_txids[i])
				continue;
			tmp.convergence_txids[i] = strdup(src->convergence_txids[i]);
			if (!tmp.convergence_txids[i])
				goto fail;
		}
	}

	work_free(dest);
	memcpy(dest, &tmp, sizeof(tmp));
	return true;

fail:
	work_free(&tmp);
	return false;
}

static int fail(const char *msg)
{
	fprintf(stderr, "FAIL: %s\n", msg);
	return 1;
}

int main(void)
{
	struct work src;
	struct work dest;
	char *src_txs;

	memset(&src, 0, sizeof(src));
	memset(&dest, 0, sizeof(dest));
	src.data[1] = 0xabcdef01;
	src.txs = strdup("deadbeef");
	src.workid = strdup("wid-1");
	if (!src.txs || !src.workid)
		return fail("setup strdup");

	src_txs = src.txs;
	if (!work_copy(&dest, &src))
		return fail("work_copy");
	if (dest.txs == src.txs)
		return fail("dest.txs aliases src.txs");
	if (!dest.txs || strcmp(dest.txs, "deadbeef") != 0)
		return fail("dest.txs content");
	if (dest.workid == src.workid)
		return fail("dest.workid aliases src.workid");
	if (dest.data[1] != 0xabcdef01)
		return fail("scalar fields not copied");

	work_free(&dest);
	if (src.txs != src_txs || strcmp(src.txs, "deadbeef") != 0)
		return fail("work_free(dest) mutated src");

	if (work_copy(&dest, NULL))
		return fail("NULL src must fail");
	if (dest.txs)
		return fail("failed copy mutated dest");

	work_free(&src);
	printf("PASS test_work_copy\n");
	return 0;
}
