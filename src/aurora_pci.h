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

#ifndef _AURORA_PCI_H_
#define _AURORA_PCI_H_

#include <stdint.h>
#include <vepci.h>
#include <assert.h>
#include "unvme_vfio.h"

uint64_t aurora_map(uint64_t address);
uint64_t aurora_resolve_addr(void *buf);
int aurora_init();
void aurora_mem_alloc(vfio_dma_t *dma, size_t size);
void aurora_mem_free(vfio_dma_t *dma_ctx);
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
		"       shm.w   %0, 0(%1)\n" ::"r"(value),
		"r"(vehva));
}

#endif // _AURORA_PCI_H_
