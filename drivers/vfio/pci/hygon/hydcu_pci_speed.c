// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci.h>
#include "hydcu.h"
#include "hydcu_pci_speed.h"

#ifndef X86_VENDOR_HYGON
#define X86_VENDOR_HYGON 9
#endif

#define MAX_DEPTH 10
struct hydcu_dev_list {
	struct pci_dev *plist[MAX_DEPTH];
};

#ifdef CONFIG_X86
static PHYINFO phyinfo[] = {
	//index  numlanes start_lane
	{0, 2, 0},
	{1, 2, 2},
	{2, 4, 4},
	{3, 4, 8},
	{4, 4, 12},
};

static PHYPARA phypara[] = {
	//https://conf.hygon.cn/pages/viewpage.action?pageId=152344779
	//reg_offset g3_sl1[0]   g3_sl1[1]   g3_sl1[2]   g3_sl1r2    g3_5
	{0x10fa, {0x00a600ac, 0x00a600ac, 0x00b600ac, 0x00a200ac, 0x0}},
	{0x10c0, {0x00000018, 0x00000018, 0x00000018, 0x00000018, 0x0}},
	{0x10fc, {0x0007007d, 0x0003007d, 0x0007007d, 0x000700ff, 0x0}},
	{0x10f4, {0x00480000, 0x00480000, 0x00480000, 0x00480000, 0x0}},
	{0x10f2, {0x0000000d, 0x0000000d, 0x00000055, 0x0000001d, 0x0}},
	{0x104c, {0x2cd900ce, 0x2cd900ce, 0x2cd900ce, 0x2cd900ce, 0x2cd900ce}},
};

#endif

/*
 * @brief   get link LTSSM
 *
 * @return No return
 */
static int get_current_lcstate(struct hydcu_device *hdev)
{
	uint32_t addr, val;

	addr = smnPCIE_LC_STATE0;
	val = RREG32_PCIE(addr);
	return val & 0xff;
}

static struct hydcu_dev_list get_dev_list(struct hydcu_device *hdev)
{
	struct hydcu_dev_list dev_list;
	struct pci_dev *next, *bridge;
	struct pci_dev *pdev = hdev->pdev;
	int i;

	for (i = 0; i < MAX_DEPTH; i++)
		dev_list.plist[i] = NULL;

	bridge = pdev;
	for (i = 0; i < MAX_DEPTH; i++) {
		dev_list.plist[i] = bridge;
		next = bridge->bus ? bridge->bus->self : NULL;
		if (next)
			bridge = next;
		else
			break;
	}

	if (i < 3)
		dev_warn(&pdev->dev,
			 "Topology is unknown,please check your devices!\n");

	return dev_list;
}

static void release_target_link_speed_override_en(struct hydcu_device *hdev)
{
	uint32_t addr;

	PCIE_LC_SPEED_CNTL_T reg;
	// disable override en
	addr = smnPCIE_LC_SPEED_CNTL;
	reg.data = RREG32_PCIE(addr);
	reg.LC_TARGET_LINK_SPEED_OVERRIDE_EN = 0;
	WREG32_PCIE(addr, reg.data);
	udelay(10);
	reg.data = RREG32_PCIE(addr);
	if (reg.LC_TARGET_LINK_SPEED_OVERRIDE_EN != 0)
		dev_info(&hdev->pdev->dev,
			 "release target link speed override en fail!\n");
}

static void set_tx_preset(struct hydcu_device *hdev)
{
	uint32_t addr;
	PCIE_LC_CNTL8_T reg;

	addr = smnPCIE_LC_CNTL8;
	reg.data = RREG32_PCIE(addr);
	//reg.LC_FORCE_PRESET_IN_EQ_REQ_PHASE_16GT=0x1;
	//force P4
	reg.LC_FORCE_PRESET_VALUE_16GT = 0x4;
	WREG32_PCIE(addr, reg.data);
	udelay(10);
	reg.data = RREG32_PCIE(addr);
	if (reg.LC_FORCE_PRESET_VALUE_16GT != 0x4)
		dev_info(&hdev->pdev->dev, "set tx preset fail!\n");
}

