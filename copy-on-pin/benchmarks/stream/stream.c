/*-----------------------------------------------------------------------*/
/* Program: STREAM                                                       */
/* Revision: $Id: stream.c,v 5.10 2013/01/17 16:01:06 mccalpin Exp mccalpin $ */
/* Original code developed by John D. McCalpin                           */
/* Programmers: John D. McCalpin                                         */
/*              Joe R. Zagar                                             */
/*                                                                       */
/* This program measures memory transfer rates in MB/s for simple        */
/* computational kernels coded in C.                                     */
/*-----------------------------------------------------------------------*/
/* Copyright 1991-2013: John D. McCalpin                                 */
/*-----------------------------------------------------------------------*/
/* License:                                                              */
/*  1. You are free to use this program and/or to redistribute           */
/*     this program.                                                     */
/*  2. You are free to modify this program for your own use,             */
/*     including commercial use, subject to the publication              */
/*     restrictions in item 3.                                           */
/*  3. You are free to publish results obtained from running this        */
/*     program, or from works that you derive from this program,         */
/*     with the following limitations:                                   */
/*     3a. In order to be referred to as "STREAM benchmark results",     */
/*         published results must be in conformance to the STREAM        */
/*         Run Rules, (briefly reviewed below) published at              */
/*         http://www.cs.virginia.edu/stream/ref.html                    */
/*         and incorporated herein by reference.                         */
/*         As the copyright holder, John McCalpin retains the            */
/*         right to determine conformity with the Run Rules.             */
/*     3b. Results based on modified source code or on runs not in       */
/*         accordance with the STREAM Run Rules must be clearly          */
/*         labelled whenever they are published.  Examples of            */
/*         proper labelling include:                                     */
/*           "tuned STREAM benchmark results"                            */
/*           "based on a variant of the STREAM benchmark code"           */
/*         Other comparable, clear, and reasonable labelling is          */
/*         acceptable.                                                   */
/*     3c. Submission of results to the STREAM benchmark web site        */
/*         is encouraged, but not required.                              */
/*  4. Use of this program or creation of derived works based on this    */
/*     program constitutes acceptance of these licensing restrictions.   */
/*  5. Absolutely no warranty is expressed or implied.                   */
/*-----------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>

enum mode {
    MODE_DEFAULT = 0,
    MODE_KSM,
    MODE_FORK,
    MODE_SWAP,
    MODE_MPROTECT,
    MODE_MPROTECT_KSM,
    MODE_FORK_KSM,
    MODE_SWAP_FORK,
    MODE_MAX,
};

const char * const mode_to_str[MODE_MAX] = {
    [MODE_DEFAULT] = "default",
    [MODE_KSM] = "ksm",
    [MODE_FORK] = "fork",
    [MODE_SWAP] = "swap",
    [MODE_MPROTECT] = "mprotect",
    [MODE_MPROTECT_KSM] = "mprotect+ksm",
    [MODE_FORK_KSM] = "fork+ksm",
    [MODE_SWAP_FORK] = "swap+fork",
};

static int parse_mode(const char *str)
{
    int i;

    for (i = 0; i < MODE_MAX; i++) {
        if (!strcmp(str, mode_to_str[i]))
            return i;
    }
    return MODE_MAX;
};

volatile double total;

static void touch(const double *val, unsigned long bytes)
{
    unsigned long count = bytes / sizeof(double);

    while (count) {
        total += val[count];
        count--;
    }
}

static unsigned long vmstat_pgcopy(void)
{
    FILE *f = fopen("/proc/vmstat", "r");
    unsigned long val = 0;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int ret;

    if (!f)
        return 0;

    while ((read = getline(&line, &len, f)) != -1) {
        ret = sscanf(line, "pgcopy %lu", &val);
        if (!ret)
            continue;
    }
    fclose(f);

    return val;
}

static void do_fork(void)
{
    int ret;

    fflush(stdout);
    ret = fork();
    if (ret < 0) {
        fprintf(stderr, "fork() failed\n");
        exit(1);
    } else if (!ret) {
        exit(0);
    }
    wait(&ret);
}

static void do_mprotect(double *a, double *b, double *c, unsigned long bytes)
{
    int ret;

    ret = mprotect(a, bytes, PROT_READ);
    ret |= mprotect(b, bytes, PROT_READ);
    ret |= mprotect(c, bytes, PROT_READ);
    ret |= mprotect(a, bytes, PROT_READ|PROT_WRITE);
    ret |= mprotect(b, bytes, PROT_READ|PROT_WRITE);
    ret |= mprotect(c, bytes, PROT_READ|PROT_WRITE);
    if (ret) {
        fprintf(stderr, "mprotect() failed\n");
        exit(1);
    }
}

static void do_swap(double *a, double *b, double *c, unsigned long bytes)
{
    int ret;

    ret = madvise(a, bytes, MADV_PAGEOUT);
    ret |= madvise(b, bytes, MADV_PAGEOUT);
    ret |= madvise(c, bytes, MADV_PAGEOUT);
    if (ret) {
        fprintf(stderr, "madvise() failed\n");
        exit(1);
    }
    touch(a, bytes);
    touch(b, bytes);
    touch(c, bytes);
}

static void do_trigger_ksm(void)
{
    FILE *f = fopen("/sys/kernel/mm/ksm/run", "w");
    const char *val = "1";

    if (!f) {
        fprintf(stderr, "triggering ksm failed\n");
        exit(1);
    }

    if (fputs(val, f) <= 0) {
        fprintf(stderr, "triggering ksm failed\n");
        exit(1);
    }

    fclose(f);
}

/*-----------------------------------------------------------------------
 * INSTRUCTIONS:
 *
 *    1) STREAM requires different amounts of memory to run on different
 *           systems, depending on both the system cache size(s) and the
 *           granularity of the system timer.
 *     You should adjust the value of 'STREAM_ARRAY_SIZE' (below)
 *           to meet *both* of the following criteria:
 *       (a) Each array must be at least 4 times the size of the
 *           available cache memory. I don't worry about the difference
 *           between 10^6 and 2^20, so in practice the minimum array size
 *           is about 3.8 times the cache size.
 *           Example 1: One Xeon E3 with 8 MB L3 cache
 *               STREAM_ARRAY_SIZE should be >= 4 million, giving
 *               an array size of 30.5 MB and a total memory requirement
 *               of 91.5 MB.  
 *           Example 2: Two Xeon E5's with 20 MB L3 cache each (using OpenMP)
 *               STREAM_ARRAY_SIZE should be >= 20 million, giving
 *               an array size of 153 MB and a total memory requirement
 *               of 458 MB.  
 *       (b) The size should be large enough so that the 'timing calibration'
 *           output by the program is at least 20 clock-ticks.  
 *           Example: most versions of Windows have a 10 millisecond timer
 *               granularity.  20 "ticks" at 10 ms/tic is 200 milliseconds.
 *               If the chip is capable of 10 GB/s, it moves 2 GB in 200 msec.
 *               This means the each array must be at least 1 GB, or 128M elements.
 *
 *      Version 5.10 increases the default array size from 2 million
 *          elements to 10 million elements in response to the increasing
 *          size of L3 caches.  The new default size is large enough for caches
 *          up to 20 MB. 
 *      Version 5.10 changes the loop index variables from "register int"
 *          to "ssize_t", which allows array indices >2^32 (4 billion)
 *          on properly configured 64-bit systems.  Additional compiler options
 *          (such as "-mcmodel=medium") may be required for large memory runs.
 *
 *      Array size can be set at compile time without modifying the source
 *          code for the (many) compilers that support preprocessor definitions
 *          on the compile line.  E.g.,
 *                gcc -O -DSTREAM_ARRAY_SIZE=100000000 stream.c -o stream.100M
 *          will override the default size of 10M with a new size of 100M elements
 *          per array.
 */
