/**
 * Copyright 2020 NEC Laboratories Europe GmbH
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 *    3. Neither the name of NEC Laboratories Europe GmbH nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY NEC Laboratories Europe GmbH AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NEC Laboratories 
 * Europe GmbH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO,  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdatomic.h>
#include "aurora_pci.h"
#include "unvme_lock.h"

#define SIZE_64M (1UL << 26)
#define PCIATB_PAGESIZE (1UL << 26)
#define MAX_SIZE (PCIATB_PAGESIZE * 16/*512*/)

static uint64_t vehva = 0;
static atomic_uint lock = ATOMIC_VAR_INIT(0);

static int existFile(const char *path)
{
        struct stat st;

        if (stat(path, &st) != 0)
        {
                return 0;
        }

        return (st.st_mode & S_IFMT) == S_IFCHR;
}

uint64_t aurora_map(uint64_t address)
{
        if (!existFile("/dev/uio0"))
        {
                fprintf(stderr, "error: /dev/uio0 doesn't exist!\n");
                exit(1);
        }

        // map device register memory space to ve host address space
        uint64_t pci_vhsaa;
        size_t size = SIZE_64M;
        /* get PCI ATB entry address from another VE node */
        pci_vhsaa = (address / SIZE_64M) * SIZE_64M;

        /* map PCI ATB entry to VEHVA */
        vehva = ve_register_pci_to_vehva(pci_vhsaa, size);
        if (vehva == (uint64_t)-1)
        {
                perror("Fail to ve_register_pci_to_vehva()");
                return (uint64_t)-1;
        }
        return vehva + (address % SIZE_64M);
}

static uint64_t pci_vhsaa;
static void *vemva;

static void vhsa_release() {
  FILE *fp;
  uint64_t pci_vhsaa, vhsa_size;
  fp = fopen("/home/sawamoto/.vhsaa_registration_log", "r");
  
  while(fscanf(fp, "%lx,%lx", &pci_vhsaa, &vhsa_size) != EOF ) {
    int ret = ve_unregister_mem_from_pci(pci_vhsaa, vhsa_size);
    if (ret) {
      perror("Fail to ve_unregister_mem_from_pci()");
    } else {
      printf("Successfully unregistered from %lx (size: %lx).\n", pci_vhsaa, vhsa_size);
    }
  }
  fclose(fp);
  truncate("/home/sawamoto/.vhsaa_registration_log", 0);
}

int aurora_release()
{
        int ret;
        if (vehva != 0)
        {
                ret = ve_unregister_pci_from_vehva(vehva, SIZE_64M);
                if (ret)
                {
                        perror("Fail to ve_unregister_pci_to_vehva");
                        return 0;
                }
        }
        munmap(vemva, MAX_SIZE);
        vhsa_release();
        return 1;
}

void buddy_init(void *addr, size_t size);
int aurora_init()
{
  uint64_t vhsa_size = MAX_SIZE;
  
        vhsa_release();
        
        // these VE memory should be used for data buffer
        vemva = mmap(NULL, vhsa_size, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_SHARED | MAP_64MB, -1, 0);
        if (NULL == vemva)
        {
                perror("Fail to allocate memory");
                return 0;
        }

        pci_vhsaa = ve_register_mem_to_pci(vemva, vhsa_size);
        if (pci_vhsaa == (uint64_t)-1)
        {
          perror("Fail to ve_register_mem_to_pci()");
                return 0;
        } else {
          printf("registered at %lx (size: %lx).\n", pci_vhsaa, vhsa_size);
          FILE *fp;
          fp = fopen("/home/sawamoto/.vhsaa_registration_log", "a");
          fprintf(fp, "%lx,%lx\n", pci_vhsaa, vhsa_size);
          fclose(fp);
        }

        buddy_init(vemva, vhsa_size);
        return 1;
}

//#define MEM_SIMPLE
#ifdef MEM_SIMPLE

void buddy_init(void *addr, size_t size)
{
}

static size_t alloc_offset = 0;
vfio_dma_t *aurora_mem_alloc(size_t size)
{
        size_t mask = 4096 /* x86 pagesize */ - 1;
        size = (size + mask) & ~mask;

        while (atomic_fetch_or(&lock, 1) != 0)
        {
                asm volatile("" ::
                                 : "memory");
        }

        if (alloc_offset + size < MAX_SIZE)
        {
                vfio_dma_t *buf = malloc(sizeof(vfio_dma_t));
                buf->buf = vemva + alloc_offset;
                buf->addr = pci_vhsaa + alloc_offset;
                buf->size = size;
                buf->next = NULL;
                alloc_offset += size;

                lock = ATOMIC_VAR_INIT(0);
                return buf;
        }

        fprintf(stderr, "UNVME_ERROR: memory exhausted\n");
        fflush(stderr);
        abort();
        return NULL;
}

void aurora_mem_free(vfio_dma_t *dma_ctx)
{
        free(dma_ctx);
}

#else
static size_t maxbytes = 2 * 1024 * 1024;
static const int kFreeListIndexMax = 16;
static void **freelist_list[kFreeListIndexMax];
static void *global_freelist;
static int entry_max;
static void *start_addr;
_Thread_local static int freelist_index = -1;
static atomic_int freelist_current_index = ATOMIC_VAR_INIT(0);