static int
rc_change_linkspeed(struct hydcu_device *hdev, struct pci_dev *rc_dev,
		    int speed)
{
	struct pci_dev *rc;
	int timeout = 10000;
	int rc_pcie_cap_pos, rc_ext_pos;
	u16 rc_val;
	u16 rc_current_speed;
	u16 target_link_speed, current_link_speed_check;
	uint32_t curLcstate;

	rc = rc_dev;
	rc_pcie_cap_pos = pci_find_capability(rc, PCI_CAP_ID_EXP);
	if (!rc_pcie_cap_pos) {
		dev_err(&rc->dev, "not support pcie cap\n");
		return -1;
	}
	switch (speed) {
	case 3:
		target_link_speed = PCI_EXP_LNKCTL2_TLS_8_0GT;
		current_link_speed_check = PCI_EXP_LNKSTA_CLS_8_0GB;
		break;
	case 4:
		target_link_speed = PCI_EXP_LNKCTL2_TLS_16_0GT;
		current_link_speed_check = PCI_EXP_LNKSTA_CLS_16_0GB;
		break;
	default:
		dev_err(&rc->dev, "speed value is not support\n");
		return -1;
	}

	dev_dbg(&rc->dev, "change speed to gen%d\n", speed);
	pci_read_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKCTL2, &rc_val);
	rc_val &= ~(PCI_EXP_LNKCTL2_TLS);
	rc_val |= target_link_speed;
	pci_write_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKCTL2, rc_val);
	do {
		pci_read_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKSTA,
				     &rc_val);
		timeout--;
		udelay(10);
	} while (rc_val & PCI_EXP_LNKSTA_LT && timeout > 0);

	pci_read_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKCTL, &rc_val);
	rc_val &= ~(PCI_EXP_LNKCTL_RL);
	rc_val |= (PCI_EXP_LNKCTL_RL);
	pci_write_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKCTL, rc_val);
	timeout = 10000;
	do {
		pci_read_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKSTA,
				     &rc_val);
		timeout--;
		udelay(10);
	} while (((rc_val & PCI_EXP_LNKSTA_LT) ||
				!(rc_val & PCI_EXP_LNKSTA_DLLLA)) && timeout > 0);

	timeout = 5;		//wait 100ms;
	do {
		msleep(20);
		pci_read_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKSTA,
				     &rc_val);
		rc_current_speed = rc_val & PCI_EXP_LNKSTA_CLS;
		curLcstate = get_current_lcstate(hdev);
		timeout--;
	} while ((rc_current_speed != current_link_speed_check
		|| curLcstate != 0x10) && timeout > 0);

	if (rc_current_speed == current_link_speed_check && curLcstate == 0x10) {
		dev_info(&rc->dev, "gen%d success\n", speed);
		return 0;
	}
	rc_ext_pos = pci_find_ext_capability(rc, PCI_EXT_CAP_ID_SECPCI);
	if (!rc_ext_pos) {
		dev_err(&rc->dev, "not support pcie ext cap\n");
		return -1;
	}
	pci_read_config_word(rc, rc_ext_pos + PCI_EXT_LNKCTL3, &rc_val);
	if ((rc_val & PCI_EXT_LNKCTL3_PE) != 0) {
		rc_val &= ~(PCI_EXT_LNKCTL3_PE);
		pci_write_config_word(rc, rc_ext_pos + PCI_EXT_LNKCTL3,
				      rc_val);
	}
	dev_dbg(&rc->dev, "change link speed to gen%d fail\n", speed);
	dev_warn(&rc->dev, "current_speed: %d,curLcstate: 0x%x\n",
		 rc_current_speed, curLcstate);

	return -1;
}

#ifdef CONFIG_X86
static void get_cpu_info(int *vendor, int *model, int *stepping, int *pkg)
{
	unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
	unsigned int op = CPUID_FAMMODSTEPEXT_INDEX;
	//get vendor_id
	cpuid(0x00000000, &eax, &ebx, &ecx, &edx);
	if (CPUID_IS(CPUID_INTEL1, CPUID_INTEL2, CPUID_INTEL3, ebx, ecx, edx))
		*vendor = X86_VENDOR_INTEL;
	if (CPUID_IS(CPUID_AMD1, CPUID_AMD2, CPUID_AMD3, ebx, ecx, edx))
		*vendor = X86_VENDOR_AMD;
	if (CPUID_IS(CPUID_HYGON1, CPUID_HYGON2, CPUID_HYGON3, ebx, ecx, edx))
		*vendor = X86_VENDOR_HYGON;
	//get model/stepping
	cpuid(0x00000001, &eax, &ebx, &ecx, &edx);
	*model =
	    ((eax & CPUID_BASE_MODEL_MASK) >> CPUID_BASE_MODEL_RIGHT_OFFSET);
	*stepping =
	    ((eax & CPUID_BASE_STEPPING_MASK) >>
	     CPUID_BASE_STEPPING_RIGHT_OFFSET);
	if (*vendor == X86_VENDOR_HYGON) {
		//get pkg_id
		cpuid(op, &eax, &ebx, &ecx, &edx);
		*pkg =
		    ((ebx & CPUID_PKG_TYPE_MASK) >>
		     CPUID_PKG_TYPE_RIGHT_OFFSET);
	}
}

static uint32_t get_core_index(struct pci_dev *pdev)
{
	struct pci_dev *rc;
	uint32_t core_index;
	u8 devnum = 0;

	rc = pdev;
	devnum = PCI_SLOT(rc->devfn);
	core_index = devnum < CPU_PCIE_CORE_DEVNUM ? 0 : 1;
	return core_index;
}

static uint32_t rc_phy_reg_read(struct pci_dev *pdev, u32 addr)
{
	struct pci_dev *rc;
	uint32_t val = 0;

	rc = pdev;
	pci_bus_write_config_dword(rc->bus, PCI_DEVFN(0, 0), NB_SMN_INDEX,
				   addr);
	pci_bus_read_config_dword(rc->bus, PCI_DEVFN(0, 0), NB_SMN_DATA, &val);
	return val;
}

static void rc_phy_reg_write(struct pci_dev *pdev, u32 addr, u32 val)
{
	struct pci_dev *rc;

	rc = pdev;
	pci_bus_write_config_dword(rc->bus, PCI_DEVFN(0, 0), NB_SMN_INDEX,
				   addr);
	pci_bus_write_config_dword(rc->bus, PCI_DEVFN(0, 0), NB_SMN_DATA, val);
}

static uint32_t get_lane_mask(struct pci_dev *pdev, int pos, u32 index)
{
	struct pci_dev *rc;
	int rc_pcie_cap_pos;
	u32 core_index, port_index;
	u32 addr = 0, val = 0;
	u32 link_width = 0;
	u32 lane_offset = 0, lane_mask = 0;
	u32 i;

	rc = pdev;
	rc_pcie_cap_pos = pos;
	core_index = index;

	pci_read_config_dword(rc, rc_pcie_cap_pos + PCI_EXP_LNKCAP, &val);
	port_index = (val >> 24) & 0xff;
	link_width = (val >> 4) & 0x3f;
	dev_dbg(&rc->dev, "core_index:%x,port_index:%x,link_width=%d\n",
		core_index, port_index, link_width);
	//get lane_offset
	addr =
	    smnPCIE_LC_PORT_ORDER_BASE + (core_index * 0x100000) +
	    (port_index * 0x1000) + smnPCIE_LC_PORT_ORDER_REG_OFF;
	val = rc_phy_reg_read(rc, addr);
	lane_offset = val & 0xf;
	//get lane_mask
	for (i = 0; i < link_width; i++)
		lane_mask |= 1 << (lane_offset + i);
	dev_dbg(&rc->dev, "lane_mask=%x\n", lane_mask);
	return lane_mask;
}

