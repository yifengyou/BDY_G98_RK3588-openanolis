// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#ifdef dev_fmt
#undef dev_fmt
#endif
#define pr_fmt(fmt) "extra.da_mgr: " fmt
#define dev_fmt pr_fmt
#include <linux/module.h>
#include <linux/mutex.h>
#include "hydcu.h"

#ifndef PCI_EXP_DPC_CTL_EN_FATAL
#define PCI_EXP_DPC_CTL_EN_FATAL 0x1
#endif

#define SYSTEM_ERROR_INTR_ON_MESG_MASK \
	(PCI_EXP_RTCTL_SECEE| \
	 PCI_EXP_RTCTL_SENFEE| \
	 PCI_EXP_RTCTL_SEFEE)

#define ROOT_PORT_INTR_ON_MESG_MASK \
	(PCI_ERR_ROOT_CMD_COR_EN| \
	 PCI_ERR_ROOT_CMD_NONFATAL_EN| \
	 PCI_ERR_ROOT_CMD_FATAL_EN)

#define PCI_EXP_DPC_CTL_EN_MASK 0x3
#define PCI_ERR_ROOT_CMD_EN_MASK 0x7

struct da_device {
	struct pci_dev *pdev;
	struct list_head device_node;

	int dpc_cap_pos;
	u16 dpc_ctl_reg;
	u16 dpc_ctl_reg_new;

	int aer_cap_pos;
	u32 aer_saved_ue_mask;
	u32 aer_saved_ce_mask;
	u32 aer_saved_ue_severity;

	u32 aer_root_cmd_saved;
	u16 rtctl_saved;
};

static DEFINE_MUTEX(pair_lock);

static int hydcu_link_is_active(struct pci_dev *pdev)
{
	u16 link_sta;
	u32 link_cap;

	pci_read_config_dword(pdev, pdev->pcie_cap + PCI_EXP_LNKCAP, &link_cap);
	pci_read_config_word(pdev, pdev->pcie_cap + PCI_EXP_LNKSTA, &link_sta);
	if (link_cap & PCI_EXP_LNKCAP_DLLLARC)
		return link_sta & PCI_EXP_LNKSTA_DLLLA;

	return link_sta & PCI_EXP_LNKSTA_NLW;
}

/**
 * aer_enable_rootport - enable Root Port's interrupts when receiving messages
 * @pdev: pointer to a Root Port data structure
 * @aer: aer cap pos
 *
 * Invoked when PCIe bus loads AER service driver.
 */
static void hydcu_aer_restore_rootport(struct da_device *device)
{
	struct pci_dev *pdev;
	u16 reg16;
	u32 reg32;
	int aer;

	pdev = device->pdev;
	aer = device->aer_cap_pos;

	/* Clear PCIe Capability's Device Status */
	pcie_capability_read_word(pdev, PCI_EXP_DEVSTA, &reg16);
	pcie_capability_write_word(pdev, PCI_EXP_DEVSTA, reg16);

	pcie_capability_write_word(pdev, PCI_EXP_RTCTL, device->rtctl_saved);

	/* Clear error status */
	pci_read_config_dword(pdev, aer + PCI_ERR_ROOT_STATUS, &reg32);
	pci_write_config_dword(pdev, aer + PCI_ERR_ROOT_STATUS, reg32);
	pci_read_config_dword(pdev, aer + PCI_ERR_COR_STATUS, &reg32);
	pci_write_config_dword(pdev, aer + PCI_ERR_COR_STATUS, reg32);
	pci_read_config_dword(pdev, aer + PCI_ERR_UNCOR_STATUS, &reg32);
	pci_write_config_dword(pdev, aer + PCI_ERR_UNCOR_STATUS, reg32);

	pci_write_config_dword(pdev, aer + PCI_ERR_ROOT_COMMAND, device->aer_root_cmd_saved);
}

/**
 * aer_disable_rootport - disable Root Port's interrupts when receiving messages
 * @pdev: pointer to a Root Port data structure
 * @aer: aer cap pos
 *
 * Invoked when PCIe bus unloads AER service driver.
 */
static void hydcu_aer_save_disable_rootport(struct da_device *device)
{
	struct pci_dev *pdev;
	int aer;
	u32 reg32;

	pdev = device->pdev;
	aer = device->aer_cap_pos;

	/* Disable Root's interrupt in response to error messages */
	pci_read_config_dword(pdev, aer + PCI_ERR_ROOT_COMMAND, &reg32);
	device->aer_root_cmd_saved = reg32;
	reg32 &= ~ROOT_PORT_INTR_ON_MESG_MASK;
	pci_write_config_dword(pdev, aer + PCI_ERR_ROOT_COMMAND, reg32);

	pcie_capability_read_word(pdev, PCI_EXP_RTCTL, &device->rtctl_saved);

	/* Clear Root's error status reg */
	pci_read_config_dword(pdev, aer + PCI_ERR_ROOT_STATUS, &reg32);
	pci_write_config_dword(pdev, aer + PCI_ERR_ROOT_STATUS, reg32);
}


static struct da_device *_hydcu_find_device(struct pci_dev *pdev, struct list_head *head)
{
	struct da_device *pos;
	struct da_device *ret_device = NULL;

	list_for_each_entry(pos, head, device_node) {
		if (pos->pdev == pdev) {
			ret_device = pos;
			break;
		}
	}

	return ret_device;
}

/**
 * disable/restore dpc and irq
 */
