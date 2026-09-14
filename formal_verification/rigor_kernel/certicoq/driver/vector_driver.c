/* Phase 6 trusted vector driver.
 *
 * TCB glue. Not the checker. Not CompCert. Not a Level 5 claim.
 *
 * Calls the CertiCoq [phase6_run] closure produced from the same
 * Observe.v literals as INVALID-001…005 / smokes. Does not
 * reconstruct boxed Z from strtoll (that would be a second,
 * unproved encoder). Prints the static record table for audit.
 *
 * Coq bool := true | false. true is constructor 0 = Val_int(0) = 1
 * (see generated hello Hello.f.c). false is constructor 1 = 3.
 */
#include <stdio.h>
#include <stdlib.h>
#include "gc_stack.h"
#include "values.h"

extern value body(struct thread_info *);
extern struct thread_info *make_tinfo(void);

#define NFIELDS 12

static const char *names[NFIELDS] = {
    "INVALID-001", "INVALID-002", "INVALID-003", "INVALID-004",
    "INVALID-005", "invalid_script", "smoke_utxo", "smoke_oru",
    "smoke_res", "smoke_script", "smoke_pow", "smoke_asert"
};

/* First six must be false; last six must be true. */
static const int expect_true[NFIELDS] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1};

static int is_coq_true(value v)
{
    return Is_long(v) && v == Val_int(0);
}

static int is_coq_false(value v)
{
    return Is_long(v) && v == Val_int(1);
}

static int decode_bool(value v, int *out)
{
    if (is_coq_true(v)) {
        *out = 1;
        return 1;
    }
    if (is_coq_false(v)) {
        *out = 0;
        return 1;
    }
    return 0;
}

int main(void)
{
    struct thread_info *tinfo;
    value rec;
    int i, ok, bit, rejected, smokes, failed;

    printf("COMPILER_CORRECTNESS=false\n");
    printf("verified_compiler_in_current_tcb=false\n");
    printf("BINARY_SEMANTIC_EQUIVALENCE=false\n");
    printf("DRIVER=TCB_GLUE\n");
    printf("DRIVER_NOT_THE_CHECKER=true\n");

    printf("VECTOR_TABLE=Observe.v\n");
    printf("INVALID-001 utxo over MAX_MONEY expect=INVALID\n");
    printf("INVALID-002 oru 7->10 DomORU expect=INVALID\n");
    printf("INVALID-003 res coinbase=true expect=INVALID\n");
    printf("INVALID-004 asert expected_q=0 expect=INVALID\n");
    printf("INVALID-005 pow compact_t=-1 expect=INVALID\n");

    tinfo = make_tinfo();
    if (tinfo == NULL) {
        fprintf(stderr, "make_tinfo failed\n");
        return 2;
    }
    rec = body(tinfo);

    if (Is_long(rec)) {
        fprintf(stderr, "PHASE6_BODY_NOT_RECORD: got unboxed %ld\n",
                (long)rec);
        return 2;
    }
    if (Wosize_val(rec) < NFIELDS) {
        fprintf(stderr, "PHASE6_BODY_FIELD_COUNT=%lu expected>=%d\n",
                (unsigned long)Wosize_val(rec), NFIELDS);
        return 2;
    }

    ok = 1;
    rejected = 1;
    smokes = 1;
    for (i = 0; i < NFIELDS; i++) {
        if (!decode_bool(Field(rec, i), &bit)) {
            printf("%s=UNDECodable\n", names[i]);
            ok = 0;
            continue;
        }
        failed = (bit && !expect_true[i]) || (!bit && expect_true[i]);
        if (i < 6) {
            if (bit)
                rejected = 0;
            printf("%s=%s\n", names[i], bit ? "ACCEPTED" : "REJECTED");
        } else {
            if (!bit)
                smokes = 0;
            printf("%s=%s\n", names[i], bit ? "PASS" : "FAIL");
        }
        if (failed)
            ok = 0;
    }

    printf("INVALID_001_005=%s\n", rejected ? "REJECTED" : "FAIL");
    printf("SMOKES=%s\n", smokes ? "PASS" : "FAIL");
    if (ok && rejected && smokes) {
        printf("PHASE6_VECTORS=PASS\n");
        return 0;
    }
    printf("PHASE6_VECTORS=FAIL\n");
    return 1;
}
