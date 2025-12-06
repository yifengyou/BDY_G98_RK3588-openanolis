/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __KVM_X86_VMX_PMU_INTEL_H
#define __KVM_X86_VMX_PMU_INTEL_H

#include <linux/kvm_host.h>

#include "cpuid.h"

static inline u64 vcpu_get_perf_capabilities(struct kvm_vcpu *vcpu)
{
	if (!guest_cpuid_has(vcpu, X86_FEATURE_PDCM))
		return 0;

	return vcpu->arch.perf_capabilities;
}

static inline bool fw_writes_is_enabled(struct kvm_vcpu *vcpu)
{
	return (vcpu_get_perf_capabilities(vcpu) & PERF_CAP_FW_WRITES) != 0;
}

#endif /* __KVM_X86_VMX_PMU_INTEL_H */
