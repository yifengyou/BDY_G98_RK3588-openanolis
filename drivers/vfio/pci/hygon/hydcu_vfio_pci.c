// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Hygon Info Technologies Ltd.
 */

#include <linux/device.h>
#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/vfio.h>
#include <linux/vfio_pci_core.h>
#include <linux/anon_inodes.h>
#include "hydcu.h"

static int vfio_pci_open_device(struct vfio_device *core_vdev)
{
	struct vfio_pci_core_device *vdev =
		container_of(core_vdev, struct vfio_pci_core_device, vdev);
	int ret;

	ret = vfio_pci_core_enable(vdev);
	if (ret)
		return ret;

	vfio_pci_core_finish_enable(vdev);

	return 0;
}

static const struct vfio_pci_device_ops vfio_pci_dev_ops = {
	.get_dmabuf_phys = vfio_pci_core_get_dmabuf_phys,
};

const struct vfio_device_ops hydcu_vfio_pci_ops = {
	.name		= "hydcu-vfio-pci",
	.init		= vfio_pci_core_init_dev,
	.release	= vfio_pci_core_release_dev,
	.open_device	= vfio_pci_open_device,
	.close_device	= vfio_pci_core_close_device,
	.ioctl		= vfio_pci_core_ioctl,
	.device_feature = vfio_pci_core_ioctl_feature,
	.read		= vfio_pci_core_read,
	.write		= vfio_pci_core_write,
	.mmap		= vfio_pci_core_mmap,
	.request	= vfio_pci_core_request,
	.match		= vfio_pci_core_match,
	.match_token_uuid = vfio_pci_core_match_token_uuid,
	.bind_iommufd	= vfio_iommufd_physical_bind,
	.unbind_iommufd	= vfio_iommufd_physical_unbind,
	.attach_ioas	= vfio_iommufd_physical_attach_ioas,
	.detach_ioas	= vfio_iommufd_physical_detach_ioas,
	.pasid_attach_ioas	= vfio_iommufd_physical_pasid_attach_ioas,
	.pasid_detach_ioas	= vfio_iommufd_physical_pasid_detach_ioas,
};

static int hydcu_vfio_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct vfio_pci_core_device *vdev;
	int ret;

	/* Fixup the pcie header of hydcu */
	hydcu_fixup_pci_header(pdev);

	vdev = vfio_alloc_device(vfio_pci_core_device, vdev, &pdev->dev,
				 &hydcu_vfio_pci_ops);
	if (IS_ERR(vdev))
		return PTR_ERR(vdev);

	dev_set_drvdata(&pdev->dev, vdev);
	vdev->pci_ops = &vfio_pci_dev_ops;
	ret = vfio_pci_core_register_device(vdev);
	if (ret)
		goto out_put_vdev;

	return 0;

out_put_vdev:
	vfio_put_device(&vdev->vdev);
	return ret;
}

static void hydcu_vfio_pci_remove(struct pci_dev *pdev)
{
	struct vfio_pci_core_device *vdev = dev_get_drvdata(&pdev->dev);

	vfio_pci_core_unregister_device(vdev);
	vfio_put_device(&vdev->vdev);
}

static const struct pci_device_id hydcu_vfio_pci_table[] = {
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_Z100SM), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_C878182), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_C878186), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_Z100), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_Z100L), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_C878181), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_C878185), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_C878188), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_C878174), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_KONGMING), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_KONGMING_SNAP), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_ZHONGDA), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_ZHONGDA_ECO), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_BW3000), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_BMZ), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, 0x6360), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, 0x6370), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, 0x631A), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, 0x632A), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, 0x637A), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, 0x636A), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_NMZ_8388), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_NMZ_8373), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_NMZ_8378), },
	{ PCI_DRIVER_OVERRIDE_DEVICE_VFIO(PCI_VENDOR_ID_HYGON, DEVICE_NMZ_8383), },
	{},
};

static struct pci_driver hydcu_vfio_pci_driver = {
	.name			= "hydcu-vfio-pci",
	.id_table = hydcu_vfio_pci_table,
	.probe = hydcu_vfio_pci_probe,
	.remove = hydcu_vfio_pci_remove,
	.err_handler = &vfio_pci_core_err_handlers,
	.driver_managed_dma = true,
};

module_pci_driver(hydcu_vfio_pci_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Huang Jun <huangjun@hygon.cn>");
MODULE_AUTHOR("Peng Xiangzhou <pengxiangzhou@hygon.cn>");
MODULE_DESCRIPTION("HYGON VFIO PCI - VFIO PCI driver support for HYGON DCU device family");