static uint32_t get_val_index(int model, int pkg)
{
	uint32_t val_index = 0;

	switch (model) {
	case cpu_g3:
		switch (pkg) {
		case pkg_sl1:
			if (cpu_phy_para_index < 3)
				val_index =
				    cpu_phy_para_index ? cpu_phy_para_index : 0;
			else
				val_index = 0;
			break;
		case pkg_sl1r2:
			val_index = 3;
			break;
		case pkg_dm1:
			val_index = 0xff;
			break;
		}
		break;
	case cpu_g3_5:
		val_index = 4;
		break;
	}
	return val_index;
}

static uint32_t get_lane_position(uint32_t lane, uint32_t *phy_index,
		uint32_t *lane_index)
{
	uint32_t start_lane;
	uint32_t temp_phy_index = 0;

	start_lane = 0;
	for (temp_phy_index = 0;
	     temp_phy_index < (sizeof(phyinfo) / sizeof(phyinfo[0]));
	     temp_phy_index++) {
		if ((lane >= start_lane)
		    && (lane < (start_lane + phyinfo[temp_phy_index].numlanes))) {
			*phy_index = temp_phy_index;
			*lane_index = lane - start_lane;
			return 0;
		}
		start_lane += phyinfo[temp_phy_index].numlanes;
	}
	return -1;
}

static TASK_INFO *prepare_task_descriptor(struct hydcu_device *hdev,
					  uint32_t core_index,
					  uint32_t lane_mask)
{
	int ret = 0;
	TASK_INFO *task_list;
	TASK_INFO *task;
	uint32_t index;
	uint32_t lane;
	uint64_t lane_count;
	uint32_t phy_index;
	uint32_t lane_index;
	unsigned long mask = lane_mask;

	lane_count = hweight32(lane_mask);
	task_list = (TASK_INFO *) kzalloc(lane_count * sizeof(TASK_INFO), GFP_KERNEL);
	if (task_list == NULL) {
		dev_err(&hdev->pdev->dev, "devm_kzalloc failed!\n");
		return NULL;
	}
	dev_dbg(&hdev->pdev->dev, "Preparing... %lld task(s) in total\n",
		lane_count);
	index = 0;
	for (lane = 0; lane < sizeof(lane_mask) * 8; lane++) {
		if (test_bit(lane, &mask)) {
			ret = get_lane_position(lane, &phy_index, &lane_index);
			if (ret != 0)
				goto error;

			task = &task_list[index];
			task->core_index = core_index;
			task->phy_index = phy_index;
			task->lane_index = lane_index;
			index++;
		}
	}
	return task_list;

error:
	kfree(task_list);

	return NULL;
}

static int set_phy_parameters(struct pci_dev *pdev, TASK_INFO *task_list,
		   uint32_t lane_mask, uint32_t val_index, bool is_cpu_g3_5)
{
	int ret = 0;
	uint32_t task_index, task_count, core_index, phy_index, lane_index,
	    reg_offset, phypara_len;
	TASK_INFO *task;
	struct pci_dev *rc;
	uint32_t addr = 0, val_w = 0, i = 0;
	uint32_t val_r = 0xa5a5a5a5;

	rc = pdev;
	task_count = hweight32(lane_mask);
	for (task_index = 0; task_index < task_count; task_index++) {
		task = &task_list[task_index];
		core_index = task->core_index;
		phy_index = task->phy_index;
		lane_index = task->lane_index;
		phypara_len = sizeof(phypara) / sizeof(phypara[0]);
		for (i = 0; i < phypara_len; i++) {
			if (is_cpu_g3_5 && (i != (phypara_len - 1)))
				continue;
			reg_offset = phypara[i].reg_offset;
			val_w = phypara[i].val[val_index];
			addr =
			    smnPCIE_PHY_BASE + (core_index * 0x100000) +
			    (((phy_index << 16) + (lane_index << 8) +
			      reg_offset) << 1);
			rc_phy_reg_write(rc, addr, val_w);
			udelay(100);
			val_r = rc_phy_reg_read(rc, addr);
			if (val_w != val_r)
				dev_warn(&rc->dev,
					 "cpu phy parameter is unexpected! val_w:0x%x, val_r:0x%x\n",
					 val_w, val_r);
		}
	}

	return ret;
}
#endif

static void wait_link_stable(struct hydcu_device *hdev)
{
	int timeout = 20;
	uint32_t curLcstate;

	do {
		msleep(200);
		curLcstate = get_current_lcstate(hdev);
		timeout--;
	} while ((curLcstate != PCIE_LSTSSM_L0) && timeout > 0);
}

