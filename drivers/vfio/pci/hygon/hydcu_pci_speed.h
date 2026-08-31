/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#ifndef _HYDCU_PCI_SPEED_H
#define _HYDCU_PCI_SPEED_H

/* copy from main driver */
#ifndef X86_VENDOR_INTEL
#define X86_VENDOR_INTEL    0
#endif

#define smnPCIE_LC_CNTL8                        0x11140374
#define smnPCIE_LC_CNTL9                        0x11140378
#define smnPCIE_LC_STATE0                       0x11140294
#define smnPCIE_LC_SPEED_CNTL                   0x11140290
#define smnPCIE_PHY_BASE                        0x12200000
#define smnPCIE_LC_PORT_ORDER_BASE              0x11140000
#define smnPCIE_LC_PORT_ORDER_REG_OFF           0x320
#define CPU_PCIE_CORE_DEVNUM                    3
#define NB_SMN_INDEX                            0xB8
#define NB_SMN_DATA                             0xBC
#define PCIE_LSTSSM_L0                          0x10

#define PCIE_LC_CNTL8__LC_BYPASS_EQ_REQ_PHASE_16GT__SHIFT   0xb
#define PCIE_LC_CNTL9__LC_DSC_ACCEPT_16GT_EQ_REDO__SHIFT    0x19
#define PCIE_LC_CNTL8__LC_USC_EQ_NOT_REQD_16GT__SHIFT       0x8

#define PCIE_LINK_STATUS2_REG_OFFSET            0x30
#define PCIE_LINK_STATUS2_REG_SHIFT             16
#define PCIE_LINK_STATUS2_REG_MASK              0xFF
#define PCIE_LINK_STATUS2_8GT_STA_E3_MASK       0x10
#define PCIE_LINK_STATUS2_8GT_STA_E2_E1_E0_MASK 0x0C


#ifndef PCI_EXT_CAP_ID_PL_16GT
#define PCI_EXT_CAP_ID_PL_16GT          0x0026	/* Physical Layer 16.0 GT/s Extended Capability */
#endif
#define PCI_EXT_LNKCTL3                 4	/* Link Control 3 */
#define PCI_EXT_LNKCTL3_PE              0x1
#define PCI_EXT_16GT_STA                0x0C	/* 16.0 GT/s Status Register */
#define PCI_EXT_16GT_STA_E3_MASK        0x8  /*bit3*/
#define PCI_EXT_16GT_STA_E2_E1_E0_MASK  0x7  /*bit2/1/0*/
#define PCI_EXP_LNKCTL2_TLS_16_0GT      0x0004 /* Supported Speed 16GT/s */

#define CPUID_FAMMODSTEPEXT_INDEX         0x80000001
#define CPUID_BASE_MODEL_MASK             0x000000F0ul
#define CPUID_BASE_MODEL_RIGHT_OFFSET     4
#define CPUID_BASE_STEPPING_MASK          0x0000000Ful
#define CPUID_BASE_STEPPING_RIGHT_OFFSET  0
#define CPUID_PKG_TYPE_MASK               0xF0000000ul
#define CPUID_PKG_TYPE_RIGHT_OFFSET       28

#define QCHAR(a, b, c, d) ((a) + ((b) << 8) + ((c) << 16) + ((d) << 24))
#define CPUID_INTEL1 QCHAR('G', 'e', 'n', 'u')
#define CPUID_INTEL2 QCHAR('i', 'n', 'e', 'I')
#define CPUID_INTEL3 QCHAR('n', 't', 'e', 'l')
#define CPUID_AMD1 QCHAR('A', 'u', 't', 'h')
#define CPUID_AMD2 QCHAR('e', 'n', 't', 'i')
#define CPUID_AMD3 QCHAR('c', 'A', 'M', 'D')
#define CPUID_HYGON1 QCHAR('H', 'y', 'g', 'o')
#define CPUID_HYGON2 QCHAR('n', 'G', 'e', 'n')
#define CPUID_HYGON3 QCHAR('u', 'i', 'n', 'e')

