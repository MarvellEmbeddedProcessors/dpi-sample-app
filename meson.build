# SPDX-License-Identifier: BSD-3-Clause
# Copyright(C) 2024 Marvell.

project(
	'DPI DMA Sample Application',
	'C',
	meson_version: '>= 0.63.0',
	version: '24.03.0',
	default_options: ['warning_level=2', 'werror=true']
)
executable('dma_app', 'vfio_dev.c', 'dpi_vf.c', 'dma_app.c')
