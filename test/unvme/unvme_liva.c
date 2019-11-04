/**
 * Copyright (c) 2015-2016, Micron Technology, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the copyright holder nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief UNVMe API test.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <err.h>
#include <fcntl.h>
//#include <veaio.h>

#include "unvme.h"

/// Print if verbose flag is set
#define VERBOSE(fmt, arg...) \
    if (verbose)             \
    printf(fmt, ##arg)

/**
 * Read tsc.
 * @return  tsc value.
 */
static inline uint64_t rdtsc(void)
{
    uint64_t ret;
    void *vehva = ((void *)0x000000001000);
    asm volatile("lhm.l %0,0(%1)"
                 : "=r"(ret)
                 : "r"(vehva));
    // the "800" is due to the base frequency of Tsubasa
    return ((uint64_t)1000 * ret) / 800;
}

/**
 * Get tsc per second using sleeping for 1/100th of a second.
 */
static inline uint64_t rdtsc_second()
{
    static uint64_t tsc_ps = 0;
    if (!tsc_ps)
    {
        uint64_t t0 = rdtsc();
        usleep(1000);
        uint64_t t1 = rdtsc();
        usleep(1000);
        uint64_t t2 = rdtsc();
        t2 -= t1;
        t1 -= t0;
        if (t2 > t1)
            t2 = t1;
        tsc_ps = t2 * 1000;
    }
    return tsc_ps;
}

void vepci(char *pciname, int ratio, int verbose);
void posix(char *pciname, int ratio, int verbose);

void vepci_test(char *pciname, int ratio, int verbose);

/**
 * Main.
 */
int main(int argc, char **argv)
{
    const char *usage = "Usage: %s [OPTION]... PCINAME\n\
           -p         posix\n\
           -v         verbose\n\
           -r RATIO   max blocks per I/O ratio (default 4)\n\
           PCINAME    PCI device name (as 01:00.0[/1] format)";

    int opt, ratio = 4, verbose = 0, posix_flag = 0;
    const char *prog = strrchr(argv[0], '/');
    prog = prog ? prog + 1 : argv[0];

    while ((opt = getopt(argc, argv, "r:va")) != -1)
    {
        switch (opt)
        {
        case 'r':
            ratio = strtol(optarg, 0, 0);
            if (ratio <= 0)
                errx(1, "r must be > 0");
            break;
        case 'v':
            verbose = 1;
            break;
        case 'p':
            posix_flag = 1;
            break;
        default:
            warnx(usage, prog);
            exit(1);
        }
    }
    if ((optind + 1) != argc)
    {
        warnx(usage, prog);
        exit(1);
    }
    char *pciname = argv[optind];

    if (posix_flag == 0)
    {
        vepci_test(pciname, ratio, verbose);
        //        vepci(pciname, ratio, verbose);
    }
    else
    {
        posix(pciname, ratio, verbose);
    }

    return 0;
}

#define LOOPNUM 100
#define NLB 2

void vepci_test(char *pciname, int ratio, int verbose)
{
    printf("API TEST BEGIN\n");
    const unvme_ns_t *ns = unvme_open(pciname);
    if (!ns)
        exit(1);

    // set large number of I/O and size
    int maxnlb = ratio * ns->maxbpio;
    int iocount = ratio * (ns->qsize - 1);

    printf("%s qc=%d/%d qs=%d/%d bc=%#lx bs=%d maxnlb=%d/%d\n",
           ns->device, ns->qcount, ns->maxqcount, ns->qsize, ns->maxqsize,
           ns->blockcount, ns->blocksize, maxnlb, ns->maxbpio);

    int q, i;
    int j;
    u64 slba, size, w, *p;
    unvme_iod_t *iod = malloc(iocount * sizeof(unvme_iod_t));
    void **buf = malloc(iocount * sizeof(void *));

    q = 0;
    printf("> Test q=%d ioc=%d\n", q, iocount);
    u32 nlb = 80000;

    printf("Test alloc\n");
    for (i = 0; i < iocount; i++)
    {
        size = nlb * ns->blocksize;
        VERBOSE("  alloc.%-2d  %#8x %#lx\n", i, nlb, size);
        if (!(buf[i] = unvme_alloc(ns, size)))
            errx(1, "alloc.%d failed", i);
    }

    printf("Test awrite\n");
    uint64_t t1 = rdtsc();
    slba = 0;
    for (i = 0; i < iocount; i++)
    {
        size = nlb * ns->blocksize / sizeof(u64);
        p = buf[i];
        for (w = 0; w < size; w++)
            p[w] = (w << 32) + i;
        VERBOSE("  awrite.%-2d %#8x %p %#lx\n", i, nlb, p, slba);
        if (!(iod[i] = unvme_awrite(ns, q, p, slba, nlb)))
            errx(1, "awrite.%d failed", i);
        slba += nlb;
    }

    for (i = iocount - 1; i >= 0; i--)
    {
        VERBOSE("  apoll.awrite.%-2d\n", i);
        if (unvme_apoll(iod[i], UNVME_TIMEOUT))
            errx(1, "apoll_awrite.%d failed", i);
    }
    uint64_t t2 = rdtsc();

    printf("Test verify\n");
    slba = 0;
    for (i = 0; i < iocount; i++)
    {
        size = nlb * ns->blocksize / sizeof(u64);
        p = buf[i];
        VERBOSE("  verify.%-2d %#8x %p %#lx\n", i, nlb, p, slba);
        for (w = 0; w < size; w++)
        {
            if (p[w] != ((w << 32) + i))
            {
                w *= sizeof(w);
                slba += w / ns->blocksize;
                w %= ns->blocksize;
                errx(1, "miscompare at lba %#lx offset %#lx", slba, p);
            }
        }
        slba += NLB;
    }

    printf("Test free\n");
    for (i = 0; i < iocount; i++)
    {
        VERBOSE("  free.%-2d\n", i);
        if (unvme_free(ns, buf[i]))
            errx(1, "free.%d failed", i);
    }

    free(buf);
    free(iod);
    unvme_close(ns);

    printf("API TEST COMPLETE (%ld clock)\n", t2 - t1);
}

