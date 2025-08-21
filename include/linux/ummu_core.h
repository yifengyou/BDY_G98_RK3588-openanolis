/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright(c) 2025 HiSilicon Technologies CO., All rights reserved.
 * Description: UMMU-CORE defines and function prototypes.
 */

#ifndef _UMMU_CORE_H_
#define _UMMU_CORE_H_

#include <linux/iommu.h>

struct iova_slot;

#ifdef CONFIG_UB_UMMU_CORE
/* UMMU IOVA API */
/**
 * Allocate a range of IOVA. The input iova size might be aligned.
 * @dev: related device.
 * @size: iova size.
 * @attrs: dma attributes.
 * @iovap: iova address returned here.
 * @sizep: iova size returned here.
 *
 * Return: iova slot which managed the iova range, or an ERR_PTR.
 */
struct iova_slot *dma_alloc_iova(struct device *dev, size_t size,
				 unsigned long attrs, dma_addr_t *iovap,
				 size_t *sizep);

/**
 * Free a range of IOVA.
 * The API is not thread-safe.
 * @slot: iova slot, generated from dma_alloc_iova.
 */
void dma_free_iova(struct iova_slot *slot);

/**
 * Fill a range of IOVA. It allocates pages and maps pages to the iova.
 * The API is not thread-safe.
 * @slot: iova slot, generated from dma_alloc_iova.
 * @iova: iova start.
 * @nr_pages: fill pages count.
 *
 * Return: 0 on success, or an error number.
 */
int ummu_fill_pages(struct iova_slot *slot, dma_addr_t iova, unsigned long nr_pages);

/**
 * Drain a range of IOVA. It unmaps iova and releases pages.
 * The API is not thread-safe.
 * @slot: iova slot, generated from dma_alloc_iova.
 * @iova: iova start.
 * @nr_pages: drain pages count.
 *
 * Return: 0 on success, or an error number.
 */
int ummu_drain_pages(struct iova_slot *slot, dma_addr_t iova, unsigned long nr_pages);
#else
static inline struct iova_slot *dma_alloc_iova(struct device *dev, size_t size,
					       unsigned long attrs,
					       dma_addr_t *iovap, size_t *sizep)
{
	return ERR_PTR(-EOPNOTSUPP);
}

static inline void dma_free_iova(struct iova_slot *slot)
{
}
static inline int ummu_fill_pages(struct iova_slot *slot, dma_addr_t iova,
				  unsigned long nr_pages)
{
	return -EOPNOTSUPP;
}

static inline int ummu_drain_pages(struct iova_slot *slot, dma_addr_t iova,
				   unsigned long nr_pages)
{
	return -EOPNOTSUPP;
}
#endif /* CONFIG_UB_UMMU_CORE */
#endif /* _UMMU_CORE_H_ */