static void bypass_eq2(struct hydcu_device *hdev, bool enable)
{
	u32 data;

	data = RREG32_PCIE(smnPCIE_LC_CNTL8);
	if (enable) {
		// Bypass EQ2
		dev_info(&hdev->pdev->dev, "bypass eq2!\n");
		data |= 1 << PCIE_LC_CNTL8__LC_USC_EQ_NOT_REQD_16GT__SHIFT;
		data |= 1 << PCIE_LC_CNTL8__LC_BYPASS_EQ_REQ_PHASE_16GT__SHIFT;
	} else {
		// Open EQ2
		dev_info(&hdev->pdev->dev, "open eq2!\n");
		data &= ~(1 << PCIE_LC_CNTL8__LC_USC_EQ_NOT_REQD_16GT__SHIFT);
		data &=
		    ~(1 << PCIE_LC_CNTL8__LC_BYPASS_EQ_REQ_PHASE_16GT__SHIFT);
	}
	WREG32_PCIE(smnPCIE_LC_CNTL8, data);
}

static int check_eq_status(struct pci_dev *ups, int ups_ext)
{
	u32 data, mask;
	int ups_ext_pos;
	struct pci_dev *upstream;
	int ret = 0;

	upstream = ups;
	ups_ext_pos = ups_ext;
	//check eq status
	pci_read_config_dword(upstream, ups_ext_pos + PCI_EXT_16GT_STA, &data);
	if (data == 0xffffffff) {
		dev_warn(&upstream->dev, "link maybe lost,data=%x\n", data);
		return -1;
	}
	//check e3 status
	mask = data & PCI_EXT_16GT_STA_E3_MASK;
	if (mask != PCI_EXT_16GT_STA_E3_MASK) {
		dev_warn(&upstream->dev, "eq3 status: %x\n", data);
		ret = -1;
	} else {
		dev_info(&upstream->dev, "eq3 success! status: %x\n", data);
		ret = 0;
		return ret;
	}
	//check e2 e1 e0 status
	mask = data & PCI_EXT_16GT_STA_E2_E1_E0_MASK;
	if (mask != PCI_EXT_16GT_STA_E2_E1_E0_MASK) {
		dev_warn(&upstream->dev, "eq2 or eq1 or eq0 status: %x\n",
			 data);
		ret = -1;	// for more information, please contact DE(yanglijie)
	}
	return ret;
}

static void redo_eq(struct hydcu_device *hdev, struct pci_dev *ds,
		    struct pci_dev *ups, int ds_exp, int ups_exp)
{
	struct pci_dev *downstream_near, *upstream;
	int ds_nr_exp_pos, ups_exp_pos, ds_nr_ext_pos;
	int timeout = 10000;
	u32 data;
	u16 ds_nr_val;

	downstream_near = ds;
	upstream = ups;
	ds_nr_exp_pos = ds_exp;
	ups_exp_pos = ups_exp;
	ds_nr_ext_pos =
	    pci_find_ext_capability(downstream_near, PCI_EXT_CAP_ID_SECPCI);
	if (!ds_nr_ext_pos) {
		dev_err(&downstream_near->dev, "not support pcie ext cap\n");
		return;
	}
	udelay(1000);
	// 1、config ep PCIE_LC_CNTL9
	data = RREG32_PCIE(smnPCIE_LC_CNTL9);
	data |= 1 << PCIE_LC_CNTL9__LC_DSC_ACCEPT_16GT_EQ_REDO__SHIFT;
	WREG32_PCIE(smnPCIE_LC_CNTL9, data);
	// 2、set downstream_near config space target link speed gen4 (option)
	// 3、set downstream_near config space linkcontrol3 perform equalization
	ds_nr_ext_pos =
	    pci_find_ext_capability(downstream_near, PCI_EXT_CAP_ID_SECPCI);
	pci_read_config_dword(downstream_near, ds_nr_ext_pos + PCI_EXT_LNKCTL3,
			      &data);
	data &= ~(PCI_EXT_LNKCTL3_PE);
	data |= PCI_EXT_LNKCTL3_PE;
	pci_write_config_dword(downstream_near, ds_nr_ext_pos + PCI_EXT_LNKCTL3,
			       data);
	// 4、set downstream_near config space linkcontrol retrainlink
	pci_read_config_word(downstream_near, ds_nr_exp_pos + PCI_EXP_LNKCTL,
			     &ds_nr_val);
	ds_nr_val &= ~(PCI_EXP_LNKCTL_RL);
	ds_nr_val |= (PCI_EXP_LNKCTL_RL);
	pci_write_config_word(downstream_near, ds_nr_exp_pos + PCI_EXP_LNKCTL,
			      ds_nr_val);
	// 5、wait retraining complete
	do {
		pci_read_config_word(downstream_near,
				     ds_nr_exp_pos + PCI_EXP_LNKSTA,
				     &ds_nr_val);
		timeout--;
		udelay(10);
	} while (ds_nr_val & PCI_EXP_LNKSTA_LT && timeout > 0);
}

static int check_eq_status_gen3(struct pci_dev *ups, int ups_ext)
{
	u32 data, mask;
	int ups_ext_pos;
	struct pci_dev *upstream;
	int ret = 0;

	upstream = ups;
	ups_ext_pos = ups_ext;
	//check eq status
	pci_read_config_dword(upstream, ups_ext_pos + PCIE_LINK_STATUS2_REG_OFFSET, &data);
	if (data == 0xffffffff) {
		dev_warn(&upstream->dev, "link maybe lost,data=%x\n", data);
		return -1;
	}
	data = (data >> PCIE_LINK_STATUS2_REG_SHIFT) & PCIE_LINK_STATUS2_REG_MASK;
	//check e3 status
	mask = data & PCIE_LINK_STATUS2_8GT_STA_E3_MASK;
	if (mask != PCIE_LINK_STATUS2_8GT_STA_E3_MASK) {
		dev_warn(&upstream->dev, "eq3 status: %x\n", data);
		ret = -1;
	} else {
		dev_info(&upstream->dev, "eq3 success! status: %x\n", data);
		ret = 0;
		return ret;
	}
	//check e2 e1 e0 status
	mask = data & PCIE_LINK_STATUS2_8GT_STA_E2_E1_E0_MASK;
	if (mask != PCIE_LINK_STATUS2_8GT_STA_E2_E1_E0_MASK) {
		dev_warn(&upstream->dev, "eq2 or eq1 or eq0 status: %x\n", data);
		ret = -1; // for more information, please contact DE(yanglijie)
	}
	return ret;
}

