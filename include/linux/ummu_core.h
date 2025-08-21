/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright(c) 2025 HiSilicon Technologies CO., All rights reserved.
 * Description: UMMU-CORE defines and function prototypes.
 */

#ifndef _UMMU_CORE_H_
#define _UMMU_CORE_H_

#include <linux/iommu.h>
#include <linux/uuid.h>
#include <linux/xarray.h>

#define eid_t u128

enum eid_type {
	EID_NONE = 0,
	EID_BYPASS,
	EID_TYPE_MAX,
};

struct iova_slot;
struct ummu_base_domain;
struct ummu_core_device;

/**
 * struct ummu_core_ops - ummu ops for normal use, expand from iommu_ops.
 * @add_eid: Add EID to the UMMU device.
 * @del_eid: Add EID to the UMMU device.
 */
struct ummu_core_ops {
	int (*add_eid)(struct ummu_core_device *dev, guid_t *guid, eid_t eid,
		       enum eid_type type);
	void (*del_eid)(struct ummu_core_device *dev, guid_t *guid, eid_t eid,
			enum eid_type type);
};

/**
 * ummu-core defined iommu device type
 * @list: used to link all ummu-core devices
 * @iommu: iommu prototype
 * @ops: ummu-core defined iommu operations
 */
struct ummu_core_device {
	struct list_head list;
	struct iommu_device iommu;
	const struct ummu_core_ops *ops;
};

struct ummu_base_domain {
	struct iommu_domain domain;
	struct ummu_core_device *core_dev;
	struct iommu_domain *parent;
	struct list_head list;
	u32 tid;
};

static inline struct ummu_core_device *to_ummu_core(struct iommu_device *iommu)
{
	return container_of(iommu, struct ummu_core_device, iommu);
}

static inline struct ummu_base_domain *
to_ummu_base_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct ummu_base_domain, domain);
}

#ifdef CONFIG_UB_UMMU_CORE
/* EID API */
/**
 * Add a new EID to the UMMU.
 * @guid: entity/device identity.
 * @eid: entity id to be added.
 * @type: eid type.
 *
 * Return: 0 on success, or an error.
 */
int ummu_core_add_eid(guid_t *guid, eid_t eid, enum eid_type type);
/**
 * Delete an EID from the UMMU.
 * @guid: entity/device identity.
 * @eid: entity id to be deleted.
 * @type: eid type.
 */
void ummu_core_del_eid(guid_t *guid, eid_t eid, enum eid_type type);

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
static inline int ummu_core_add_eid(guid_t *guid, eid_t eid, enum eid_type type)
{
	return -EOPNOTSUPP;
}

static inline void ummu_core_del_eid(guid_t *guid, eid_t eid,
				     enum eid_type type)
{
}

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
