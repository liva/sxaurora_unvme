#ifndef _AURORA_PCI_H_
#define _AURORA_PCI_H_

#include <stdint.h>
#include <vepci.h>
#include "unvme_vfio.h"

uint64_t aurora_map(uint64_t address);
int aurora_init();
vfio_dma_t *aurora_mem_alloc(size_t size);
int aurora_release();

/**
 * @brief This function loads 32 bit data from address mapped to VEHVA
 *
 * @param[in] vehva Address of data
 *
 * @return "32bit size data"
 */
inline uint32_t ve_pci_load32(uint64_t vehva) __attribute__((always_inline));
inline uint32_t ve_pci_load32(uint64_t vehva)
{
	uint32_t ret;
	asm volatile(
		"       lhm.w   %0, 0(%1)\n"
		"	fencem 2\n"
			: "=r"(ret)
			: "r"(vehva));
	return ret;
}

/**
 * @brief This function stores 32 bit data to address mapped to VEHVA
 *
 * @param[in] vehva Address of data
 * @param[in] value 32 bit size data
 */
inline void ve_pci_store32(uint64_t vehva, uint32_t value) __attribute__((always_inline));
inline void ve_pci_store32(uint64_t vehva, uint32_t value)
{
	asm volatile(
		"	fencem 1\n"
		"       shm.w   %0, 0(%1)\n"
			:: "r"(value), "r"(vehva));
}


#endif // _AURORA_PCI_H_
