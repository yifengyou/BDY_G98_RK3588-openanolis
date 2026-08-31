/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#ifndef __HYDCU_H__
#define __HYDCU_H__

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/list.h>

#define PCI_VENDOR_ID_HYGON 0x1d94

#define DEVICE_Z100SM		0x51b7
#define DEVICE_C878182		0x52b7
#define DEVICE_C878186		0x53b7
#define DEVICE_Z100			0x54b7
#define DEVICE_Z100L		0x55b7
#define DEVICE_C878181		0x56b7
#define DEVICE_C878185		0x57b7
#define DEVICE_C878188		0x58b7
#define DEVICE_C878174		0x59b7
#define DEVICE_KONGMING 0x61b7
#define DEVICE_KONGMING_SNAP 0x62b7
#define DEVICE_ZHONGDA 0x6210
#define DEVICE_ZHONGDA_ECO 0x6211
#define DEVICE_ZD_K100SM_AI 0x6214
#define DEVICE_BMZ      0x6310
#define DEVICE_BW3000   0x6320
#define DEVICE_NMZ_8388	0x6410
#define DEVICE_NMZ_8373	0x6420
#define DEVICE_NMZ_8378	0x6430
#define DEVICE_NMZ_8383	0x6440

#ifndef PCI_EXP_LNKCTL2_TLS_8_0GT
#define PCI_EXP_LNKCTL2_TLS_8_0GT 0x0003 /* Supported Speed 8GT/s */
#endif
#ifndef PCI_EXP_LNKCTL2_TLS_16_0GT
#define PCI_EXP_LNKCTL2_TLS_16_0GT 0x0004 /* Supported Speed 16GT/s */
#endif
#ifndef PCI_EXP_LNKCTL2_TLS_32_0GT
#define PCI_EXP_LNKCTL2_TLS_32_0GT 0x0005 /* Supported Speed 32GT/s */
#endif

#ifndef PCI_EXP_LNKSTA_CLS_8_0GB
#define PCI_EXP_LNKSTA_CLS_8_0GB 0x0003 /* Current Link Speed 8.0GT/s */
#endif
#ifndef PCI_EXP_LNKSTA_CLS_16_0GB
#define PCI_EXP_LNKSTA_CLS_16_0GB 0x0004 /* Current Link Speed 16.0GT/s */
#endif
#ifndef PCI_EXP_LNKSTA_CLS_32_0GB
#define PCI_EXP_LNKSTA_CLS_32_0GB 0x0005 /* Current Link Speed 32.0GT/s */
#endif

#ifndef PCI_EXP_LNKCAP_SLS_16_0GB
#define PCI_EXP_LNKCAP_SLS_16_0GB 0x00000004 /* LNKCAP2 SLS Vector bit 3 */
#endif
#ifndef PCI_EXP_LNKCAP_SLS_32_0GB
#define PCI_EXP_LNKCAP_SLS_32_0GB 0x00000005 /* LNKCAP2 SLS Vector bit 4 */
#endif

#ifndef PCI_EXP_LNKCTL2_TLS
#define PCI_EXP_LNKCTL2_TLS        0x000f
#endif
struct hydcu_device {
	struct pci_dev	*pdev;
	void __iomem	*rmmio;
	struct list_head node;
};

int hydcu_pci_speed_action(struct hydcu_device *hdev);

int hydcu_da_save_and_disable_to_rp(struct pci_dev *pdev, struct list_head *head);
int hydcu_da_restore_and_enable_to_rp(struct pci_dev *pdev, struct list_head *head);

#define R_INDEX		0x38
#define R_DATA		0x3C

#define WREG32_PCIE(reg, val)	\
	do {	\
		writel((reg), hdev->rmmio + R_INDEX);	\
		readl(hdev->rmmio + R_INDEX);		\
		writel((val), hdev->rmmio + R_DATA);	\
		readl(hdev->rmmio + R_DATA);	\
	} while (0)

#define RREG32_PCIE(reg) ({	\
	writel((reg), hdev->rmmio + R_INDEX);	\
	readl(hdev->rmmio + R_INDEX);	\
	readl(hdev->rmmio + R_DATA);	\
	})

int hycu_deny_updating_vbios(struct hydcu_device *hdev);

int hydcu_feature_start(struct pci_dev *pdev);
void hydcu_feature_end(struct pci_dev *pdev);

void hydcu_fixup_pci_header(struct pci_dev *pdev);

extern int parm_deny_vbios_update;
struct pci_dev *hydcu_find_outside_device(struct pci_dev *pdev);

typedef int (*hdev_callback_t)(struct hydcu_device *hdev, void *data);
int hycu_broadcast_to_devices(hdev_callback_t func, void *data);

#define mmBIOS_SCRATCH_5 0x144
#define mmBIOS_SCRATCH_6 0x148
#define mmSOCKET_ID  0x5a08c

#endif /* __HYDCU_H__ */