void vepci(char *pciname, int ratio, int verbose)
{
    printf("API TEST BEGIN\n");
    const unvme_ns_t *ns = unvme_open(pciname);
    if (!ns)
        exit(1);

    // set large number of I/O and size
    int maxnlb = ratio * ns->maxbpio;
    int iocount = ratio * (ns->qsize - 1);

    printf("%s qc=%d/%d qs=%d/%d bc=%#lx bs=%d maxnlb=%d/%d\n",
           ns->device, ns->qcount, ns->maxqcount, ns->qsize, ns->maxqsize,
           ns->blockcount, ns->blocksize, maxnlb, ns->maxbpio);

    int q, i;
    int j;
    u64 slba, size, w, *p;
    unvme_iod_t *iod = malloc(iocount * sizeof(unvme_iod_t));
    void **buf = malloc(iocount * sizeof(void *));

    q = 0;
    printf("> Test q=%d ioc=%d\n", q, iocount);

    printf("Test alloc\n");
    for (i = 0; i < iocount; i++)
    {
        size = NLB * ns->blocksize;
        VERBOSE("  alloc.%-2d  %#8x %#lx\n", i, NLB, size);
        if (!(buf[i] = unvme_alloc(ns, size)))
            errx(1, "alloc.%d failed", i);
    }

    printf("Test awrite\n");
    uint64_t t1 = rdtsc();
    for (j = 0; j < LOOPNUM; j++)
    {
        slba = 0;
        for (i = 0; i < iocount; i++)
        {
            size = NLB * ns->blocksize / sizeof(u64);
            p = buf[i];
            for (w = 0; w < size; w++)
                p[w] = (w << 32) + i;
            VERBOSE("  awrite.%-2d %#8x %p %#lx\n", i, NLB, p, slba);
            if (!(iod[i] = unvme_awrite(ns, q, p, slba, NLB)))
                errx(1, "awrite.%d failed", i);
            slba += NLB;
        }

        for (i = iocount - 1; i >= 0; i--)
        {
            VERBOSE("  apoll.awrite.%-2d\n", i);
            if (unvme_apoll(iod[i], UNVME_TIMEOUT))
                errx(1, "apoll_awrite.%d failed", i);
        }
    }
    uint64_t t2 = rdtsc();

    printf("Test aread\n");
    uint64_t t3 = rdtsc();
    for (j = 0; j < 100; j++)
    {
        slba = 0;
        for (i = 0; i < iocount; i++)
        {
            size = NLB * ns->blocksize;
            p = buf[i];
            bzero(p, size);
            VERBOSE("  aread.%-2d  %#8x %p %#lx\n", i, NLB, p, slba);
            if (!(iod[i] = unvme_aread(ns, q, p, slba, NLB)))
                errx(1, "aread.%d failed", i);
            slba += NLB;
        }

        for (i = iocount - 1; i >= 0; i--)
        {
            VERBOSE("  apoll.aread.%-2d\n", i);
            if (unvme_apoll(iod[i], UNVME_TIMEOUT))
                errx(1, "apoll_aread.%d failed", i);
        }
    }
    uint64_t t4 = rdtsc();

    printf("Test verify\n");
    slba = 0;
    for (i = 0; i < iocount; i++)
    {
        size = NLB * ns->blocksize / sizeof(u64);
        p = buf[i];
        VERBOSE("  verify.%-2d %#8x %p %#lx\n", i, NLB, p, slba);
        for (w = 0; w < size; w++)
        {
            if (p[w] != ((w << 32) + i))
            {
                w *= sizeof(w);
                slba += w / ns->blocksize;
                w %= ns->blocksize;
                errx(1, "miscompare at lba %#lx offset %#lx", slba, p);
            }
        }
        slba += NLB;
    }

    printf("Test free\n");
    for (i = 0; i < iocount; i++)
    {
        VERBOSE("  free.%-2d\n", i);
        if (unvme_free(ns, buf[i]))
            errx(1, "free.%d failed", i);
    }

    free(buf);
    free(iod);
    unvme_close(ns);

    printf("API TEST COMPLETE (%ld clock)\n", t4 - t3 + t2 - t1);
}