#ifndef STREAM_ARRAY_SIZE
#   define STREAM_ARRAY_SIZE 44040192
#endif

# define HLINE "-------------------------------------------------------------\n"

# ifndef MIN
# define MIN(x,y) ((x)<(y)?(x):(y))
# endif
# ifndef MAX
# define MAX(x,y) ((x)>(y)?(x):(y))
# endif

static double *a, *b, *c;

#define STREAM_ARRAY_SIZE_BYTES (sizeof(double) * STREAM_ARRAY_SIZE)

static double bytes[4] = {
    2 * STREAM_ARRAY_SIZE_BYTES,
    2 * STREAM_ARRAY_SIZE_BYTES,
    3 * STREAM_ARRAY_SIZE_BYTES,
    3 * STREAM_ARRAY_SIZE_BYTES,
};

#define WARMUP_ITERATIONS 10

static double mysecond();
static void checkSTREAMresults();
int main(int argc, char *argv[])
{
    unsigned long iterations, k, pgcopy_old, pgcopy_new;
    int mode = MODE_DEFAULT;
    double istart, istop;
    double scalar;
    ssize_t j;

    if (argc < 2) {
        fprintf(stderr, "Usage: ./stream $ITERATIONS $MODE\n");
        return 1;
    }
    iterations = strtoull(argv[1], NULL, 10);
    if (iterations < 1) {
        fprintf(stderr, "Iterations has to be > 0.\n");
        return 1;
    }
    if (argc == 3) {
        mode = parse_mode(argv[2]);
        if (mode < 0 || mode >= MODE_MAX) {
            fprintf(stderr, "Unknown mode.\n");
            return 1;
        }
    }

    /* --- SETUP --- determine precision and check timing --- */
    if (sizeof(double) != 8) {
        fprintf(stderr, "sizeof(double) != 8 not supported\n");
        return 1;
    }

    a = aligned_alloc(4096, STREAM_ARRAY_SIZE_BYTES);
    b = aligned_alloc(4096, STREAM_ARRAY_SIZE_BYTES);
    c = aligned_alloc(4096, STREAM_ARRAY_SIZE_BYTES);
    if (!a || !b || !c) {
        fprintf(stderr, "Failed to allocate memory.");
        return 1;
    }

    /* For KSM purposes we don't want identical pages. */
    a[0] = 0.1;
    b[0] = 0.2;
    c[0] = 0.0;

    /* Get initial value for system clock. */
    for (j = 1; j < STREAM_ARRAY_SIZE; j++) {
        a[j] = a[j - 1] + 0.0001;
        b[j] = b[j - 1] + 0.0002;
        c[j] = c[j - 1] + 0.0003;
    }

    if (mode == MODE_KSM || mode == MODE_MPROTECT_KSM || mode == MODE_FORK_KSM) {
        int ret;

        ret = madvise(a, STREAM_ARRAY_SIZE_BYTES, MADV_MERGEABLE);
        ret |= madvise(b, STREAM_ARRAY_SIZE_BYTES, MADV_MERGEABLE);
        ret |= madvise(c, STREAM_ARRAY_SIZE_BYTES, MADV_MERGEABLE);
        if (ret) {
            fprintf(stderr, "madvise(MADV_MERGEABLE) failed.");
            exit(1);
        }
    }

    /* --- MAIN LOOP --- */

    printf("KiB/s,Copies/s,Copies,Seconds\n");

    scalar = 1.0001;

    for (k = 0; k < iterations + WARMUP_ITERATIONS; k++) {
        switch (mode) {
        case MODE_DEFAULT:
        default:
            break;
        case MODE_FORK:
            do_fork();
            break;
        case MODE_KSM:
            do_trigger_ksm();
            break;
        case MODE_SWAP:
            do_swap(a, b, c, STREAM_ARRAY_SIZE_BYTES);
            break;
        case MODE_MPROTECT:
            do_mprotect(a, b, c, STREAM_ARRAY_SIZE_BYTES);
            break;
        case MODE_SWAP_FORK:
            do_swap(a, b, c, STREAM_ARRAY_SIZE_BYTES);
            do_fork();
            break;
        case MODE_MPROTECT_KSM:
            do_mprotect(a, b, c, STREAM_ARRAY_SIZE_BYTES);
            do_trigger_ksm();
            break;
        case MODE_FORK_KSM:
            do_fork();
            do_trigger_ksm();
            break;
        };

        pgcopy_old = vmstat_pgcopy();
        istart = mysecond();
        for (j = 0; j < STREAM_ARRAY_SIZE; j++)
            c[j] = a[j];

        for (j = 0; j < STREAM_ARRAY_SIZE; j++)
            b[j] = scalar * c[j];

        for (j = 0; j < STREAM_ARRAY_SIZE; j++)
            c[j] = a[j] + b[j];

        for (j = 0; j < STREAM_ARRAY_SIZE; j++)
            a[j] = b[j] + scalar * c[j];
        istop = mysecond();
        pgcopy_new = vmstat_pgcopy();

	if (k < WARMUP_ITERATIONS)
		continue;

        printf("%llu,%llu,%llu,%lf\n", (unsigned long long) (((bytes[0] + bytes[1] + bytes[2] + bytes[3]) / 1024) /
                (istop - istart)), (unsigned long long) ((pgcopy_new - pgcopy_old) / ((istop - istart))), (unsigned long long) (pgcopy_new - pgcopy_old), istop - istart);
    }

    return 0;

    /* --- Check Results --- */
    checkSTREAMresults(iterations + WARMUP_ITERATIONS);

    return 0;
}

