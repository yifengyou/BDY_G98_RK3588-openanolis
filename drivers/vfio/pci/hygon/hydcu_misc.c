// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#include "hydcu.h"
#define mmMP0_C2PMSG_65_alt_1 0x3810a04
#define mmROM_UPDATE_DENY_STAT 0x154
#define mmVBIOS_STATUS 0x381005c

int hycu_deny_updating_vbios(struct hydcu_device *hdev)
{
	WREG32_PCIE(mmMP0_C2PMSG_65_alt_1, 0xc0);
	mdelay(10);
	if (RREG32_PCIE(mmROM_UPDATE_DENY_STAT) & 0x1) {
		dev_info(&hdev->pdev->dev, "Disabled vbios updating path\n");
	} else {
		if (RREG32_PCIE(mmVBIOS_STATUS) != 0x200) {
			WREG32_PCIE(mmROM_UPDATE_DENY_STAT, 0x1);
			dev_info(&hdev->pdev->dev, "Link not ready. deny directly");
		} else {
			dev_info(&hdev->pdev->dev, "Failed to disable vbios updating path. The version of vbios should be 829040 or later");
		}
	}
	return 0;
}
