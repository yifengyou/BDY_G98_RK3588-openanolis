// SPDX-License-Identifier: GPL-2.0

#include <linux/string.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem_vram_helper.h>
#include <drm/ttm/ttm_caching.h>
#include <drm/ttm/ttm_device.h>
#include <drm/ttm/ttm_placement.h>

#include "yhgch_drm_drv.h"

int yhgch_dumb_create(struct drm_file *file, struct drm_device *dev,
		      struct drm_mode_create_dumb *args)
{
	return drm_gem_vram_fill_create_dumb(file, dev, 0, 16, args);
}

static struct ttm_device_funcs yhgch_vram_funcs_override;

static int yhgch_ttm_io_mem_reserve(struct ttm_device *bdev,
				  struct ttm_resource *mem)
{
	struct drm_vram_mm *vmm = drm_vram_mm_of_bdev(bdev);

	switch (mem->mem_type) {
	case TTM_PL_SYSTEM:
		break;
	case TTM_PL_VRAM:
		mem->bus.offset = (mem->start << PAGE_SHIFT) + vmm->vram_base;
		mem->bus.is_iomem = true;
		mem->bus.caching = ttm_uncached;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void yhgch_vram_post_init(struct yhgch_drm_private *yhgch)
{
	struct drm_device *dev = yhgch->dev;
	struct drm_vram_mm *vmm;
	struct ttm_device *bdev;

	if (!yhgch->is_5c01_device || !dev->vram_mm)
		return;

	vmm = dev->vram_mm;
	bdev = &vmm->bdev;

	memcpy(&yhgch_vram_funcs_override, bdev->funcs,
	       sizeof(yhgch_vram_funcs_override));
	yhgch_vram_funcs_override.io_mem_reserve = yhgch_ttm_io_mem_reserve;
	*(const struct ttm_device_funcs **)(void *)&bdev->funcs =
		&yhgch_vram_funcs_override;
}

const struct drm_mode_config_funcs yhgch_mode_funcs = {
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
	.fb_create = drm_gem_fb_create,
	.mode_valid = drm_vram_helper_mode_valid,
};