static inline void hydcu_ds_dpc_disable(struct da_device *device, int en)
{
	struct pci_dev *pdev = device->pdev;
	u16 ctl = 0;

	pci_read_config_word(pdev, device->dpc_cap_pos + PCI_EXP_DPC_CTL, &ctl);

	if (en) {
		device->dpc_ctl_reg_new = ctl;
		ctl &= ~(PCI_EXP_DPC_CTL_EN_MASK | PCI_EXP_DPC_CTL_INT_EN);
	} else {
		/* restore old value */
		ctl = device->dpc_ctl_reg_new;
	}
	pci_write_config_word(pdev, device->dpc_cap_pos + PCI_EXP_DPC_CTL, ctl);
}

static struct da_device *hydcu_device_create_and_add(struct pci_dev *pdev, struct list_head *head)
{
	struct da_device *device;

	device = kzalloc(sizeof(struct da_device), GFP_KERNEL);
	if (!device)
		return NULL;

	device->pdev = pci_dev_get(pdev);
	device->aer_cap_pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_ERR);
	device->dpc_cap_pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_DPC);
	INIT_LIST_HEAD(&device->device_node);
	list_add_tail(&device->device_node, head);

	return device;
}

static int hydcu_da_restore_and_enable_cb(struct pci_dev *pdev, void *arg)
{
	int pci_type, pos;
	struct da_device *device = _hydcu_find_device(pdev, arg);

	if (!device)
		return -ENODEV;

	pos = device->aer_cap_pos;
	if (pos) {
		pci_write_config_dword(pdev, pos + PCI_ERR_UNCOR_MASK, device->aer_saved_ue_mask);
		pci_write_config_dword(pdev, pos + PCI_ERR_COR_MASK, device->aer_saved_ce_mask);
		pci_write_config_dword(pdev, pos + PCI_ERR_UNCOR_SEVER, device->aer_saved_ue_severity);
	}
	pci_type = pci_pcie_type(pdev);
	if ((pci_type == PCI_EXP_TYPE_ROOT_PORT || pci_type == PCI_EXP_TYPE_DOWNSTREAM) && device->dpc_cap_pos)
		hydcu_ds_dpc_disable(device, 0);
	if (pci_type == PCI_EXP_TYPE_ROOT_PORT && pos)
		hydcu_aer_restore_rootport(device);

	pci_restore_state(pdev);

	list_del(&device->device_node);
	pci_dev_put(device->pdev);
	kfree(device);

	return 0;
}

static int hydcu_da_save_and_disable_cb(struct pci_dev *pdev, void *arg)
{
	int pci_type, pos;
	uint32_t val;
	struct da_device *device = _hydcu_find_device(pdev, arg);

	if (device)
		return 0;

	if (!hydcu_link_is_active(pdev))
		return -ENOTCONN;

	device = hydcu_device_create_and_add(pdev, arg);
	if (!device)
		return -ENOMEM;

	pos = device->aer_cap_pos;
	if (pos) {
		/* clear */
		pci_read_config_dword(pdev, pos + PCI_ERR_COR_STATUS, &val);
		pci_write_config_dword(pdev, pos + PCI_ERR_COR_STATUS, val);
		pci_read_config_dword(pdev, pos + PCI_ERR_UNCOR_STATUS, &val);
		pci_write_config_dword(pdev, pos + PCI_ERR_UNCOR_STATUS, val);

		/* save */
		pci_read_config_dword(pdev, pos + PCI_ERR_UNCOR_MASK, &device->aer_saved_ue_mask);
		pci_read_config_dword(pdev, pos + PCI_ERR_COR_MASK, &device->aer_saved_ce_mask);
		pci_read_config_dword(pdev, pos + PCI_ERR_UNCOR_SEVER, &device->aer_saved_ue_severity);
	}
	pci_save_state(pdev);

	/* disable */
	if (pos) {
#define AER_MASK_ALL (~0u)
		pci_write_config_dword(pdev, pos + PCI_ERR_UNCOR_MASK, AER_MASK_ALL);
		pci_write_config_dword(pdev, pos + PCI_ERR_COR_MASK, AER_MASK_ALL);
		pci_write_config_dword(pdev, pos + PCI_ERR_UNCOR_SEVER, 0);
	}

	pci_type = pci_pcie_type(pdev);
	if ((pci_type == PCI_EXP_TYPE_ROOT_PORT || pci_type == PCI_EXP_TYPE_DOWNSTREAM) && device->dpc_cap_pos)
		hydcu_ds_dpc_disable(device, 1);

	if (pci_type == PCI_EXP_TYPE_ROOT_PORT && pos)
		hydcu_aer_save_disable_rootport(device);

	return 0;
}

int hydcu_scan_to_root_port(struct pci_dev *pdev,
		int (*cb)(struct pci_dev *, void *), void *args)
{
	struct pci_dev *updev = pdev;
	int ret = 0;

	while (updev) {
		ret = cb(updev, args);
		if (ret)
			break;
		updev = pci_upstream_bridge(updev);
	}

	return ret;
}

int hydcu_da_save_and_disable_to_rp(struct pci_dev *pdev, struct list_head *head)
	__acquires(&pair_lock)
{
	INIT_LIST_HEAD(head);
	mutex_lock(&pair_lock);
	return hydcu_scan_to_root_port(pdev, hydcu_da_save_and_disable_cb, (void *)head);
}

int hydcu_da_restore_and_enable_to_rp(struct pci_dev *pdev, struct list_head *head)
	__releases(&pair_lock)
{
	int ret = hydcu_scan_to_root_port(pdev, hydcu_da_restore_and_enable_cb, (void *)head);

	mutex_unlock(&pair_lock);
	return ret;
}
