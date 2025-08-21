// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright(c) 2025 HiSilicon Technologies CO., All rights reserved.
 * Description: UMMU CORE API, extend the IOMMU Framework.
 */

#define pr_fmt(fmt) "[UMMU_CORE]: " fmt
#include <linux/module.h>
#include <ub/ubus/ubus.h>

#include "ummu_core_priv.h"

int ummu_get_tid(struct device *dev, struct iommu_sva *sva, uint32_t *tidp)
{
	struct iommu_domain *domain;

	if (!dev || !tidp)
		return -EINVAL;

	if (sva)
		domain = sva->handle.domain;
	else
		domain = iommu_get_domain_for_dev(dev);

	if (!domain)
		return -ENODEV;

	*tidp = to_ummu_base_domain(domain)->tid;
	return 0;
}

MODULE_IMPORT_NS(UMMU_CORE_INTERNAL);
MODULE_DESCRIPTION("UMMU Framework");
MODULE_AUTHOR("HUAWEI TECHNOLOGIES CO., LTD.");
MODULE_LICENSE("GPL");
