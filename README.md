OCTEON DPI DMA sample application
=================================

copyright (c) 2024 Marvell.

This repo contains a simple DPI DMA application, it is just a sample application to provide
a reference for developing DMA application along with user space DMA drivers.

Overview
========
The purpose of this application is to demonstrates on how user space applications intract with
Octeon's DPI DMA physical function(PF) kernel driver. The kernel driver running on the DPI PF
enables the hardware mailbox between DPI Physical function & it's virtual function, so that VFs
can send mailbox requests to PF device (sitting in kernel space) for setting up the respective VF
device resource configuration. PF kernel driver also creates a device node to receive any requests
(like configuring DPI DMA hardware's global resources) from user space dma applications through
ioctl calls.

Identifying CN10K DPI DMA VF devices
====================================
#. Load DPI PF kernel driver and enable required number of VFs

   .. code-block:: shell-session

     # insmode mrvl_cn10k_dpi.ko
     # echo 8 > /sys/bus/pci/devices/0000\:06\:00.0/sriov_numvfs

#. Use lspci command to list DPI PF & VFs devices.

   .. code-block:: shell-session

      # lspci | grep a080
      # lspci | grep a081

   The above commands show below similar output

   .. code-block:: shell-session

      0000:06:00.0 System peripheral: Cavium, Inc. Device a080 /* CN10K DPI PF */
      0000:06:00.1 System peripheral: Cavium, Inc. Device a081 /* CN10K DPI VF0 */
      0000:06:00.2 System peripheral: Cavium, Inc. Device a081 /* CN10K DPI VF1 */
      0000:06:00.3 System peripheral: Cavium, Inc. Device a081 /* CN10K DPI VF2 */
      0000:06:00.4 System peripheral: Cavium, Inc. Device a081 /* CN10K DPI VF3 */
      0000:06:00.5 System peripheral: Cavium, Inc. Device a081 /* CN10K DPI VF4 */
      0000:06:00.6 System peripheral: Cavium, Inc. Device a081 /* CN10K DPI VF5 */
      0000:06:00.7 System peripheral: Cavium, Inc. Device a081 /* CN10K DPI VF6 */
      0000:06:01.0 System peripheral: Cavium, Inc. Device a081 /* CN10K DPI VF7 */

#. Bind VF devices to vfio-pci driver and use them in user space DPI applications.

   .. code-block:: shell-session

      # modprobe vfio-pci
      # echo "177d a081" > /sys/bus/pci/drivers/vfio-pci/new_id

Building and running the sample application
===========================================
#. Run below meson & ninja commands to build sample test on Marvell's CN10K platform.

   .. code-block:: shell-session

      # meson setup build
      # ninja -C build

#. Command to run dma_app on DPI VF0 device and set MPS to 256B and MRRS to 512B in DPI hardware.

   .. code-block:: shell-session

      # ./build/dma_app -a 0000:06:00.1 --mps 256 --mrrs 512