#define CPUID_IS(a, b, c, ebx, ecx, edx)        \
		(!((ebx ^ (a))|(edx ^ (b))|(ecx ^ (c))))

typedef union {
	struct {
		uint32_t LC_GEN2_EN_STRAP : 1;
		uint32_t LC_GEN3_EN_STRAP : 1;
		uint32_t LC_GEN4_EN_STRAP : 1;
		uint32_t LC_TARGET_LINK_SPEED_OVERRIDE_EN : 1;
		uint32_t LC_TARGET_LINK_SPEED_OVERRIDE : 2;
		uint32_t LC_FORCE_EN_SW_SPEED_CHANGE : 1;
		uint32_t LC_FORCE_DIS_SW_SPEED_CHANGE : 1;
		uint32_t LC_FORCE_EN_HW_SPEED_CHANGE : 1;
		uint32_t LC_FORCE_DIS_HW_SPEED_CHANGE : 1;
		uint32_t LC_INITIATE_LINK_SPEED_CHANGE : 1;
		uint32_t LC_SPEED_CHANGE_ATTEMPTS_ALLOWED : 2;
		uint32_t LC_SPEED_CHANGE_ATTEMPT_FAILED : 1;
		uint32_t LC_CURRENT_DATA_RATE : 2;
		uint32_t LC_DONT_CLR_TRGET_SPD_CHANGE_STATUS : 1;
		uint32_t LC_CLR_FAILED_SPD_CHANGE_CNT : 1;
		uint32_t RESERVED_18_31 : 14;
	};
	uint32_t data;
} PCIE_LC_SPEED_CNTL_T;

typedef union {
	struct {
		uint32_t LC_EQ_SEARCH_MODE_16GT                  : 2;
		uint32_t LC_BYPASS_EQ_16GT                       : 1;
		uint32_t LC_BYPASS_EQ_PRESET_16GT                : 4;
		uint32_t LC_REDO_EQ_16GT                         : 1;
		uint32_t LC_USC_EQ_NOT_REDO_16GT                 : 1;
		uint32_t LC_USC_GO_TO_EQ_16GT                    : 1;
		uint32_t LC_UNEXPECTED_COEFFS_RCVD_16GT          : 1;
		uint32_t LC_BYPASS_EQ_REQ_PHASE_16GT             : 1;
		uint32_t LC_FORCE_PRESET_IN_EQ_REQ_PHASE_16GT    : 1;
		uint32_t LC_FORCE_PRESET_VALUE_16GT              : 4;
		uint32_t LC_EQTS2_PRESET_EN                      : 1;
		uint32_t LC_EQTS2_PRESET                         : 4;
		uint32_t LC_USE_EQTS2_PRESET                     : 1;
		uint32_t LC_FOM_TIME                             : 2;
		uint32_t LC_SAFE_EQ_SEARCH                       : 1;
		uint32_t LC_DONT_CHECK_EQTS_IN_RCFG              : 1;
		uint32_t LC_DELAY_COEFF_UPDATE_DIS               : 1;
		uint32_t LC_8GT_EQ_REDO_EN                       : 1;
		uint32_t LC_WAIT_FO_EIEOS_IN_RLOCK               : 1;
		uint32_t LC_DYNAMIC_INACTIVE_TS_SELECT           : 2;
	};
	uint32_t data;
} PCIE_LC_CNTL8_T;

typedef struct {
	uint8_t index;
	uint8_t numlanes;
	uint8_t start_lane;
} PHYINFO;

typedef struct {
	uint32_t reg_offset;
	uint32_t val[5];
} PHYPARA;

typedef struct {
	uint32_t core_index;
	uint32_t phy_index;
	uint32_t lane_index;
} TASK_INFO;

typedef enum {
	pkg_sl1 = 4,
	pkg_dm1 = 6,
	pkg_sl1r2 = 7,
} PKG_TYPE;

typedef enum {
	cpu_g1 = 0,
	cpu_g2,
	cpu_g3,
	cpu_g3_5,
} GEN_TYPE;

extern uint32_t cpu_phy_para_index;

#endif /* _HYDCU_PCI_SPEED_H */
