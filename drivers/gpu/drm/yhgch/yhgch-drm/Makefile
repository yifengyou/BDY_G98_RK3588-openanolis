ifneq ($(KERNELRELEASE),)

yhgch-drm-y := yhgch_drm_drv.o yhgch_drm_de.o yhgch_drm_vdac.o yhgch_ttm.o

obj-$(CONFIG_DRM_YHGCH) += yhgch-drm.o


else
knv := $(shell uname -r)
KERNELDIR := /lib/modules/$(knv)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
