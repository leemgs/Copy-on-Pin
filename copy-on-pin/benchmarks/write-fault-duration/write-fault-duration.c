#include <stdio.h>
#include <float.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define WARMUP_ITERATIONS 10

static double get_seconds(void)
{
    struct timezone tzp;
    struct timeval tp;

    gettimeofday(&tp,&tzp);
    return (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6;
}

int main(int argc, char *argv[])
{
    const size_t size = 1024*1024*1024u;
    unsigned long iterations;
    double istart, istop;
    unsigned char *mem;
    unsigned int i, j;
    int ret;

    if (argc < 2) {
        fprintf(stderr, "Usage: ./write-fault-duration $ITERATIONS\n");
        return 1;
    }
    iterations = strtoull(argv[1], NULL, 10);
    if (iterations < 1) {
        fprintf(stderr, "Iterations has to be > 0.\n");
        return 1;
    }

    mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (mem == MAP_FAILED) {
            fprintf(stderr, "MMAP failed.\n");
            return 1;
    }

    printf("ns/access\n");

    memset(mem, 0, size);
    for (i = 0; i < iterations + WARMUP_ITERATIONS; i++) {

        /* Map the pages read-only. */
        ret = mprotect(mem, size, PROT_READ);
        ret |= mprotect(mem, size, PROT_READ|PROT_WRITE);
        if (ret) {
            fprintf(stderr, "mprotect() failed\n");
            return 1;
        }

        istart = get_seconds();
        /* Touch each page once. */
        for (j = 0; j < size; j += 4096)
            mem[j] = j;
        istop = get_seconds();

        if (i < WARMUP_ITERATIONS)
            continue;

        printf("%llu\n", (unsigned long long)(((istop - istart) * 1000 * 1000 * 1000) / (size / 4096)));
    }

    return 0;
}

