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
 * @brief VFIO support routines.
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <linux/pci.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>

#include "unvme_vfio.h"
#include "unvme_log.h"
#include "aurora_pci.h"

/// Print fatal error and exit
#define FATAL(fmt, arg...)  do { ERROR(fmt, ##arg); abort(); } while (0)

/**
 * Create a VFIO device context.
 * @param   dev         if NULL then allocate context
 * @param   pci         PCI device id (as %x:%x.%x format)
 * @return  device context or NULL if failure.
 */
vfio_device_t* vfio_create(vfio_device_t* dev, int pci)
{
    // map PCI to vfio device number
    int i;
    char pciname[64];
    sprintf(pciname, "0000:%02x:%02x.%x", pci >> 16, (pci >> 8) & 0xff, pci & 0xff);

/*    char path[128];
    sprintf(path, "/sys/bus/pci/devices/%s/iommu_group", pciname);
    if ((i = readlink(path, path, sizeof(path))) < 0)
        FATAL("No iommu_group associated with device %s", pciname);
    path[i] = 0;
    sprintf(path, "/dev/vfio%s", strrchr(path, '/'));
    
    struct vfio_group_status group_status = { .argsz = sizeof(group_status) };
    struct vfio_iommu_type1_info iommu_info = { .argsz = sizeof(iommu_info) };
    struct vfio_device_info dev_info = { .argsz = sizeof(dev_info) };
*/
    // allocate and initialize device context
    if (!dev) dev = zalloc(sizeof(*dev));
    else dev->ext = 1;
    dev->pci = pci;
 /*   dev->pagesize = sysconf(_SC_PAGESIZE);
    dev->iovabase = VFIO_IOVA;
    dev->iovanext = dev->iovabase;*/
    if (pthread_mutex_init(&dev->lock, 0)) return NULL;

    if (!aurora_init()) {
        errx(1, "aurora_init");
    }
/*
    // map vfio context
    if ((dev->contfd = open("/dev/vfio/vfio", O_RDWR)) < 0)
        FATAL("open /dev/vfio/vfio");

    if (ioctl(dev->contfd, VFIO_GET_API_VERSION) != VFIO_API_VERSION)
        FATAL("ioctl VFIO_GET_API_VERSION");

    if (ioctl(dev->contfd, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU) == 0)
        FATAL("ioctl VFIO_CHECK_EXTENSION");

    if ((dev->groupfd = open(path, O_RDWR)) < 0)
        FATAL("open %s failed", path);

    if (ioctl(dev->groupfd, VFIO_GROUP_GET_STATUS, &group_status) < 0)
        FATAL("ioctl VFIO_GROUP_GET_STATUS");

    if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE))
        FATAL("group not viable %#x", group_status.flags);

    if (ioctl(dev->groupfd, VFIO_GROUP_SET_CONTAINER, &dev->contfd) < 0)
        FATAL("ioctl VFIO_GROUP_SET_CONTAINER");

    if (ioctl(dev->contfd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU) < 0)
        FATAL("ioctl VFIO_SET_IOMMU");

    if (ioctl(dev->contfd, VFIO_IOMMU_GET_INFO, &iommu_info) < 0)
        FATAL("ioctl VFIO_IOMMU_GET_INFO");

    dev->fd = ioctl(dev->groupfd, VFIO_GROUP_GET_DEVICE_FD, pciname);
    if (dev->fd < 0)
        FATAL("ioctl VFIO_GROUP_GET_DEVICE_FD");

    if (ioctl(dev->fd, VFIO_DEVICE_GET_INFO, &dev_info) < 0)
        FATAL("ioctl VFIO_DEVICE_GET_INFO");

    DEBUG_FN("%x flags=%u regions=%u irqs=%u",
             pci, dev_info.flags, dev_info.num_regions, dev_info.num_irqs);

    for (i = 0; i < dev_info.num_regions; i++) {
        struct vfio_region_info reg = { .argsz = sizeof(reg), .index = i };

        if (ioctl(dev->fd, VFIO_DEVICE_GET_REGION_INFO, &reg)) continue;

        DEBUG_FN("%x region=%d flags=%#x off=%#llx size=%#llx",
                 pci, reg.index, reg.flags, reg.offset, reg.size);

        if (i == VFIO_PCI_CONFIG_REGION_INDEX) {
            __u8 config[256];
            vfio_read(dev, config, sizeof(config), reg.offset);
            HEX_DUMP(config, sizeof(config));

            __u16* vendor = (__u16*)(config + PCI_VENDOR_ID);
            __u16* cmd = (__u16*)(config + PCI_COMMAND);

            if (*vendor == 0xffff)
                FATAL("device in bad state");

            *cmd |= PCI_COMMAND_MASTER|PCI_COMMAND_MEMORY|PCI_COMMAND_INTX_DISABLE;
            vfio_write(dev, cmd, sizeof(*cmd), reg.offset + PCI_COMMAND);
            vfio_read(dev, cmd, sizeof(*cmd), reg.offset + PCI_COMMAND);

            // read MSIX table size
            __u8 cap = config[PCI_CAPABILITY_LIST];
            while (cap) {
                if (config[cap] == PCI_CAP_ID_MSIX) {
                    __u16* msixflags = (__u16*)(config + cap + PCI_MSIX_FLAGS);
                    dev->msixsize = (*msixflags & PCI_MSIX_FLAGS_QSIZE) + 1;
                    break;
                }
                cap = config[cap+1];
            }

            DEBUG_FN("%x vendor=%#x cmd=%#x msix=%d device=%#x rev=%d",
                     pci, *vendor, *cmd, dev->msixsize,
                     (__u16*)(config + PCI_DEVICE_ID), config[PCI_REVISION_ID]);
        }
    }*/

/*    for (i = 0; i < dev_info.num_irqs; i++) {
        struct vfio_irq_info irq = { .argsz = sizeof(irq), .index = i };

        if (ioctl(dev->fd, VFIO_DEVICE_GET_IRQ_INFO, &irq)) continue;
        DEBUG_FN("%x irq=%s count=%d flags=%#x",
                 pci, vfio_irq_names[i], irq.count, irq.flags);
        if (i == VFIO_PCI_MSIX_IRQ_INDEX && irq.count != dev->msixsize)
            FATAL("VFIO_DEVICE_GET_IRQ_INFO MSIX count %d != %d", irq.count, dev->msixsize);
    }
*/
/*#ifdef  UNVME_IDENTITY_MAP_DMA
    // Set up mask to support identity IOVA map option
    struct vfio_iommu_type1_dma_map map = {
        .argsz = sizeof(map),
        .flags = (VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE),
        .iova = dev->iovabase,
        .size = dev->pagesize,
    };
    struct vfio_iommu_type1_dma_unmap unmap = {
        .argsz = sizeof(unmap),
        .size = dev->pagesize,
    };

    map.vaddr = (__u64)mmap(0, map.size, PROT_READ|PROT_WRITE,
                            MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if ((void*)map.vaddr == MAP_FAILED)
            FATAL("mmap: %s", strerror(errno));
    while (map.iova) {
        if (ioctl(dev->contfd, VFIO_IOMMU_MAP_DMA, &map) < 0) {
            if (errno == EFAULT) break;
            FATAL("VFIO_IOMMU_MAP_DMA: %s", strerror(errno));
        }
        unmap.iova = map.iova;
        if (ioctl(dev->contfd, VFIO_IOMMU_UNMAP_DMA, &unmap) < 0)
            FATAL("VFIO_IOMMU_MUNAP_DMA: %s", strerror(errno));
        map.iova <<= 1;
    }
    dev->iovamask = map.iova - 1;
    (void) munmap((void*)map.vaddr, map.size);
    DEBUG_FN("iovamask=%#llx", dev->iovamask);
#endif*/

    return (vfio_device_t*)dev;
}

