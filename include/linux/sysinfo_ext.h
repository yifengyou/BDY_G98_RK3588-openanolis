/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SYSINFO_EXT_H
#define _LINUX_SYSINFO_EXT_H

#include <linux/mmzone.h>

struct sysinfo_ext {
	unsigned long lrupages[NR_LRU_LISTS];
	unsigned long cached;
	unsigned long available;
	unsigned long file_dirty;
	unsigned long writeback;
	unsigned long anon_mapped;
	unsigned long file_mapped;
	unsigned long slab_reclaimable;
	unsigned long slab_unreclaimable;
	unsigned long kernel_stack_kb;
	unsigned long writeback_temp;
	unsigned long anon_thps;
	unsigned long shmem_thps;
	unsigned long shmem_pmd_mapped;
};

#endif /* _LINUX_SYSINFO_EXT_H */