/**
 * hydcu_check_change_speed - check and change speed to gen4
 *
 * @hdev: hydcu_device pointer
 *
 * if current link support gen4 speed,but current speed is not gen4,
 * then we will change speed to gen4.
 * Called at driver startup.
 */
static void hydcu_check_change_speed(struct hydcu_device *hdev)
{
	struct hydcu_dev_list dev_list;
	struct pci_dev *rc, *upstream;
	struct list_head head;
	int rc_pcie_cap_pos, upstream_pcie_cap_pos, upstream_ext_pos;
	u16 rc_val, ups_val, max_pci_gen, current_speed;
	u16 rc_current_speed, ups_current_speed;
	u32 rc_lnkcap_val, ups_lnkcap_val;
	u32 rc_max_speed, ups_max_speed;
	u32 count = 0, count_err = 0;
	int ret;
#ifdef CONFIG_X86
	u32 depth = 0, core_index = 0, val_index = 0;
	int vendor = 0, model = 0, stepping = 0, pkg = 0;
	u32 lane_mask = 0x0;
	TASK_INFO *task_list;
	bool is_cpu_g3_5 = false;
	bool is_phy_para_set = false;
#endif

	release_target_link_speed_override_en(hdev);

	dev_list = get_dev_list(hdev);
	if (dev_list.plist[3] == NULL)
		return;
#ifdef CONFIG_X86
	if (dev_list.plist[4] == NULL)
		depth = 4;
#endif
	rc = dev_list.plist[3];
	upstream = dev_list.plist[2];
	rc_pcie_cap_pos = pci_find_capability(rc, PCI_CAP_ID_EXP);
	if (!rc_pcie_cap_pos) {
		dev_err(&rc->dev, "not support pcie cap\n");
		return;
	}
	upstream_pcie_cap_pos = pci_find_capability(upstream, PCI_CAP_ID_EXP);
	if (!upstream_pcie_cap_pos) {
		dev_err(&upstream->dev, "not support pcie cap\n");
		return;
	}
	upstream_ext_pos =
	    pci_find_ext_capability(upstream, PCI_EXT_CAP_ID_PL_16GT);
	if (!upstream_ext_pos) {
		dev_err(&upstream->dev, "not support pcie ext cap\n");
		return;
	}

#ifdef CONFIG_X86
	if (depth == 4) {
		get_cpu_info(&vendor, &model, &stepping, &pkg);
		dev_info(&rc->dev, "vendor:%d,model:%d,stepping:%d,pkg:%d\n",
			 vendor, model, stepping, pkg);
	}
#endif
	pci_read_config_dword(rc, rc_pcie_cap_pos + PCI_EXP_LNKCAP,
			      &rc_lnkcap_val);
	pci_read_config_dword(upstream, upstream_pcie_cap_pos + PCI_EXP_LNKCAP,
			      &ups_lnkcap_val);
	rc_max_speed = rc_lnkcap_val & PCI_EXP_LNKCAP_SLS;
	ups_max_speed = ups_lnkcap_val & PCI_EXP_LNKCAP_SLS;
	max_pci_gen = rc_max_speed < ups_max_speed ? rc_max_speed : ups_max_speed;

	dev_info(&rc->dev, "Device support max pcie version = 0x%x\n", max_pci_gen);
	switch (max_pci_gen) {
	case PCI_EXP_LNKCAP_SLS_16_0GB:
		ret = hydcu_da_save_and_disable_to_rp(hdev->pdev, &head);
		if (ret) {
			dev_err(&rc->dev, "save and disable to root port failed: %d\n", ret);
			hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
			return;
		}
		dev_dbg(&rc->dev, "support gen4\n");
		dev_dbg(&upstream->dev, "support gen4\n");
#ifdef CONFIG_X86
		if (vendor == X86_VENDOR_INTEL) {
			//intel 4316/6354
			if (model == 106 && stepping == 6) {
				do {
					// open eq2
					if (count == 0)
						bypass_eq2(hdev, false);
					// Bypass eq2
					if (count == 10)
						bypass_eq2(hdev, true);
					redo_eq(hdev, rc, upstream,
						rc_pcie_cap_pos,
						upstream_pcie_cap_pos);
					wait_link_stable(hdev);
					ret = check_eq_status(upstream, upstream_ext_pos);
					count++;
				} while (ret < 0 && count < 11);

				ret = hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
				if (ret)
					dev_err(&rc->dev, "restore and enable to root port failed: %d\n", ret);
				return;
			}
		}
#endif
		pci_read_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKSTA,
				     &rc_val);
		pci_read_config_word(upstream,
				     upstream_pcie_cap_pos + PCI_EXP_LNKSTA,
				     &ups_val);
		rc_current_speed = rc_val & PCI_EXP_LNKSTA_CLS;
		ups_current_speed = ups_val & PCI_EXP_LNKSTA_CLS;
		if (rc_current_speed != PCI_EXP_LNKSTA_CLS_16_0GB) {
			if (rc_current_speed < PCI_EXP_LNKSTA_CLS_8_0GB) {
				do {
					//change to gen3
					ret = rc_change_linkspeed(hdev, rc, 3);
					count++;
				} while (ret < 0 && count < 3);
			}

			count = 0;

			do {
				//set dcu tx preset
				if (count == 2)
					set_tx_preset(hdev);
				//change to gen4
				ret = rc_change_linkspeed(hdev, rc, 4);
				count++;
			} while (ret < 0 && count < 3);

			if (ret < 0) {
				ret = hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
				if (ret)
					dev_err(&rc->dev, "restore and enable to root port failed: %d\n", ret);
				return;
			}
#ifdef CONFIG_X86
			if (vendor == X86_VENDOR_HYGON) {
				if ((model == cpu_g3 && stepping != 0)
				    || (model == cpu_g3_5)) {
					if (ret == 0 && !is_phy_para_set) {
						core_index = get_core_index(rc);
						lane_mask = get_lane_mask(rc, rc_pcie_cap_pos, core_index);
						task_list = prepare_task_descriptor(hdev, core_index, lane_mask);
						val_index = get_val_index(model, pkg);
						if (val_index != 0xff && task_list) {
							is_cpu_g3_5 = (model == cpu_g3_5) ? true : false;
							dev_dbg(&rc->dev,
								"core_index=%d,lane_mask=%x,val_index=%d\n",
								core_index, lane_mask, val_index);
							dev_info(&rc->dev, "set phy parameters...\n");
							set_phy_parameters(rc, task_list, lane_mask,
									   val_index, is_cpu_g3_5);
							is_phy_para_set = true;
							redo_eq(hdev, rc, upstream,
								rc_pcie_cap_pos, upstream_pcie_cap_pos);
							wait_link_stable(hdev);
							kfree(task_list);
						}
					}
				}
			}
#endif
			count = 0;
			ret = check_eq_status(upstream, upstream_ext_pos);
			if (ret < 0) {
				do {
					// Bypass eq2
					if (count == 20)
						bypass_eq2(hdev, true);
					redo_eq(hdev, rc, upstream,
						rc_pcie_cap_pos, upstream_pcie_cap_pos);
					wait_link_stable(hdev);
					pci_read_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKSTA,
							     &rc_val);
					rc_current_speed = rc_val & PCI_EXP_LNKSTA_CLS;
					if (rc_current_speed != PCI_EXP_LNKSTA_CLS_16_0GB) {
						count_err++;
						dev_dbg(&rc->dev, "current speed: %d\n",
							rc_current_speed);
					}
					ret = check_eq_status(upstream, upstream_ext_pos);
					count++;
				} while (ret < 0 && count < 21);
				dev_info(&rc->dev, "count: %d, low speed: %d\n",
					 count, count_err);
			}
		}
		ret = hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
		if (ret)
			dev_err(&rc->dev, "restore and enable to root port failed: %d\n", ret);
		break;
	case PCI_EXP_LNKCAP_SLS_8_0GB:
		ret = hydcu_da_save_and_disable_to_rp(hdev->pdev, &head);
		if (ret) {
			dev_err(&rc->dev, "save and disable to root port failed: %d\n", ret);
			hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
			return;
		}
		dev_dbg(&rc->dev, "support gen3\n");
		dev_dbg(&upstream->dev, "support gen3\n");
		dev_info(&rc->dev, "support gen3\n");