#if 0
void aio(char *pciname, int ratio, int verbose)
{
    printf("API TEST BEGIN\n");
    int fd = open(pciname, O_RDWR | O_CREAT, S_IRWXU);
    if (fd == -1)
    {
        perror("open");
        exit(1);
    }
    int qsize = 256;
    int blocksize = 512;

    // set large number of I/O and size
    int iocount = ratio * (qsize - 1);

    printf("%s qc=%d/%d qs=%d/%d bc=%#lx bs=%d maxnlb=%d/%d\n",
           pciname, 32, 32, 256, 0,
           0, blocksize, 0, 0);

    int q, i;
    int j;
    ssize_t retval;
    int errnoval;
    u64 slba, size, w, *p;
    struct ve_aio_ctx **ctx = malloc(iocount * sizeof(struct ve_aio_ctx *));
    void **buf = malloc(iocount * sizeof(void *));

    q = 0;
    printf("> Test q=%d ioc=%d\n", q, iocount);

    printf("Test alloc\n");
    for (i = 0; i < iocount; i++)
    {
        size = NLB * blocksize;
        VERBOSE("  alloc.%-2d  %#8x %#lx\n", i, NLB, size);

        if (!(buf[i] = malloc(size)))
            errx(1, "alloc.%d failed", i);

        if (!(ctx[i] = ve_aio_init()))
            errx(1, "alloc.%d failed", i);
    }

    printf("Test awrite\n");
    uint64_t t1 = rdtsc();
    for (j = 0; j < LOOPNUM; j++)
    {
        slba = 0;
        for (i = 0; i < iocount; i++)
        {
            size = NLB * blocksize / sizeof(u64);
            p = buf[i];
            for (w = 0; w < size; w++)
                p[w] = (w << 32) + i;
            VERBOSE("  awrite.%-2d %#8x %p %#lx\n", i, NLB, p, slba);
            if (ve_aio_write(ctx[i], fd, NLB * blocksize, buf[i], slba * blocksize))
                errx(1, "awrite.%d failed", i);
            slba += NLB;
        }

        for (i = iocount - 1; i >= 0; i--)
        {
            VERBOSE("  apoll.awrite.%-2d\n", i);
            if (ve_aio_wait(ctx[i], &retval, &errnoval))
                errx(1, "apoll_awrite.%d failed", i);
        }
    }
    uint64_t t2 = rdtsc();

    printf("Test aread\n");
    uint64_t t3 = rdtsc();
    for (j = 0; j < 100; j++)
    {
        slba = 0;
        for (i = 0; i < iocount; i++)
        {
            size = NLB * blocksize;
            p = buf[i];
            bzero(p, size);
            VERBOSE("  aread.%-2d  %#8x %p %#lx\n", i, NLB, p, slba);
            if (ve_aio_read(ctx[i], fd, NLB * blocksize, buf[i], slba * blocksize))
                errx(1, "aread.%d failed", i);
            slba += NLB;
        }

        for (i = iocount - 1; i >= 0; i--)
        {
            VERBOSE("  apoll.aread.%-2d\n", i);
            if (ve_aio_wait(ctx[i], &retval, &errnoval))
                errx(1, "apoll_aread.%d failed", i);
        }
    }
    uint64_t t4 = rdtsc();

    printf("Test verify\n");
    slba = 0;
    for (i = 0; i < iocount; i++)
    {
        size = NLB * blocksize / sizeof(u64);
        p = buf[i];
        VERBOSE("  verify.%-2d %#8x %p %#lx\n", i, NLB, p, slba);
        for (w = 0; w < size; w++)
        {
            if (p[w] != ((w << 32) + i))
            {
                w *= sizeof(w);
                slba += w / blocksize;
                w %= blocksize;
                errx(1, "miscompare at lba %#lx offset %#lx", slba, p);
            }
        }
        slba += NLB;
    }

    printf("Test free\n");
    for (i = 0; i < iocount; i++)
    {
        VERBOSE("  free.%-2d\n", i);
        free(buf[i]);

        if (ve_aio_fini(ctx[i]))
            errx(1, "free.%d failed", i);
    }

    free(buf);
    free(ctx);

    printf("API TEST COMPLETE (%ld clock)\n", t4 - t3 + t2 - t1);
}
#endif