/**
 * Delete a VFIO device context.
 * @param   dev         device context
 */
void vfio_delete(vfio_device_t* dev)
{
    if (!dev) return;
    DEBUG_FN("%x", dev->pci);

/*    // free all memory associated with the device
    while (dev->memlist) vfio_mem_free(dev->memlist);

    if (dev->fd) {
        close(dev->fd);
        dev->fd = 0;
    }
    if (dev->contfd) {
        close(dev->contfd);
        dev->contfd = 0;
    }
    if (dev->groupfd) {
        close(dev->groupfd);
        dev->groupfd = 0;
    }
    */

    if (!aurora_release()) {
        errx(1, "aurora_release");
    }

    pthread_mutex_destroy(&dev->lock);
    if (!dev->ext) free(dev);
}


/**
 * Allocate and return a DMA buffer.
 * @param   dev         device context
 * @param   size        allocation size
 * @return  0 if ok else -1.
 */
vfio_dma_t* vfio_dma_alloc(vfio_device_t* dev, size_t size)
{
    pthread_mutex_lock(&dev->lock);
    vfio_dma_t *mem = aurora_mem_alloc(size);
    pthread_mutex_unlock(&dev->lock);
    return mem;
}

/**
 * Free a DMA buffer.
 * @param   dma         memory pointer
 * @return  0 if ok else -1.
 */
int vfio_dma_free(vfio_dma_t* dma)
{
    free(dma);
    return 0;
}