#ifdef CONFIG_X86
		if (vendor == X86_VENDOR_INTEL) {
			//intel 4316/6354
			if (model == 106 && stepping == 6) {
				do {
					// open eq2
					if (count == 0)
						bypass_eq2(hdev, false);
					// Bypass eq2
					if (count == 10)
						bypass_eq2(hdev, true);
					redo_eq(hdev, rc, upstream,
						rc_pcie_cap_pos, upstream_pcie_cap_pos);
					wait_link_stable(hdev);
					ret = check_eq_status_gen3(upstream, upstream_pcie_cap_pos);
					count++;
				} while (ret < 0 && count < 11);
				ret = hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
				if (ret)
					dev_err(&rc->dev, "restore and enable to root port failed: %d\n", ret);
				return;
			}
		}
#endif
		pci_read_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKSTA, &rc_val);
		pci_read_config_word(upstream, upstream_pcie_cap_pos + PCI_EXP_LNKSTA,
				     &ups_val);
		rc_current_speed = rc_val & PCI_EXP_LNKSTA_CLS;
		ups_current_speed = ups_val & PCI_EXP_LNKSTA_CLS;
		current_speed = rc_current_speed < ups_current_speed ? rc_current_speed : ups_current_speed;
		if (rc_current_speed != PCI_EXP_LNKSTA_CLS_16_0GB) {
			if (current_speed < PCI_EXP_LNKSTA_CLS_8_0GB) {
				do {
					//change to gen3
					ret = rc_change_linkspeed(hdev, rc, 3);
					count++;
				} while (ret < 0 && count < 3);
			} else {
				dev_info(&rc->dev, "Current is Gen3, no need change.\n");
				ret = hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
				if (ret)
					dev_err(&rc->dev, "restore and enable to root port failed: %d\n", ret);
				return;
			}

			if (ret < 0) {
				dev_info(&rc->dev, "Failed to change PCI rate to GEN3.\n");
				ret = hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
				if (ret)
					dev_err(&rc->dev, "restore and enable to root port failed: %d\n", ret);
				return;
			}
#ifdef CONFIG_X86
			if (vendor == X86_VENDOR_HYGON) {
				if ((model == cpu_g3 && stepping != 0)
				    || (model == cpu_g3_5)) {
					if (ret == 0 && !is_phy_para_set) {
						core_index = get_core_index(rc);
						lane_mask = get_lane_mask(rc, rc_pcie_cap_pos, core_index);
						task_list = prepare_task_descriptor(hdev, core_index, lane_mask);
						val_index = get_val_index(model, pkg);
						if (val_index != 0xff && task_list) {
							is_cpu_g3_5 = (model == cpu_g3_5) ? true : false;
							dev_dbg(&rc->dev,
								"core_index=%d,lane_mask=%x,val_index=%d\n",
								core_index, lane_mask, val_index);
							dev_info(&rc->dev, "set phy parameters...\n");
							set_phy_parameters(rc, task_list, lane_mask,
									   val_index, is_cpu_g3_5);
							is_phy_para_set = true;
							redo_eq(hdev, rc, upstream,
								rc_pcie_cap_pos, upstream_pcie_cap_pos);
							wait_link_stable(hdev);
							kfree(task_list);
						}
					}
				}
			}
#endif
			count = 0;
			ret = check_eq_status_gen3(upstream, upstream_pcie_cap_pos);
			if (ret < 0) {
				do {
					// Bypass eq2
					if (count == 20)
						bypass_eq2(hdev, true);
					redo_eq(hdev, rc, upstream,
						rc_pcie_cap_pos, upstream_pcie_cap_pos);
					wait_link_stable(hdev);
					pci_read_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKSTA,
							     &rc_val);
					rc_current_speed = rc_val & PCI_EXP_LNKSTA_CLS;
					if (rc_current_speed !=
						PCI_EXP_LNKSTA_CLS_16_0GB) {
						count_err++;
						dev_dbg(&rc->dev, "current speed: %d\n",
							rc_current_speed);
					}
					ret = check_eq_status_gen3(upstream, upstream_pcie_cap_pos);
					count++;
				} while (ret < 0 && count < 21);

				dev_info(&rc->dev, "count: %d, low speed: %d\n",
					 count, count_err);
			}
		}
		ret = hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
		if (ret)
			dev_err(&rc->dev, "restore and enable to root port failed: %d\n", ret);
		break;
	case PCI_EXP_LNKCAP_SLS_5_0GB:
		ret = hydcu_da_save_and_disable_to_rp(hdev->pdev, &head);
		if (ret) {
			dev_err(&rc->dev, "save and disable to root port failed: %d\n", ret);
			hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
			return;
		}
		dev_dbg(&rc->dev, "support gen2\n");
		dev_dbg(&upstream->dev, "support gen2\n");
		pci_read_config_word(rc, rc_pcie_cap_pos + PCI_EXP_LNKSTA, &rc_val);
		pci_read_config_word(upstream, upstream_pcie_cap_pos + PCI_EXP_LNKSTA,
				     &ups_val);
		rc_current_speed = rc_val & PCI_EXP_LNKSTA_CLS;
		ups_current_speed = ups_val & PCI_EXP_LNKSTA_CLS;
		current_speed = rc_current_speed < ups_current_speed ? rc_current_speed : ups_current_speed;
		if (rc_current_speed != PCI_EXP_LNKSTA_CLS_16_0GB) {
			if (current_speed < PCI_EXP_LNKCAP_SLS_5_0GB) {
				do {
					ret = rc_change_linkspeed(hdev, rc, 3);
					count++;
				} while (ret < 0 && count < 3);
			} else {
				dev_info(&rc->dev, "Current is Gen2, no need change.\n");
				ret = hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
				if (ret)
					dev_err(&rc->dev, "restore and enable to root port failed: %d\n", ret);
				return;
			}
		}
		ret = hydcu_da_restore_and_enable_to_rp(hdev->pdev, &head);
		if (ret)
			dev_err(&rc->dev, "restore and enable to root port failed: %d\n", ret);
		break;
	default:
		break;
	}

}

