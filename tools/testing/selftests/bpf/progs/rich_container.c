// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2023 Alibaba, Inc. */

#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

#define MAX_NR_ARR 128

/*
 * 0: no cpus
 * 1: only 1 cpu
 * 2: all cpus
 */
int test_mode;

static void set_cpu_masks(cpumask_t *cpumask, u64 mask)
{
	const size_t len = bpf_core_type_size(*cpumask) / 8;
	int i;

	#pragma unroll
	for (i = 0; i < MAX_NR_ARR; i++) {
		if (i < len)
			cpumask->bits[i] = mask;
	}
}

SEC("cgroup/rich_container_cpu")
int bpf_prog1(struct bpf_rich_container_info *ctx)
{
	cpumask_t *cpumask = &ctx->cpus_mask;

	if (test_mode == 2) {
		set_cpu_masks(cpumask, -1UL);
		return 0;
	}

	set_cpu_masks(cpumask, 0);
	if (test_mode)
		cpumask->bits[0] = 1;

	return 0;
}

char _license[] SEC("license") = "GPL";
