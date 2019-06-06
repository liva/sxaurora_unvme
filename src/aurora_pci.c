#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <sys/mman.h>
#include "aurora_pci.h"
#include "unvme_lock.h"

#define SIZE_64M (1UL << 26)
#define PCIATB_PAGESIZE (1UL << 26)
#define MAX_SIZE (PCIATB_PAGESIZE * 256)

static uint64_t vehva = 0;

uint64_t aurora_map(uint64_t address) {
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

static void *vemva;
static uint64_t pci_vhsaa;

static void *vmava_cnt;
static uint64_t pci_vhsaa_cnt;

int aurora_init() {
        // these VE memory should be used for data buffer
       	vemva = mmap(NULL, MAX_SIZE, PROT_READ|PROT_WRITE,
		MAP_ANONYMOUS|MAP_SHARED|MAP_64MB, -1, 0);
        if (NULL == vemva) {
		perror("Fail to allocate memory");
		return 0;
	}
        vmava_cnt = vemva;

	pci_vhsaa = ve_register_mem_to_pci(vemva, MAX_SIZE);
	if (pci_vhsaa == (uint64_t)-1) {
		perror("Fail to ve_register_mem_to_pci()");
		return 0;
	}
        printf("aurora_init: %#lx-%#lx\n", vemva, vemva + MAX_SIZE);
        pci_vhsaa_cnt = pci_vhsaa;
        return 1;
}

int aurora_release() {
        int ret;
        ret = ve_unregister_mem_from_pci(pci_vhsaa, MAX_SIZE);
        if (ret) {
                perror("Fail to ve_unregister_mem_from_pci()");
                return 0;
        }
        if (vehva != 0) {
                ret = ve_unregister_pci_from_vehva(vehva, SIZE_64M);
                if (ret) {
		        perror("Fail to ve_unregister_pci_to_vehva");
		        return 0;
	        }
        }
        return 1;
}

vfio_dma_t *aurora_mem_alloc(size_t size) {
        // allocate data buffer
        if (pci_vhsaa_cnt + size > pci_vhsaa + MAX_SIZE) {
                return NULL;
        }

        vfio_dma_t *buf = malloc(sizeof(vfio_dma_t));
        buf->buf = vmava_cnt;
        buf->size = size;
        buf->addr = pci_vhsaa_cnt;
        size_t mask = 4096 /* x86 pagesize */ - 1;
        size = (size + mask) & ~mask;
        pci_vhsaa_cnt += size;
        vmava_cnt = (void *)(size + (size_t)vmava_cnt);

        memset(buf->buf, 0, size);
        return buf;
}
