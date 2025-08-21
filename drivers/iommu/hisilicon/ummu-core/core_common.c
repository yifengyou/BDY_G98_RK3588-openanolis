// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright(c) 2025 HiSilicon Technologies CO., All rights reserved.
 * Description: common built-in symbols.
 */

#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/dma-map-ops.h>

#include "ummu_core_priv.h"

struct ummu_core_device *global_core_device;
EXPORT_SYMBOL_NS_GPL(global_core_device, UMMU_CORE_INTERNAL);

DEFINE_MUTEX(global_device_lock);
EXPORT_SYMBOL_NS_GPL(global_device_lock, UMMU_CORE_INTERNAL);

void setup_tdev_dma_ops(struct device *dev, bool coherent)
{
	arch_setup_dma_ops(dev, coherent);
}
EXPORT_SYMBOL_NS_GPL(setup_tdev_dma_ops, UMMU_CORE_INTERNAL);
