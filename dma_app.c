/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 Marvell.
 */

#include <stdint.h>
#include <linux/vfio.h>
#include <misc/mrvl_cn10k_dpi.h>

#include "dpi_vf.h"
#include "log.h"
#include "vfio_dev.h"

#define DPI_MAX_MOLR	1024
#define DPI_MAX_MPS	1024
#define DPI_MIN_MPS	128
#define DPI_MAX_MRRS	1024
#define DPI_MIN_MRRS	128
#define DPI_BURST_REQ	256
#define MAX_POINTERS	15
#define MAX_DPI_VFS	32
#define MAX_DPI_ENGS	6

static uint64_t fifo_mask = 0x10101010;
static uint16_t mrrs = 256;
static uint16_t mps = 128;
static uint16_t pem_id;
static char *pci_addr;

static char dpi_node[] = "/dev/mrvl_cn10k_dpi";

#define CNXK_DPI_MAX_POINTER		    15
#define CNXK_DPI_MAX_VCHANS_PER_QUEUE	    128
#define CNXK_DPI_QUEUE_BUF_SIZE		    16256
#define CNXK_DPI_QUEUE_BUF_SIZE_V2	    130944
#define CNXK_DPI_POOL_MAX_CACHE_SZ	    (16)
#define CNXK_DPI_DW_PER_SINGLE_CMD	    8
#define CNXK_DPI_HDR_LEN		    4
#define CNXK_DPI_CMD_LEN(src, dst)	    (CNXK_DPI_HDR_LEN + ((src) << 1) + ((dst) << 1))
#define CNXK_DPI_MAX_CMD_SZ		    CNXK_DPI_CMD_LEN(CNXK_DPI_MAX_POINTER,		\
							     CNXK_DPI_MAX_POINTER)
#define CNXK_DPI_CHUNKS_FROM_DESC(cz, desc) (((desc) / (((cz) / 8) / CNXK_DPI_MAX_CMD_SZ)) + 1)

struct dma_device {
	struct vfio_mem_resource *res;
	struct dpi_vf rdpi;
	uint32_t aura;
	int vfio_fd; /* device fd */
};

/* display usage */
static void cmdline_arg_usage(const char *prgname)
{
	printf("%s -a <bus:device:function>\n"
		"--mps=<size>: Max payload size of PCIe trascation\n"
		"--fifo_mask=<mask>: FIFO size mask of DPI DMA engines\n"
		"--mrrs=<size>: Max PCIe read request size\n", prgname);
}

static int parse_dmadev_pci_addr(const char *arg)
{
	size_t optlen;
	int ret;

	optlen = strlen(optarg) + 1;
	pci_addr = malloc(optlen);
	if (pci_addr == NULL) {
		error("Unable to allocate device option");
		return -ENOMEM;
	}

	ret = snprintf(pci_addr, optlen, "%s", arg);
	if (ret < 0) {
		error("Unable to copy device option");
		free(pci_addr);
		return -EINVAL;
	}

	return 0;
}