static double mysecond(void)
{
    struct timezone tzp;
    struct timeval tp;
    int ret;

    ret = gettimeofday(&tp,&tzp);
    assert(!ret);
    return (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6;
}

#ifndef abs
#define abs(a) ((a) >= 0 ? (a) : -(a))
#endif
static void checkSTREAMresults(unsigned long iterations) {
    double aj,bj,cj,scalar;
    double aSumErr,bSumErr,cSumErr;
    double aAvgErr,bAvgErr,cAvgErr;
    unsigned long k;
    double epsilon;
    ssize_t j;
    int ierr, err;

    /* reproduce initialization */
    aj = 2.0;
    bj = 2.0;
    cj = 0.0;
    /* now execute timing loop */
    scalar = 3.0;
    for (k = 0; k < iterations; k++) {
        cj = aj;
        bj = scalar * cj;
        cj = aj + bj;
        aj = bj + scalar * cj;
    }

    /* accumulate deltas between observed and expected results */
    aSumErr = 0.0;
    bSumErr = 0.0;
    cSumErr = 0.0;
    for (j = 0; j < STREAM_ARRAY_SIZE; j++) {
        aSumErr += abs(a[j] - aj);
        bSumErr += abs(b[j] - bj);
        cSumErr += abs(c[j] - cj);
    }
    aAvgErr = aSumErr / (double) STREAM_ARRAY_SIZE;
    bAvgErr = bSumErr / (double) STREAM_ARRAY_SIZE;
    cAvgErr = cSumErr / (double) STREAM_ARRAY_SIZE;

    assert(sizeof(double) == 8);
    epsilon = 1.e-13;

    err = 0;
    if (abs(aAvgErr / aj) > epsilon) {
        err++;
        printf ("Failed Validation on array a[], AvgRelAbsErr > epsilon (%e)\n",
                epsilon);
        printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",
                aj, aAvgErr, abs(aAvgErr) / aj);
        ierr = 0;
        for (j = 0; j < STREAM_ARRAY_SIZE; j++) {
            if (abs(a[j] / aj - 1.0) > epsilon) {
                ierr++;
            }
        }
        printf("     For array a[], %d errors were found.\n",ierr);
    }
    if (abs(bAvgErr / bj) > epsilon) {
        err++;
        printf ("Failed Validation on array b[], AvgRelAbsErr > epsilon (%e)\n",
                epsilon);
        printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",
                bj, bAvgErr, abs(bAvgErr) / bj);
        printf ("     AvgRelAbsErr > Epsilon (%e)\n", epsilon);
        ierr = 0;
        for (j= 0 ; j < STREAM_ARRAY_SIZE; j++) {
            if (abs(b[j] / bj - 1.0) > epsilon) {
                ierr++;
            }
        }
        printf("     For array b[], %d errors were found.\n",ierr);
    }
    if (abs(cAvgErr / cj) > epsilon) {
        err++;
        printf ("Failed Validation on array c[], AvgRelAbsErr > epsilon (%e)\n",
                epsilon);
        printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",
                cj, cAvgErr, abs(cAvgErr) / cj);
        printf ("     AvgRelAbsErr > Epsilon (%e)\n", epsilon);
        ierr = 0;
        for (j= 0; j < STREAM_ARRAY_SIZE; j++) {
            if (abs(c[j] / cj - 1.0) > epsilon) {
                ierr++;
            }
        }
        printf("     For array c[], %d errors were found.\n", ierr);
    }
    if (!err) {
        printf ("Solution Validates: avg error less than %e on all three arrays\n",
                epsilon);
    }
}
