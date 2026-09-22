/* RGRPOW01 compat CLI: miner C implementation (rgrpow_hash80). */
#include "rgrpow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_hex(const char *hex, unsigned char *out, int want)
{
    int n = (int)strlen(hex);
    if (n != want * 2)
        return -1;
    for (int i = 0; i < want; i++) {
        unsigned int b;
        if (sscanf(hex + 2 * i, "%2x", &b) != 1)
            return -1;
        out[i] = (unsigned char)b;
    }
    return 0;
}

int main(int argc, char **argv)
{
    unsigned char header[80], digest[32];
    if (argc != 2) {
        fprintf(stderr, "usage: hash80_c <header80_hex>\n");
        return 2;
    }
    if (parse_hex(argv[1], header, 80) != 0) {
        fprintf(stderr, "header must be 160 hex chars\n");
        return 2;
    }
    rgrpow_hash80(header, digest);
    for (int i = 0; i < 32; i++)
        printf("%02x", digest[i]);
    printf("\n");
    return 0;
}