/* earse the argument given in the command line of the application */
static int parse_app_args(int argc, char **argv)
{
	int opt, ret, opt_idx;
	char **argvopt;
	char *prgname = argv[0];

	static struct option lgopts[] = {
		{ "mps", 1, 0, 0},
		{ "mrrs", 1, 0, 0},
		{ "fifo_mask", 1, 0, 0},
		{0, 0, 0, 0 },
	};

	argvopt = argv;
	pci_addr = NULL;

	while ((opt = getopt_long(argc, argvopt, "a:", lgopts, &opt_idx)) != EOF) {

		switch (opt) {
		case 'a':
			ret = parse_dmadev_pci_addr(optarg);
			if (ret < 0) {
				printf("Invalid pci device\n");
				return ret;
			}
			break;
		case 0: /* long options */
			if (!strcmp(lgopts[opt_idx].name, "mps")) {
				mps = atoi(optarg);
				if (mps < DPI_MIN_MPS || mps > DPI_MAX_MPS) {
					error("Invalid max payload size param");
					return -EINVAL;
				}
			}
			if (!strcmp(lgopts[opt_idx].name, "mrrs")) {
				mrrs = atoi(optarg);
				if (mrrs < DPI_MIN_MRRS || mrrs > DPI_MAX_MRRS) {
					error("Invalid max read req size param");
					return -EINVAL;
				}
			}
			if (!strcmp(lgopts[opt_idx].name, "fifo_mask"))
				fifo_mask = atoll(optarg);
			break;
		default:
			cmdline_arg_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */
	return ret;
}

static struct dma_device *dmadev_init(const char *pci_addr)
{
	struct dma_device *dev;

	/* Allocate memory for the dpi device that will be returned */
	dev = (struct dma_device *) malloc(sizeof(struct dma_device));
	if (dev == NULL) {
		error("Could not allocate memory");
		return dev;
	}

	dev->rdpi.pci_addr = strdup(pci_addr);

	/* initialize the IOMMU for this device */
	dev->vfio_fd = vfio_dev_init(pci_addr);
	if (dev->vfio_fd < 0) {
		error("could not initialize the IOMMU for device %s", pci_addr);
		goto err;
	}

	/* Map BAR0 region */
	dev->res = vfio_dev_map_region(dev->vfio_fd, VFIO_PCI_BAR0_REGION_INDEX);
	if (dev->res == (void *)-1) {
		error("Failed to map BAR0");
		goto err;
	}

	dev->rdpi.rbase = dev->res->addr;
	dpi_vf_dev_init(&dev->rdpi);

	return dev;

err:
	free(dev);
	return NULL;
}

static void dmadev_fini(struct dma_device *dev)
{
	vfio_dev_fini(dev->vfio_fd, dev->res);
}

static int dmadev_open(struct dma_device *dev)
{
	int rc;

	rc = dpi_vf_configure_v2(&dev->rdpi, CNXK_DPI_QUEUE_BUF_SIZE_V2, dev->aura);
	if (rc < 0) {
		error("DMA configure v2 failed err = %d", rc);
		goto open_v1;
	}
	goto done;

open_v1:
	rc = dpi_vf_configure(&dev->rdpi, CNXK_DPI_QUEUE_BUF_SIZE, dev->aura);
	if (rc < 0) {
		error("DMA configure failed err = %d", rc);
		goto error;
	}
done:

	dpi_vf_enable(&dev->rdpi);
error:
	return rc;
}

static void dmadev_close(struct dma_device *dev)
{
	dpi_vf_disable(&dev->rdpi);
	dpi_vf_dev_fini(&dev->rdpi);
}

int main(int argc, char **argv)
{
	struct dpi_mps_mrrs_cfg mcfg;
	struct dpi_engine_cfg ecfg;
	struct dma_device *dev;
	int dpi_fd;
	int ret;

	ret = parse_app_args(argc, argv);
	if (ret < 0) {
		error("Invalid App arguments\n");
		return ret;
	}

	/* Return error if no DPI VF device's PCI address is passed */
	if (pci_addr == NULL) {
		error("DPI VF PCI address is not passed\n");
		cmdline_arg_usage(argv[0]);
		return 1;
	}

	/* Open DPI PF's device node to invoke IOCTL calls to PF driver */
	dpi_fd = open(dpi_node, O_RDWR);
	if (dpi_fd < 0) {
		error("Failed to open dpi pf mode\n");
		return 1;
	}

	dev = dmadev_init(pci_addr);
	if (dev == NULL) {
		error("Failed to initialize DPI VF device\n");
		return 1;
	}

	memset(&mcfg, 0, sizeof(mcfg));
	memset(&ecfg, 0, sizeof(ecfg));

	mcfg.max_payload_sz = mps;
	mcfg.max_read_req_sz = mrrs;
	mcfg.port = pem_id;
	ecfg.fifo_mask = fifo_mask;
	ecfg.update_molr = 0;

	if (ioctl(dpi_fd, DPI_MPS_MRRS_CFG, &mcfg)) {
		error("Failed to set MPS & MRRS parameters\n");
		goto err_dpi_cfg;
	}
	if (ioctl(dpi_fd, DPI_ENGINE_CFG, &ecfg)) {
		error("Failed to configure DPI Engine FIFO sizes\n");
		goto err_dpi_cfg;
	}

	/* Open DPI VF device and setup it's resources */
	ret = dmadev_open(dev);
	if (ret < 0) {
		error("Failed to open DPI VF device\n");
		goto err_dpi_cfg;
	}

	dmadev_close(dev);
	dmadev_fini(dev);

err_dpi_cfg:
	close(dpi_fd);
	free(pci_addr);
	free(dev);

	return ret;
}