static bool hydcu_should_change_speed(struct pci_dev *pdev)
{
	uint32_t device = pdev->device;
	uint32_t sid = pdev->subsystem_device;
	bool ret = false;

	switch (device) {
	case DEVICE_KONGMING_SNAP:
		ret = true;
		break;
	case DEVICE_ZHONGDA_ECO:
		if (sid != 0x62a0)
			ret = true;
		break;
	case DEVICE_ZHONGDA:
		if (sid != 0x6214 && sid != 0x62a0 && sid != 0x62b0)
			ret = true;
		break;
	}

	return ret;
}

struct pci_dev *hydcu_find_outside_device(struct pci_dev *pdev)
{
	struct pci_dev *parent = pdev;
	struct pci_dev *outside_dev = NULL;

	while (parent->bus->self != NULL) {
		if (pci_pcie_type(parent) == PCI_EXP_TYPE_UPSTREAM && !outside_dev) {
			outside_dev = parent->bus->self;
			break;
		}
		parent = parent->bus->self;
	}

	return outside_dev;
}

static char *strcasestr(const char *haystack, const char *needle)
{
	if (!haystack || !needle)
		return NULL;
	if (*needle == 0)
		return (char *)haystack;

	for (; *haystack != 0; haystack++) {
		const char *h = haystack;
		const char *n = needle;

		while (*h != 0 && *n != 0 &&
			tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
			h++;
			n++;
		}

		if (*n == 0)
			return (char *)haystack;
	}

	return NULL;
}

