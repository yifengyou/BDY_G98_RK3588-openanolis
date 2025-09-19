// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) HiSilicon Technologies Co., Ltd. 2025. All rights reserved.
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/iommu.h>

#include "ubus_inner.h"

static struct fwnode_handle *iommu_fwnode;
void ub_iommu_fwnode_handle_set(const struct fwnode_handle *fwnode)
{
	iommu_fwnode = (struct fwnode_handle *)fwnode;
}
EXPORT_SYMBOL_GPL(ub_iommu_fwnode_handle_set);

const struct fwnode_handle *ub_iommu_fwnode_handle_get(void)
{
	return iommu_fwnode;
}
EXPORT_SYMBOL_GPL(ub_iommu_fwnode_handle_get);

static void ubct_dma_setup(struct device *dev)
{
	struct ub_entity *uent = to_ub_entity(dev);
	u64 end, mask, size;
	u8 addr_limit;

	addr_limit = uent->ubc->attr.mem_size_limit;
	if (!addr_limit) {
		pr_warn(FW_BUG "ub bus controller missing memory address limit\n");
		return;
	}

	size = (addr_limit >= SZ_64) ? U64_MAX : (1ULL << addr_limit);
	end = size - 1;
	mask = DMA_BIT_MASK(ilog2(end) + 1);
	dev->bus_dma_limit = end;
	dev->coherent_dma_mask = min(dev->coherent_dma_mask, mask);
	*dev->dma_mask = min(*dev->dma_mask, mask);
}

static int ubct_iommu_fwspec_init(struct device *dev, u32 id,
				  struct fwnode_handle *fwnode,
				  bool *fwspec_exists)
{
	int ret;

	*fwspec_exists = dev_iommu_fwspec_get(dev) != NULL;
	ret = iommu_fwspec_init(dev, fwnode);
	if (ret)
		return ret;
	if (*fwspec_exists)
		return 0;

	return iommu_fwspec_add_ids(dev, &id, 1);
}

static int ubct_iommu_configure(struct device *dev, bool *fwspec_exists)
{
	if (!iommu_fwnode) {
		dev_err(dev, "ubus's iommu_fwnode not ready\n");
		return -ENODEV;
	}

	/* input id is 0, ummu not care */
	return ubct_iommu_fwspec_init(dev, 0, iommu_fwnode, fwspec_exists);
}

static int handle_iommu_configure_error(struct device *dev, int err)
{
	if (err == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	dev_warn(dev, "Adding to IOMMU failed: %d\n", err);
	return 0;
}

static int ub_hybrid_iommu_configure(struct device *dev)
{
	bool fwspec_exists;
	int err;

	err = ubct_iommu_configure(dev, &fwspec_exists);
	if (err) {
		if (err != -EPROBE_DEFER)
			dev_err(dev, "viot iommu configure: %d\n", err);

		return handle_iommu_configure_error(dev, err);
	}
	if (fwspec_exists)
		return 0;

	if (dev->bus && !device_iommu_mapped(dev)) {
		err = iommu_probe_device(dev);
		if (err)
			return handle_iommu_configure_error(dev, err);
	}

	return 0;
}

static int ub_hybrid_dma_configure(struct device *dev, enum dev_dma_attr attr)
{
	int ret;

	if (attr == DEV_DMA_NOT_SUPPORTED) {
		set_dma_ops(dev, &dma_dummy_ops);
		return 0;
	}

	ubct_dma_setup(dev);

	ret = ub_hybrid_iommu_configure(dev);
	if (ret == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	arch_setup_dma_ops(dev, attr == DEV_DMA_COHERENT);

	return 0;
}

/* Translate from FW */
static enum dev_dma_attr ub_dma_attr_trans(u16 dma_attr)
{
	switch (dma_attr) {
	case 0:
		return DEV_DMA_NON_COHERENT;
	case 1:
		return DEV_DMA_COHERENT;
	default:
		return DEV_DMA_NOT_SUPPORTED;
	}
}

static int ub_dma_configure(struct device *dev)
{
	struct ub_driver *udrv = to_ub_driver(dev->driver);
	struct ub_entity *uent = to_ub_entity(dev);
	struct ub_bus_controller *ubc;
	int ret;

	ubc = ub_ubc_get(uent->ubc);

	ret = ub_hybrid_dma_configure(dev,
				      ub_dma_attr_trans(ubc->attr.dma_cca));
	if (!ret && !udrv->driver_managed_dma) {
		ret = iommu_device_use_default_domain(dev);
		if (ret)
			arch_teardown_dma_ops(dev);
	}

	ub_ubc_put(ubc);
	dev_info(dev, "dma_configure ret = %d\n", ret);
	return ret;
}

static void ub_dma_cleanup(struct device *dev)
{
	struct ub_driver *driver = to_ub_driver(dev->driver);

	if (!driver->driver_managed_dma)
		iommu_device_unuse_default_domain(dev);

	dev_dbg(dev, "dma_cleanup\n");
}

struct bus_type ub_bus_type = {
	.name = "ub",
	.dma_configure = ub_dma_configure,
	.dma_cleanup = ub_dma_cleanup,
};
EXPORT_SYMBOL_GPL(ub_bus_type);

struct ub_dynid {
	struct list_head node;
	struct ub_device_id id;
};

static void ub_free_dynids(struct ub_driver *drv)
{
	struct ub_dynid *dynid, *n;

	spin_lock(&drv->dynids.lock);
	list_for_each_entry_safe(dynid, n, &drv->dynids.list, node) {
		list_del(&dynid->node);
		kfree(dynid);
	}
	spin_unlock(&drv->dynids.lock);
}

int __ub_register_driver(struct ub_driver *drv, struct module *owner,
			 const char *mod_name)
{
	if (!drv)
		return -EINVAL;

	drv->driver.name = drv->name;
	drv->driver.bus = &ub_bus_type;
	drv->driver.owner = owner;
	drv->driver.mod_name = mod_name;
	drv->driver.groups = drv->groups;

	spin_lock_init(&drv->dynids.lock);
	INIT_LIST_HEAD(&drv->dynids.list);

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL_GPL(__ub_register_driver);

void ub_unregister_driver(struct ub_driver *drv)
{
	if (!drv)
		return;

	driver_unregister(&drv->driver);
	ub_free_dynids(drv);
}
EXPORT_SYMBOL_GPL(ub_unregister_driver);

static int __init ub_driver_init(void)
{
	return bus_register(&ub_bus_type);
}
postcore_initcall(ub_driver_init);
