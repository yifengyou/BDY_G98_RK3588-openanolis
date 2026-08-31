// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include "hydcu.h"

static LIST_HEAD(g_head);
static DEFINE_MUTEX(g_lock);

static struct hydcu_device *hydcu_find(struct pci_dev *pdev)
{
	struct hydcu_device *pos, *n;

	mutex_lock(&g_lock);
	list_for_each_entry_safe(pos, n, &g_head, node) {
		if (pos->pdev == pdev) {
			mutex_unlock(&g_lock);
			return pos;
		}
	}
	mutex_unlock(&g_lock);

	return NULL;
}


struct hydcu_device *hydcu_device_init(struct pci_dev *pdev)
{
	struct hydcu_device *hdev = hydcu_find(pdev);

	if (hdev)
		return hdev;

	hdev = kmalloc(sizeof(struct hydcu_device), GFP_KERNEL);
	if (!hdev)
		return NULL;

	hdev->pdev = pci_dev_get(pdev);
	if (pci_enable_device(pdev)) {
		dev_info(&pdev->dev, "enable device fail\n");
		goto err_pci;
	}
	hdev->rmmio = pci_iomap(pdev, 5, 0);
	if (!hdev->rmmio) {
		dev_info(&pdev->dev, "map mmio fail\n");
		goto err_iomap;
	}

	mutex_lock(&g_lock);
	list_add(&hdev->node, &g_head);
	mutex_unlock(&g_lock);

	return hdev;

err_iomap:
	pci_disable_device(pdev);
err_pci:
	pci_dev_put(hdev->pdev);
	kfree(hdev);

	return NULL;
}

static void hydcu_device_fini(struct hydcu_device *hdev)
{
	if (hdev) {
		list_del(&hdev->node);
		pci_iounmap(hdev->pdev, hdev->rmmio);
		pci_disable_device(hdev->pdev);
		pci_dev_put(hdev->pdev);
		kfree(hdev);
	}
}

/*
 * just clean the alloced buffer
 */
void hydcu_feature_end(struct pci_dev *pdev)
{
	struct hydcu_device *pos, *n;

	mutex_lock(&g_lock);
	list_for_each_entry_safe(pos, n, &g_head, node) {
		if (pos->pdev == pdev)
			hydcu_device_fini(pos);
	}
	mutex_unlock(&g_lock);
}

int hycu_broadcast_to_devices(hdev_callback_t func, void *data)
{
	struct hydcu_device *hdev, *n;
	int ret = 0;

	mutex_lock(&g_lock);
	list_for_each_entry_safe(hdev, n, &g_head, node) {
		ret = func(hdev, data);
		if (ret) {
			dev_info(&hdev->pdev->dev, "call back fail. ret=%d\n", ret);
			break;
		}
	}
	mutex_unlock(&g_lock);

	return ret;
}


int hydcu_feature_start(struct pci_dev *pdev)
{
	struct hydcu_device *hdev = hydcu_device_init(pdev);

	if (!hdev)
		return -ENOMEM;

	hydcu_pci_speed_action(hdev);
	if (parm_deny_vbios_update)
		hycu_deny_updating_vbios(hdev);

	hydcu_feature_end(pdev);
	return 0;
}