void posix(char *pciname, int ratio, int verbose)
{
    printf("API TEST BEGIN\n");
    int fd = open(pciname, O_RDWR | O_CREAT, S_IRWXU);
    if (fd == -1)
    {
        perror("open");
        exit(1);
    }
    int qsize = 256;
    int blocksize = 512;

    // set large number of I/O and size
    int iocount = ratio * (qsize - 1);

    printf("%s qc=%d/%d qs=%d/%d bc=%#lx bs=%d maxnlb=%d/%d\n",
           pciname, 32, 32, 256, 0,
           0, blocksize, 0, 0);

    int q, i;
    int j;
    ssize_t retval;
    int errnoval;
    u64 slba, size, w, *p;
    struct ve_aio_ctx **ctx = malloc(iocount * sizeof(struct ve_aio_ctx *));
    void **buf = malloc(iocount * sizeof(void *));

    q = 0;
    printf("> Test q=%d ioc=%d\n", q, iocount);

    printf("Test alloc\n");
    for (i = 0; i < iocount; i++)
    {
        size = NLB * blocksize;
        VERBOSE("  alloc.%-2d  %#8x %#lx\n", i, NLB, size);

        if (!(buf[i] = malloc(size)))
            errx(1, "alloc.%d failed", i);

        if (!(ctx[i] = ve_aio_init()))
            errx(1, "alloc.%d failed", i);
    }

    printf("Test awrite\n");
    uint64_t t1 = rdtsc();
    for (j = 0; j < LOOPNUM; j++)
    {
        slba = 0;
        for (i = 0; i < iocount; i++)
        {
            size = NLB * blocksize / sizeof(u64);
            p = buf[i];
            for (w = 0; w < size; w++)
                p[w] = (w << 32) + i;
            VERBOSE("  awrite.%-2d %#8x %p %#lx\n", i, NLB, p, slba);
            if (ve_aio_write(ctx[i], fd, NLB * blocksize, buf[i], slba * blocksize))
                errx(1, "awrite.%d failed", i);
            slba += NLB;
        }

        for (i = iocount - 1; i >= 0; i--)
        {
            VERBOSE("  apoll.awrite.%-2d\n", i);
            if (ve_aio_wait(ctx[i], &retval, &errnoval))
                errx(1, "apoll_awrite.%d failed", i);
        }
    }
    uint64_t t2 = rdtsc();

    printf("Test aread\n");
    uint64_t t3 = rdtsc();
    for (j = 0; j < 100; j++)
    {
        slba = 0;
        for (i = 0; i < iocount; i++)
        {
            size = NLB * blocksize;
            p = buf[i];
            bzero(p, size);
            VERBOSE("  aread.%-2d  %#8x %p %#lx\n", i, NLB, p, slba);
            if (ve_aio_read(ctx[i], fd, NLB * blocksize, buf[i], slba * blocksize))
                errx(1, "aread.%d failed", i);
            slba += NLB;
        }

        for (i = iocount - 1; i >= 0; i--)
        {
            VERBOSE("  apoll.aread.%-2d\n", i);
            if (ve_aio_wait(ctx[i], &retval, &errnoval))
                errx(1, "apoll_aread.%d failed", i);
        }
    }
    uint64_t t4 = rdtsc();

    printf("Test verify\n");
    slba = 0;
    for (i = 0; i < iocount; i++)
    {
        size = NLB * blocksize / sizeof(u64);
        p = buf[i];
        VERBOSE("  verify.%-2d %#8x %p %#lx\n", i, NLB, p, slba);
        for (w = 0; w < size; w++)
        {
            if (p[w] != ((w << 32) + i))
            {
                w *= sizeof(w);
                slba += w / blocksize;
                w %= blocksize;
                errx(1, "miscompare at lba %#lx offset %#lx", slba, p);
            }
        }
        slba += NLB;
    }

    printf("Test free\n");
    for (i = 0; i < iocount; i++)
    {
        VERBOSE("  free.%-2d\n", i);
        free(buf[i]);

        if (ve_aio_fini(ctx[i]))
            errx(1, "free.%d failed", i);
    }

    free(buf);
    free(ctx);

    printf("API TEST COMPLETE (%ld clock)\n", t4 - t3 + t2 - t1);
}