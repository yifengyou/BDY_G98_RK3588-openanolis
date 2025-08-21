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

#define UB_MAX_TID_BITS 20U
#define UB_MAX_TID ((1 << UB_MAX_TID_BITS) - 1)

#define UMMU_NO_TID 0U
#define UMMU_INVALID_TID UB_MAX_TID

enum eid_type {
	EID_NONE = 0,
	EID_BYPASS,
	EID_TYPE_MAX,
};

enum tid_alloc_mode {
	TID_ALLOC_TRANSPARENT = 0,
	TID_ALLOC_ASSIGNED = 1,
	TID_ALLOC_NORMAL = 2,
};

enum default_tid_ops_types {
	PASID_OPS,
	DEFAULT_OPS,
	TID_OPS_MAX,
};

enum ummu_mapt_mode {
	MAPT_MODE_TABLE = 0,
	MAPT_MODE_ENTRY,
	MAPT_MODE_END,
};

struct iova_slot;
struct ummu_tid_manager;
struct ummu_base_domain;
struct ummu_core_device;

struct ummu_tid_param {
	struct device *device;
	enum ummu_mapt_mode mode;
	enum tid_alloc_mode alloc_mode;
	u32 assign_tid;
};

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
 * @tid_manager: tid domain manager.
 * @iommu: iommu prototype
 * @ops: ummu-core defined iommu operations
 */
struct ummu_core_device {
	struct list_head list;
	struct ummu_tid_manager *tid_manager;
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
struct tid_ops {
	struct ummu_tid_manager *(*alloc_tid_manager)(
		struct ummu_core_device *core_device, u32 min_tid,
		u32 max_tid);
	void (*free_tid_manager)(struct ummu_tid_manager *manager);
	int (*alloc_tid)(struct ummu_tid_manager *manager,
			 struct ummu_tid_param *param, u32 *tidp);
	void (*free_tid)(struct ummu_tid_manager *manager, u32 tid);
};

struct ummu_tid_manager {
	const struct tid_ops *ops;
	struct xarray token_ids;
	u32 min_tid;
	u32 max_tid;
	void *tid_data;
};

#if IS_ENABLED(CONFIG_UB_UMMU_CORE_DRIVER)
extern const struct tid_ops *ummu_core_tid_ops[TID_OPS_MAX];
#else
static const struct tid_ops *ummu_core_tid_ops[TID_OPS_MAX];
#endif /* CONFIG_UB_UMMU_CORE_DRIVER */

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

#if IS_ENABLED(CONFIG_UB_UMMU_CORE_DRIVER)
/* UMMU TID API */
/**
 * Alloc a tid from ummu framework, and alloc related pasid.
 * @dev: the allocated tid will be attached to.
 * @drvdata: ummu_tid_param related to tid
 * @tidp: the allocated tid returned here.
 *
 * Return: 0 on success, or an error.
 */
int ummu_core_alloc_tid(struct ummu_core_device *dev,
			struct ummu_tid_param *drvdata, u32 *tidp);

/**
 * Free a tid to ummu framework.
 * @dev: the ummu_core device tid belongs to.
 * @tid: token id.
 */
void ummu_core_free_tid(struct ummu_core_device *dev, u32 tid);

/**
 * Get mapt_mode related to the tid.
 * @dev: the ummu_core device tid belongs to.
 * @tid: token id.
 *
 * Return: ummu_mapt_mode, negative for an error.
 */
enum ummu_mapt_mode ummu_core_get_mapt_mode(struct ummu_core_device *dev,
					    u32 tid);

/**
 * Get device related to the tid.
 * It will increase the ref count of the device.
 * @dev: the ummu_core device tid belongs to.
 * @tid: token id.
 *
 * Return: device or NULL if failed.
 */
struct device *ummu_core_get_device(struct ummu_core_device *dev, u32 tid);
void ummu_core_put_device(struct device *dev);
#else
static inline int ummu_core_alloc_tid(struct ummu_core_device *dev,
				      struct ummu_tid_param *drvdata,
				      u32 *tidp)
{
	return -EOPNOTSUPP;
}

static inline void ummu_core_free_tid(struct ummu_core_device *dev,
				      u32 tid)
{
}
static inline enum ummu_mapt_mode
ummu_core_get_mapt_mode(struct ummu_core_device *dev, u32 tid)
{
	return MAPT_MODE_END;
}

static inline struct device *ummu_core_get_device(struct ummu_core_device *dev,
						  u32 tid)
{
	return NULL;
}

static inline void ummu_core_put_device(struct device *dev)
{
}
#endif /* CONFIG_UB_UMMU_CORE_DRIVER */
#endif /* _UMMU_CORE_H_ */
