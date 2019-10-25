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
#define MAX_SIZE (PCIATB_PAGESIZE * 512)
// TODO: should implement lock

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

//#define MEM_SIMPLE
#ifdef MEM_SIMPLE
int aurora_init()
{

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

        buf_list = malloc(sizeof(vfio_dma_t));
        buf_list->buf = vemva;
        buf_list->size = MAX_SIZE;
        buf_list->addr = pci_vhsaa;
        buf_list->next = NULL;
        return 1;
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
                memset(buf->buf, 0, size);
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
static void **freelist;
static int entry_max;
static void *start_addr;

void buddy_dump()
{
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
        freelist = malloc(sizeof(void *) * entry_max);
        for (int i = 0; i < entry_max; i++)
        {
                freelist[i] = NULL;
        }
        void **fle = &freelist[entry_max - 1];
        for (size_t offset = 0; offset < size; offset += maxbytes)
        {
                *fle = addr + offset;
                fle = addr + offset;
        }
        *fle = NULL;
}

void *buddy_malloc(size_t size)
{
        int i = get_index_from_size(size);
        if (i == -1)
        {
                return NULL;
        }
        void *rval = freelist[i];
        if (rval != NULL)
        {
                freelist[i] = *((void **)rval);
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

int aurora_init()
{

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

        buddy_init(vemva, MAX_SIZE);
        return 1;
}

vfio_dma_t *aurora_mem_alloc(size_t size)
{
        //size_t mask = 4096 /* x86 pagesize */ - 1;
        //size = (size + mask) & ~mask;
        size_t tsize = 4096;
        while (tsize < size)
        {
                tsize *= 2;
        }
        size = tsize;

        while (atomic_fetch_or(&lock, 1) != 0)
        {
                asm volatile("" ::
                                 : "memory");
        }

        void *p = buddy_malloc(size);
        if (p == NULL)
        {
                fprintf(stderr, "UNVME_ERROR: memory exhausted (size:%d)\n", size);
                fflush(stderr);
                abort();
                return NULL;
        }

        vfio_dma_t *buf = malloc(sizeof(vfio_dma_t));
        buf->buf = p;
        buf->addr = pci_vhsaa + (p - vemva);
        buf->size = size;
        buf->next = NULL;
        memset(buf->buf, 0, size);

        lock = ATOMIC_VAR_INIT(0);
        return buf;
}

void aurora_mem_free(vfio_dma_t *dma_ctx)
{
        while (atomic_fetch_or(&lock, 1) != 0)
        {
                asm volatile("" ::
                                 : "memory");
        }
        buddy_free(dma_ctx->buf, dma_ctx->size);
        free(dma_ctx);
        lock = ATOMIC_VAR_INIT(0);
}
#endif

uint64_t aurora_resolve_addr(void *buf)
{
        return pci_vhsaa + (buf - vemva);
}
