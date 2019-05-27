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
 * @brief UNVMe write-read a specific address.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <err.h>
#include <time.h>
#include <getopt.h>

#include "unvme.h"


int main(int argc, char** argv)
{
    const char* usage = "Usage: %s -w|-r LBA [OPTIONS]... PCINAME\n\
           -w|-r    LBA        write|read at LBA\n\
           -n       BLOCKCOUNT number of blocks (default 8)\n\
           -p       PATTERN    64-bit data pattern to write (default random)\n\
           --qsize  QSIZE      set queue size\n\
           --qcount QCOUNT     set queue count\n\
           -h                  hide outputs of data\n\
           PCINAME             PCI device name (as 02:00.0[/1] format)\n\n\
           (either -w or -r must be specified)\n";

    struct timespec start, end; 

    const char* prog = strrchr(argv[0], '/');
    prog = prog ? prog + 1 : argv[0];
    int opt;
    int rw = 0;
    u64 lba = 0L;
    u32 nlb = 8;
    u64 pat = 0L;
    int rnd = 1;
    int qsize = 0;
    int qcount = 0;
    int hidden = 0;

    int flag = 0;
    const struct option longopts[] = {
      //{    *name,           has_arg, *flag, val },
      //  {  "create",       no_argument,     0, 'c' },
      //  {  "delete", required_argument,     0, 'd' },
      //  { "setflag",       no_argument, &flag,  1  },
      //  { "clrflag",       no_argument, &flag,  0  },
        {    "qsize", required_argument, &flag,  1  },
        {   "qcount", required_argument, &flag,  2  },
        {         0,                 0,      0,  0  }, // termination
    };

    while ((opt = getopt_long(argc, argv, "w:r:n:p:h", longopts, NULL)) != -1) {
        switch (opt) {
        case 0:
            switch(flag) {
                case 1:
                    qsize = strtoul(optarg, 0, 0);;
                    break;
                case 2:
                    qcount = strtoul(optarg, 0, 0);;
                    break;
                default:
                    warnx(usage, prog);
                    exit(1);
            }
            break;
        case 'w':
        case 'r':
            rw = opt;
            lba = strtoull(optarg, 0, 0);
            break;
        case 'n':
            nlb = strtoul(optarg, 0, 0);
            break;
        case 'p':
            pat = strtoull(optarg, 0, 0);
            rnd = 0;
            break;
        case 'h':
            hidden = 1;
            break;
        default:
            warnx(usage, prog);
            exit(1);
        }
    }

    if ((rw == 0) || (optind + 1) != argc) {
        warnx(usage, prog);
        exit(1);
    }
    char* pciname = argv[optind];

    const unvme_ns_t* ns = unvme_openq(pciname, qcount, qsize);
    if (!ns) exit(1);
    printf("%s qc=%d/%d qs=%d/%d bc=%#lx bs=%d\n",
            ns->device, ns->qcount, ns->maxqcount, ns->qsize, ns->maxqsize,
            ns->blockcount, ns->blocksize);

    int stat;
    u64 i, len = nlb;
    len *= ns->blocksize;
    void* buf = unvme_alloc(ns, len);
    if (!buf) errx(1, "unvme_alloc %lu failed", len);

    if (rw == 'w') {
        if (rnd) {
            srandom(time(NULL));
            u32* p = buf;
            for (i = 0; i < len; i += sizeof(u32)) *p++ = random();
            printf("Write random pattern at lba %#lx nlb %u\n", lba, nlb);
        } else {
            u64* p = buf;
            printf("Write pattern %#016lx at lba %#lx nlb %u\n", pat, lba, nlb);
            for (i = 0; i < len; i += sizeof(u64)) *p++ = pat++;
        }

        gettimeofday(&start, NULL);
        int stat = unvme_write(ns, 0, buf, lba, nlb);
        gettimeofday(&end, NULL);
        if (stat) errx(1, "unvme_write failed: lba=%#lx nlb=%#x stat=%#x", lba, nlb, stat);
        double time = end.tv_sec - start.tv_sec;
        time = (time * 1000 * 1000 * 1000 + (end.tv_nsec - start.tv_nsec)) / (1000 * 1000 * 1000);
        double performance = len / time;
        printf("elapsed time: %lfs\n", time);
        printf("RESULT: qsize=%d\n", qsize);
        printf("RESULT: write: %lfMB/s\n", performance / 1024 / 1024);
    } else {
        printf("Read lba %#lx nlb %u\n", lba, nlb);
        gettimeofday(&start, NULL);
        stat = unvme_read(ns, 0, buf, lba, nlb);
        gettimeofday(&end, NULL);
        double time = end.tv_sec - start.tv_sec;
        time = (time * 1000 * 1000 * 1000 + (end.tv_nsec - start.tv_nsec)) / (1000 * 1000 * 1000);
        double performance = len / time;
        printf("elapsed time: %lfs\n", time);
        printf("RESULT: qsize=%d\n", qsize);
        printf("RESULT: read: %lfMB/s\n", performance / 1024 / 1024);
        if (stat) errx(1, "unvme_read failed: lba=%#lx nlb=%#x stat=%#x", lba, nlb, stat);

        if (!hidden) {
            int skip = 0;
            u64* p = buf;
            u64 w0 = ~*p, w1 = 0, w2 = 0, w3 = 0;
            for (i = 0; i < len; i += 32) {
                if (p[0] != w0 || p[1] != w1 || p[2] != w2 || p[3] != w3) {
                    printf("%08lx: %016lx %016lx %016lx %016lx\n", i, p[0], p[1], p[2], p[3]);
                    skip = 0;
                } else if (!skip) {
                    printf("*\n");
                    skip = 1;
                }
                w0 = p[0];
                w1 = p[1];
                w2 = p[2];
                w3 = p[3];
                p += 4;
            }
        }
    }

    unvme_free(ns, buf);
    unvme_close(ns);
    return 0;
}

