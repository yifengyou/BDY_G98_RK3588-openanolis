// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#include <linux/device.h>
#include <linux/pci.h>

#include "hydcu.h"

/**
 * DOC: cpu_phy_para_index (uint)
 * set cpu phy para index sysfs, in hydcu default is 0
 */
uint cpu_phy_para_index;
MODULE_PARM_DESC(phy_para_index, "Cpu phy para index (0 = default , 1 or 2 is valid)");
module_param_named(phy_para_index, cpu_phy_para_index, uint, 0444);

int parm_deny_vbios_update;
MODULE_PARM_DESC(deny_vbios_update, "0, allow updating vbios. 1, deny updating vbios. default 0");
module_param_named(deny_vbios_update, parm_deny_vbios_update, int, 0444);

void hydcu_fixup_pci_header(struct pci_dev *pdev)
{
	dev_info(&pdev->dev, "add flags NO_BUS_RESET\n");
	pdev->dev_flags |= PCI_DEV_FLAGS_NO_BUS_RESET;
	pdev->dev_flags |= PCI_DEV_FLAGS_NO_FLR_RESET;

	pci_set_power_state(pdev, PCI_D0);
	pdev->pm_cap = 0;

   /* can remove below if build fail */
	pdev->reset_methods[0] = 0;

	hydcu_feature_start(pdev);

	dev_info(&pdev->dev, "enable pcie atomic\n");
	if (pci_enable_atomic_ops_to_root(pdev,
					  PCI_EXP_DEVCAP2_ATOMIC_COMP32 |
					  PCI_EXP_DEVCAP2_ATOMIC_COMP64)) {
		dev_err(&pdev->dev, "failed to enable atomic!");
	}
}
