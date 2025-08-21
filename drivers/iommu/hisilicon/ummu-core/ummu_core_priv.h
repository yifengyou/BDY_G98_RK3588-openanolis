/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2025 HiSilicon Technologies Co., Ltd. All rights reserved.
 * Description: UMMU CORE private interfaces.
 */

#ifndef __UMMU_CORE_PRIV_H__
#define __UMMU_CORE_PRIV_H__

#include <linux/platform_device.h>
#include <linux/ummu_core.h>
#include <linux/iommu.h>

/* private initialization */
void tdev_exit(void);
int tdev_init(void);

struct tid_dev {
	struct platform_device pdev;
	struct iommu_sva *sva;
};

/* private definition */
#define to_tid_dev(n) container_of(to_platform_device(n), struct tid_dev, pdev)

/* private variable */
extern struct ummu_core_device *global_core_device;
extern struct mutex global_device_lock;
extern struct list_head core_device_list;
extern struct device tid_bus;

/* private interfaces */
void ummu_flush_cached_eid(struct ummu_core_device *core_device);
struct device *ummu_alloc_tdev(struct tdev_attr *attr, u32 *ptid);
int ummu_get_tid(struct device *dev, struct iommu_sva *sva, uint32_t *tidp);
void setup_tdev_dma_ops(struct device *dev, bool coherent);

#endif /* __UMMU_CORE_PRIV_H__ */
