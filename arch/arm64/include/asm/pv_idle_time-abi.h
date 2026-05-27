/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Paravirtualised vCPU idle hint shared structure ABI.
 *
 * The guest publishes the idle state of a vCPU into a per-vCPU shared
 * page so that the host KVM can know whether the vCPU is currently idle
 * without forcing a VM exit on every idle entry/exit.
 */

#ifndef __ASM_PV_IDLE_TIME_ABI_H
#define __ASM_PV_IDLE_TIME_ABI_H

#include <linux/types.h>

/* Values for the flag field. */
#define KVM_PV_VCPU_RUNNING	0
#define KVM_PV_VCPU_IDLE	(1U << 0)

/*
 * When the guest enters its idle loop it sets ``flag`` to KVM_PV_VCPU_IDLE
 * and clears it back to KVM_PV_VCPU_RUNNING on exit.  The host can read
 * ``flag`` at any time to cheaply decide whether the vCPU is currently idle.
 *
 * ``idle_accum`` is accumulated idle time in nanoseconds of this vcpu.
 */
struct kvm_idle_time {
	__le64 flag;
	__le64 idle_accum;
	/* Structure must be 64 byte aligned, pad to that size. */
	__u32 pad[12];
} __packed;

#endif /* __ASM_PV_IDLE_TIME_ABI_H */
