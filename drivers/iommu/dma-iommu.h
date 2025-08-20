/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2014-2015 ARM Ltd.
 */
#ifndef __DMA_IOMMU_H
#define __DMA_IOMMU_H

#include <linux/iommu.h>

#ifdef CONFIG_IOMMU_DMA

void iommu_setup_dma_ops(struct device *dev, struct iommu_domain *domain);

int iommu_get_dma_cookie(struct iommu_domain *domain);
void iommu_put_dma_cookie(struct iommu_domain *domain);
void iommu_put_msi_cookie(struct iommu_domain *domain);

int iommu_dma_init_fq(struct iommu_domain *domain);

void iommu_dma_get_resv_regions(struct device *dev, struct list_head *list);

int iommu_dma_sw_msi(struct iommu_domain *domain, struct msi_desc *desc,
		     phys_addr_t msi_addr);

#ifdef CONFIG_UB_UMMU_CORE
struct iova_domain *iommu_get_iova_domain(struct iommu_domain *domain);
#endif

extern bool iommu_dma_forcedac;

#else /* CONFIG_IOMMU_DMA */

static inline void iommu_setup_dma_ops(struct device *dev,
				       struct iommu_domain *domain)
{
}

static inline int iommu_dma_init_fq(struct iommu_domain *domain)
{
	return -EINVAL;
}

static inline int iommu_get_dma_cookie(struct iommu_domain *domain)
{
	return -ENODEV;
}

static inline void iommu_put_dma_cookie(struct iommu_domain *domain)
{
}

static inline void iommu_put_msi_cookie(struct iommu_domain *domain)
{
}

static inline void iommu_dma_get_resv_regions(struct device *dev, struct list_head *list)
{
}

static inline int iommu_dma_sw_msi(struct iommu_domain *domain,
				   struct msi_desc *desc, phys_addr_t msi_addr)
{
	return -ENODEV;
}

#ifndef CONFIG_UB_UMMU_CORE
static inline struct iova_domain *iommu_get_iova_domain(struct iommu_domain *domain)
{
	return NULL;
}
#endif

#endif	/* CONFIG_IOMMU_DMA */
#endif	/* __DMA_IOMMU_H */