static int hydcu_pci_topo_info_create(struct hydcu_device *hdev)
{
	struct pci_dev *outside = hydcu_find_outside_device(hdev->pdev);
	uint32_t msg = 0;

	if (!outside)
		return -EINVAL;

	if (pci_pcie_type(outside) == PCI_EXP_TYPE_DOWNSTREAM)
		msg = (outside->device << 16) | outside->vendor;
	else if (pci_pcie_type(outside) == PCI_EXP_TYPE_ROOT_PORT)
		msg = 0x10000;
	if (msg)
		WREG32_PCIE(mmBIOS_SCRATCH_5, msg);

#ifdef CONFIG_X86
	msg = 0;
	switch (boot_cpu_data.x86_vendor) {
	case X86_VENDOR_INTEL:
		if (strstr(boot_cpu_data.x86_model_id, "4316") ||
			strstr(boot_cpu_data.x86_model_id, "6354"))
			msg = 1;
		else if (strcasestr(boot_cpu_data.x86_model_id, "PLATINUM 8568Y"))
			msg = 2;
		break;
	case X86_VENDOR_AMD:
		if (strcasestr(boot_cpu_data.x86_model_id, "EPYC 9K24"))
			msg = 3;
		break;
	}

	if (!msg) {
		msg = boot_cpu_data.x86_vendor << 24 | 0x80000000;
		msg |= (boot_cpu_data.x86 << 16) | (boot_cpu_data.x86_model << 8);
       /* can remove below if build fail */
		msg |= boot_cpu_data.x86_stepping;
	} else {
		msg |= 0x40000000;
	}
	WREG32_PCIE(mmBIOS_SCRATCH_6, msg);
#endif
	return 0;
}

int hycu_retimer_low_latency_mode_enter(struct hydcu_device *hdev)
{
	int lm_offset, ret, i;
	u16 link_status = 0;
	u16 lane_conf[] = {
		0x182a, 0x102a, 0x182a, 0x162a,
		0x182a, 0x102a, 0x182a, 0x162a,
		0x182a, 0x102a, 0x182a, 0x162a,
		0x182a, 0x102a, 0x182a, 0x162a,
	};
	struct pci_dev *outside = hydcu_find_outside_device(hdev->pdev);

#define PCI_EXT_CAP_ID_LMR 0x27
	if (!outside)
		return 0;

	lm_offset = pci_find_ext_capability(outside, PCI_EXT_CAP_ID_LMR);
	if (!lm_offset) {
		dev_info(&outside->dev, "Device does not support Lane Margining at the Receiver.\n");
		return 0;
	}
	dev_info(&outside->dev, "LMR Capability found at offset 0x%x\n", lm_offset);

	/* is gen5 L0? */
	ret = pcie_capability_read_word(outside, PCI_EXP_LNKSTA, &link_status);
	if (ret ||  (link_status & 0xF) != PCI_EXP_LNKSTA_CLS_32_0GB) {
		dev_info(&outside->dev, "HCU not in right mode: 0x%x, ret=%d\n", link_status, ret);
		return -EINVAL;
	}

	/* config lane */
	for (i = 0; i < 16; i++) {
		ret = pci_write_config_word(outside, lm_offset + 0x08 + i * 4, lane_conf[i]);
		if (ret) {
			dev_info(&outside->dev, "Config low latency fail, ret=%d\n", ret);
			return ret;
		}
	}

	mdelay(100);

	/* trigger retraining */
	ret = pcie_retrain_link(outside, true);
	if (ret)
		dev_err(&outside->dev, "Retraining failed, ret=%d\n", ret);
	else
		dev_info(&outside->dev, "Retraining done\n");

	return ret;
}

bool hydcu_should_enable_low_latency(struct hydcu_device *hdev)
{
	struct pci_dev *pdev = hdev->pdev;

	if (pdev->device == 0x6320) {
		switch (pdev->subsystem_device) {
		case 0x631A:
		case 0x6310:
		case 0x6311:
			return true;
		default:
			break;
		}
	} else if (pdev->device == 0x6310) {
		switch (pdev->subsystem_device) {
		case 0x6310:
		case 0x6311:
			return true;
		default:
			break;
		}
	}

	return false;
}

#define SOCKET_ID_FLAG                 (2 << 16)
static uint32_t max_socket_id;
static int hycu_config_s5(struct hydcu_device *hdev, void *data)
{
	uint32_t socket_id = *(uint32_t *)data;

	WREG32_PCIE(mmBIOS_SCRATCH_5, (SOCKET_ID_FLAG | socket_id));
	return 0;
}

static void hycu_detect_topo_type(struct hydcu_device *hdev)
{
	uint32_t socket_id = RREG32_PCIE(mmSOCKET_ID);
	uint32_t last_config = RREG32_PCIE(mmBIOS_SCRATCH_5);

	if ((last_config & 0xFFFF0000) == SOCKET_ID_FLAG)
		last_config = last_config & 0xFFFF;
	else
		last_config = 0;

	if (socket_id > max_socket_id)
		max_socket_id = socket_id;

	if (max_socket_id <= last_config) {
		max_socket_id = last_config;
		dev_info(&hdev->pdev->dev, "socket id already be %d\n", max_socket_id);
		return;
	}

	hycu_broadcast_to_devices(hycu_config_s5, (void *)&max_socket_id);
	dev_info(&hdev->pdev->dev, "config socket id=%d\n", max_socket_id);
}

int hydcu_pci_speed_action(struct hydcu_device *hdev)
{
	switch (hdev->pdev->device) {
	case DEVICE_KONGMING:
	case DEVICE_KONGMING_SNAP:
	case DEVICE_ZHONGDA:
	case DEVICE_ZHONGDA_ECO:
		hydcu_pci_topo_info_create(hdev);
		break;
	default:
		hycu_detect_topo_type(hdev);
		break;
	}

	if (hydcu_should_change_speed(hdev->pdev))
		hydcu_check_change_speed(hdev);

	if (hydcu_should_enable_low_latency(hdev))
		hycu_retimer_low_latency_mode_enter(hdev);
	return 0;
}
