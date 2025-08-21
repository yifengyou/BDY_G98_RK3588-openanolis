// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright(c) 2025 HiSilicon Technologies CO., All rights reserved.
 * Description: Token-ID Device API.
 */

#define pr_fmt(fmt) "[UMMU_CORE][TDEV]: " fmt

#include <linux/dma-map-ops.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <ub/ubus/ubus.h>

#include "ummu_core_priv.h"

static void ummu_root_release(struct device *dev)
{
	pr_info("ummu_tid_root release.\n");
}

struct device tid_bus = {
	.init_name = "ummu_tid_root",
	.parent = &platform_bus,
	.release = ummu_root_release,
};

static void ummu_release_dev(struct device *dev)
{
	struct tid_dev *tdev;

	tdev = to_tid_dev(dev);
	kfree(tdev->pdev.name);
	kfree(tdev);
}

/* tdev device sysfs */
static int attr_get_tid(struct device *dev, u32 *tid)
{
	struct tid_dev *tdev;

	tdev = to_tid_dev(dev);

	return ummu_get_tid(dev, tdev->sva, tid);
}

static ssize_t tid_val_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	u32 tid;
	int ret;

	ret = attr_get_tid(dev, &tid);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%u\n", tid);
}
static DEVICE_ATTR_RO(tid_val);

static ssize_t tid_mode_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct ummu_core_device *ummu_core;
	u32 tid;
	int ret;

	ummu_core = to_ummu_core(dev->iommu->iommu_dev);
	ret = attr_get_tid(dev, &tid);
	if (ret)
		return ret;

	ret = ummu_core_get_mapt_mode(ummu_core, tid);

	return sysfs_emit(buf, "%d\n", ret);
}
static DEVICE_ATTR_RO(tid_mode);

static struct attribute *ummu_vdev_attrs[] = {
	&dev_attr_tid_val.attr,
	&dev_attr_tid_mode.attr,
	NULL,
};

static struct attribute_group ummu_vdev_group = {
	.name = "ummu-vdev-attr",
	.attrs = ummu_vdev_attrs,
};

static const struct attribute_group *ummu_vdev_groups[] = {
	&ummu_vdev_group,
	NULL,
};

static int alloc_software_node(struct device *dev, u32 *ptid,
				const struct software_node *parent)
{
	struct property_entry props[3];

	memset(props, 0, sizeof(props));
	props[0] = PROPERTY_ENTRY_U32("assign-pasid", *ptid);
	props[1] = PROPERTY_ENTRY_U32("pasid-num-bits", UB_MAX_TID_BITS);
	return device_create_managed_software_node(dev, props, parent);
}

/* tdev device creation */
static int init_tdev(struct tid_dev *tdev, const char *device_name, u32 *ptid,
					struct ummu_core_device *ummu_core)
{
	int ret;

	if (!tdev->pdev.dev.parent)
		tdev->pdev.dev.parent = ummu_core->ummu_core_root;

	tdev->pdev.name = kstrdup(device_name ? device_name : "ummu_vdev", GFP_KERNEL);
	if (!tdev->pdev.name)
		return -EINVAL;

	ret = alloc_software_node(&tdev->pdev.dev, ptid, NULL);
	if (ret) {
		pr_err("tdev create software node ERR!:%d\n", ret);
		kfree(tdev->pdev.name);
		return ret;
	}

	tdev->pdev.id = PLATFORM_DEVID_AUTO;
	tdev->pdev.dev.release = ummu_release_dev;
	tdev->pdev.dev.coherent_dma_mask = DMA_BIT_MASK(48); // 48 DMA addr size
	tdev->pdev.dev.dma_mask = &(tdev->pdev.dev.coherent_dma_mask);
	tdev->pdev.dev.groups = ummu_vdev_groups;
	tdev->pdev.dma_parms.max_segment_size = U32_MAX;
	set_dev_node(&tdev->pdev.dev, NUMA_NO_NODE);

