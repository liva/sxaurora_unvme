#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdatomic.h>
#include "aurora_pci.h"
#include "unvme_lock.h"

#define SIZE_64M (1UL << 26)
#define PCIATB_PAGESIZE (1UL << 26)
#define MAX_SIZE (PCIATB_PAGESIZE * 256)
// TODO: should implement lock

static uint64_t vehva = 0;
static atomic_uint lock = ATOMIC_VAR_INIT(0);
;

uint64_t aurora_map(uint64_t address)
{
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
        printf("aurora_map: %#lx-%#lx\n", vehva, vehva + size);
        return vehva + (address % SIZE_64M);
}

static uint64_t pci_vhsaa;
static vfio_dma_t *buf_list;

int aurora_init()
{
        void *vemva;
        // these VE memory should be used for data buffer
        vemva = mmap(NULL, MAX_SIZE, PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_SHARED | MAP_64MB, -1, 0);
        if (NULL == vemva)
        {
                perror("Fail to allocate memory");
                return 0;
        }

        pci_vhsaa = ve_register_mem_to_pci(vemva, MAX_SIZE);
        if (pci_vhsaa == (uint64_t)-1)
        {
                perror("Fail to ve_register_mem_to_pci()");
                return 0;
        }
        printf("aurora_init: %#lx-%#lx\n", vemva, vemva + MAX_SIZE);

        buf_list = malloc(sizeof(vfio_dma_t));
        buf_list->buf = vemva;
        buf_list->size = MAX_SIZE;
        buf_list->addr = pci_vhsaa;
        buf_list->next = NULL;
        return 1;
}

int aurora_release()
{
        int ret;
        ret = ve_unregister_mem_from_pci(pci_vhsaa, MAX_SIZE);
        if (ret)
        {
                perror("Fail to ve_unregister_mem_from_pci()");
                return 0;
        }
        if (vehva != 0)
        {
                ret = ve_unregister_pci_from_vehva(vehva, SIZE_64M);
                if (ret)
                {
                        perror("Fail to ve_unregister_pci_to_vehva");
                        return 0;
                }
        }
        return 1;
}

vfio_dma_t *aurora_mem_alloc(size_t size)
{
        vfio_dma_t *buf = malloc(sizeof(vfio_dma_t));
        size_t mask = 4096 /* x86 pagesize */ - 1;
        size = (size + mask) & ~mask;

        while (atomic_fetch_or(&lock, 1) != 0)
        {
                asm volatile("" ::
                                 : "memory");
        }
        vfio_dma_t *buf_cur = buf_list;
        while (buf_cur->next != NULL)
        {
                vfio_dma_t *next = buf_cur->next;
                if (buf_cur->addr > next->addr)
                {
                        vfio_dma_t tmp = *buf_cur;
                        *buf_cur = *next;
                        *next = tmp;
                        next->next = buf_cur->next;
                        buf_cur->next = next;
                }

                if (buf_cur->addr + buf_cur->size == next->addr)
                {
                        buf_cur->size += next->size;
                        buf_cur->next = next->next;
                        free(next);
                }

                if (buf_cur->size >= size)
                {
                        goto alloc;
                }
                buf_cur = buf_cur->next;
        }

        if (buf_cur->size >= size)
        {
                goto alloc;
        }

        lock = ATOMIC_VAR_INIT(0);

        fprintf(stderr, "UNVME_ERROR: memory exhausted\n");
        abort();
        return NULL;
alloc:
        *buf = *buf_cur;

        buf_cur->buf += size;
        buf_cur->size -= size;
        buf_cur->addr += size;

        lock = ATOMIC_VAR_INIT(0);

        buf->size = size;
        buf->next = NULL;

        memset(buf->buf, 0, size);
        return buf;
}

void aurora_mem_free(vfio_dma_t *dma_ctx)
{
        while (atomic_fetch_or(&lock, 1) != 0)
        {
                asm volatile("" ::
                                 : "memory");
        }
        dma_ctx->next = buf_list;
        buf_list = dma_ctx;
        lock = ATOMIC_VAR_INIT(0);
}