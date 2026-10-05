#include <xjwt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    uint64_t count = argc > 1 ? strtoull(argv[1], NULL, 10) : 10000u;
    double start = xrtTimer();
    if (!count) return 1;
    for (uint64_t i = 0; i < count; ++i) {
        xvalue* claims = xrtValueObject(); if (!claims) return 1; char* token = xjwtHs256(claims, "benchmark-secret", 60); xrtValueRelease(claims); if (!token) return 1; xvalue* verified = xjwtVerify(token, "benchmark-secret", NULL); xrtFree(token); if (!verified) return 1; xrtValueRelease(verified);
    }
    double elapsed = xrtTimer() - start;
    printf("ops_per_sec: %.3f\n", (double)count / (elapsed > 0.0 ? elapsed : 0.000001));
    printf("checksum: %" PRIu64 "\n", count);
    return 0;
}