	ret = platform_device_register(&tdev->pdev);
	if (ret) {
		platform_device_put(&tdev->pdev);
		return ret;
	}

	ret = sysfs_create_link_nowarn(&tdev->pdev.dev.kobj,
					&ummu_core->iommu.dev->kobj,
					dev_name(ummu_core->iommu.dev));
	if (ret) {
		platform_device_unregister(&tdev->pdev);
		pr_err("tdev link to iommmu dev ERR!:%d\n", ret);
		return ret;
	}

	return 0;
}

static struct iommu_device *select_ummu_device(struct tdev_attr *attr)
{
	struct ummu_core_device *entry;

	mutex_lock(&global_device_lock);
	list_for_each_entry(entry, &core_device_list, list) {
		if (!entry->ops || !entry->ops->tdev_support_attr)
			continue;
		if (entry->ops->tdev_support_attr(entry, attr)) {
			mutex_unlock(&global_device_lock);
			return &entry->iommu;
		}
	}
	mutex_unlock(&global_device_lock);

	pr_err("ummu device not found.\n");
	return NULL;
}

static struct iommu_device *get_iommu_dev(struct tdev_attr *attr)
{
	struct iommu_device *dev;

	dev = select_ummu_device(attr);
	if (!dev) {
		pr_err("get ummu device failed.\n");
		return NULL;
	}

	return dev;
}

static void set_dma_configure(struct device *dev, enum dev_dma_attr attr)
{
	if (attr == DEV_DMA_NOT_SUPPORTED)
		return;

	setup_tdev_dma_ops(dev, attr == DEV_DMA_COHERENT);
}

extern void iommu_fwspec_free(struct device *dev);
struct device *ummu_alloc_tdev(struct tdev_attr *attr, u32 *ptid)
{
	struct iommu_device *iommu_dev;
	struct tid_dev *tdev;
	int ret;

	iommu_dev = get_iommu_dev(attr);
	if (!iommu_dev)
		return NULL;

	tdev = kzalloc(sizeof(*tdev), GFP_KERNEL);
	if (!tdev)
		return NULL;

	ret = iommu_fwspec_init(&tdev->pdev.dev, iommu_dev->fwnode);
	if (ret) {
		kfree(tdev);
		return NULL;
	}

	ret = init_tdev(tdev, attr->name, ptid, to_ummu_core(iommu_dev));
	if (ret) {
		iommu_fwspec_free(&tdev->pdev.dev);
		kfree(tdev);
		return NULL;
	}

	set_dma_configure(&tdev->pdev.dev, attr->dma_attr);

	return &tdev->pdev.dev;
}

struct device *ummu_core_alloc_tdev(struct tdev_attr *attr, u32 *ptid)
{
	u32 temp_tid = UMMU_INVALID_TID;
	struct device *dev;
	int ret;

	if (!ptid || !attr) {
		pr_err("invalid param\n");
		return NULL;
	}

	if (*ptid != UMMU_INVALID_TID)
		temp_tid = *ptid;

	dev = ummu_alloc_tdev(attr, ptid);
	if (!dev)
		return NULL;

	ret = ummu_get_tid(dev, NULL, ptid);
	if (ret || (temp_tid != UMMU_INVALID_TID && temp_tid != *ptid)) {
		ummu_core_free_tdev(dev);
		return NULL;
	}

	return dev;
}
EXPORT_SYMBOL_GPL(ummu_core_alloc_tdev);

int ummu_core_free_tdev(struct device *dev)
{
	struct tid_dev *tdev = to_tid_dev(dev);

	platform_device_unregister(&tdev->pdev);
	return 0;
}
EXPORT_SYMBOL_GPL(ummu_core_free_tdev);

int tdev_init(void)
{
	return device_register(&tid_bus);
}

void tdev_exit(void)
{
	device_unregister(&tid_bus);
}
