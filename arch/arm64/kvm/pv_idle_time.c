// SPDX-License-Identifier: GPL-2.0-only
/*
 * PV vCPU idle time host support.
 *
 * The guest registers a per-vCPU shared page (kvm_idle_time) via
 * SMCCC. The guest writes the idle flag into that page on idle entry
 * and clears it on idle exit. The host can then peek at that flag to
 * know whether the vCPU is currently in its idle loop.
 */

#include <linux/arm-smccc.h>
#include <linux/kvm_host.h>

#include <asm/pv_idle_time-abi.h>
#include <asm/kvm_mmu.h>
#include <kvm/arm_hypercalls.h>

#define KVM_PV_IDLE_TIME_ALIGNMENT_BITS	5
#define KVM_PV_IDLE_TIME_VALID_BITS	((-1ULL << (KVM_PV_IDLE_TIME_ALIGNMENT_BITS + 1)))
#define KVM_PV_IDLE_TIME_RESERVED_MASK	\
	(((1 << KVM_PV_IDLE_TIME_ALIGNMENT_BITS) - 1) << 1)

long kvm_smccc_register_pv_idle_time(struct kvm_vcpu *vcpu)
{
	struct gfn_to_hva_cache *ghc = &vcpu->arch.pv_idle_time.cache;
	u64 data = smccc_get_arg1(vcpu);
	gpa_t gpa = data & KVM_PV_IDLE_TIME_VALID_BITS;
	int ret, idx;

	/* We rely on the fact that it fits in a single page. */
	BUILD_BUG_ON((sizeof(struct kvm_idle_time) - 1) & KVM_PV_IDLE_TIME_VALID_BITS);

	vcpu->arch.pv_idle_time.base = 0;

	if (data & KVM_PV_IDLE_TIME_RESERVED_MASK)
		return SMCCC_RET_INVALID_PARAMETER;

	if (data & ARM_SMCCC_KVM_ENABLED) {
		idx = srcu_read_lock(&vcpu->kvm->srcu);
		ret = kvm_gfn_to_hva_cache_init(vcpu->kvm, ghc, gpa,
						sizeof(struct kvm_idle_time));
		srcu_read_unlock(&vcpu->kvm->srcu, idx);
		if (ret)
			return SMCCC_RET_INVALID_PARAMETER;
	}

	vcpu->arch.pv_idle_time.base = data;

	return SMCCC_RET_SUCCESS;
}

static bool kvm_pv_idle_time_enabled(struct kvm_vcpu *vcpu)
{
	return (vcpu->arch.pv_idle_time.base & ARM_SMCCC_KVM_ENABLED);
}

bool kvm_arch_is_vcpu_pv_idle(struct kvm_vcpu *vcpu)
{
	struct gfn_to_hva_cache *ghc = &vcpu->arch.pv_idle_time.cache;
	__le64 flag;
	int ret, idx;

	if (!kvm_pv_idle_time_enabled(vcpu))
		return false;

	idx = srcu_read_lock(&vcpu->kvm->srcu);
	ret = kvm_read_guest_offset_cached(vcpu->kvm, ghc, &flag,
					   offsetof(struct kvm_idle_time, flag),
					   sizeof(flag));
	srcu_read_unlock(&vcpu->kvm->srcu, idx);
	if (ret)
		return false;

	return !!(le64_to_cpu(flag) & KVM_PV_VCPU_IDLE);
}