void buddy_dump()
{
#if 0
        entry_max = 0;
        for (int i = 512; i <= maxbytes; i *= 2)
        {
                entry_max++;
        }
        size_t size = 512;
        for (int i = 0; i < entry_max; i++)
        {
                void **fle = freelist[i];
                printf("%lu: ", size);
                int j = 0;
                while (fle)
                {
                        printf("%p ", fle);
                        fle = *fle;
                        j++;
                        if (j == 10)
                        {
                                printf("...");
                                break;
                        }
                }
                printf("\n");
                size *= 2;
        }
#endif
}

static inline int get_index_from_size(size_t size)
{
        int r = 0;
        for (int i = 512; i <= maxbytes; i *= 2)
        {
                if (i == size)
                {
                        return r;
                }
                r++;
        }
        return -1;
}

static inline int is_left(void *p, size_t size)
{
        return ((size_t)(p - start_addr) % (size * 2)) == 0;
}

void buddy_init(void *addr, size_t size)
{
        assert(size % maxbytes == 0);
        entry_max = 0;
        start_addr = addr;
        for (int i = 512; i <= maxbytes; i *= 2)
        {
                entry_max++;
        }
        for (int i = 0; i < atomic_load(&freelist_current_index); i++)
        {
                for (int j = 0; j < entry_max; j++)
                {
                        freelist_list[i][j] = NULL;
                }
        }
        for (int i = atomic_load(&freelist_current_index); i < kFreeListIndexMax; i++)
        {
                freelist_list[i] = NULL;
        }
        void **fle = &global_freelist;
        for (size_t offset = 0; offset < size; offset += maxbytes)
        {
                *fle = addr + offset;
                fle = addr + offset;
        }
        *fle = NULL;
}

void **get_freelist()
{
        if (freelist_index == -1)
        {
                int index = atomic_fetch_add(&freelist_current_index, 1);
                if (index >= kFreeListIndexMax)
                {
                        abort();
                }
                freelist_index = index;

                freelist_list[freelist_index] = malloc(sizeof(void *) * entry_max);
                for (int i = 0; i < entry_max; i++)
                {
                        freelist_list[freelist_index][i] = NULL;
                }
        }
        return freelist_list[freelist_index];
}

void *buddy_malloc(size_t size)
{
        int i = get_index_from_size(size);
        if (i == -1)
        {
                return NULL;
        }
        void **freelist = get_freelist();
        void *rval = freelist[i];
        if (rval != NULL)
        {
                freelist[i] = *((void **)rval);
        }
        else
        {
                if (size == maxbytes)
                {
                        while (atomic_fetch_or(&lock, 1) != 0)
                        {
                                asm volatile("" ::
                                                 : "memory");
                        }
                        void *fle = global_freelist;
                        if (fle == NULL)
                        {
                                return NULL;
                        }
                        rval = fle;
                        fle = *((void **)fle);
                        freelist[i] = fle;
                        for (int i = 0; i < 64; i++)
                        {
                                if (fle == NULL)
                                {
                                        break;
                                }
                                fle = *((void **)fle);
                        }
                        if (fle == NULL)
                        {
                                global_freelist = NULL;
                        }
                        else
                        {
                                global_freelist = *((void **)fle);
                                *((void **)fle) = NULL;
                        }
                        lock = ATOMIC_VAR_INIT(0);
                }
                else
                {
                        rval = buddy_malloc(size * 2);
                        if (rval != NULL)
                        {
                                freelist[i] = rval + size;
                                *((void **)(rval + size)) = NULL;
                        }
                }
        }
        return rval;
}

void buddy_free(void *buf, size_t size)
{
        int i = get_index_from_size(size);
        if (i == -1)
        {
                return;
        }
        int do_compaction = (size != maxbytes) ? 1 : 0;
        void **freelist = get_freelist();
        void **fle = &freelist[i];
        while (1)
        {
                if (*fle == NULL || buf < *fle)
                {
                        if (do_compaction && buf + size == *fle && is_left(buf, size))
                        {
                                assert(*fle != NULL);
                                // compaction
                                *fle = *((void **)*fle);
                                buddy_free(buf, size * 2);
                                return;
                        }
                        *((void **)buf) = *fle;
                        *fle = buf;
                        return;
                }
                if (do_compaction && *fle + size == buf && is_left(*fle, size))
                {
                        // compaction
                        void *p = *fle;
                        *fle = *((void **)*fle);
                        buddy_free(p, size * 2);
                        return;
                }
                fle = *fle;
        }
}

void aurora_mem_alloc(vfio_dma_t *buf, size_t size)
{
        //size_t mask = 4096 /* x86 pagesize */ - 1;
        //size = (size + mask) & ~mask;
        size_t tsize = 4096;
        while (tsize < size)
        {
                tsize *= 2;
        }
        size = tsize;

        void *p = buddy_malloc(size);
        if (p == NULL)
        {
                fprintf(stderr, "UNVME_ERROR: memory exhausted (size:%ld)\n", size);
                fflush(stderr);
                abort();
        }

        buf->buf = p;
        buf->addr = pci_vhsaa + (p - vemva);
        buf->size = size;
        buf->next = NULL;
}

void aurora_mem_free(vfio_dma_t *dma_ctx)
{
        buddy_free(dma_ctx->buf, dma_ctx->size);
}
#endif

uint64_t aurora_resolve_addr(void *buf)
{
        uint64_t _buf = (uint64_t)buf;
        uint64_t _vemva = (uint64_t)vemva;
        if (_vemva > _buf || (_buf - _vemva) > MAX_SIZE)
        {
                fprintf(stderr, "UNVME_ERROR: try to use non I/O region\n");
                fflush(stderr);
                abort();
        }

        return pci_vhsaa + (_buf - _vemva);
}
