# WS2812 Driver

This firmware implements a bit-banging driver for a string of WS2812 LEDs connected to a Creator Ci40 board.

## Hardware

The LED string should be connected to the Ci40 expansion port (https://docs.creatordev.io/ci40/guides/hardware/#primary-expansion-header) as follows:

| Ci40 Expansion Header (CN5) Pin | WS2812   |
| :-----------------------------: |:--------:|
| 2  (+5V)                        | +5V      |
| 39 (GND)                        | GND      |
| 8  (MFIO14)                     | DATA     |

### Jumpers
MFIO14 needs to be configured as IO rather than UART2 TX by moving jumper JP7 to bridge the 2 pins closest to the expansion header.

## Software

The remoteproc firmware is preconfigured to drive a string of 144 LEDs, but the length of the string can easily be changed with the NUM_LEDS define.
The timing of the WS2812's is done via the ws2812_delay function and a NS_TO_LOOPS macro to calculate the equivalent number of ticks of the MIPS coprocessor 0 timer for a given delay.
The pattern driven to the string is configured in the main loop.

## Running on the CI40

* Apply the patches from the kernel_patches directory to a kernel of the right version.
```
linux $ patch -p1 < <exmaple>/kernel_patches/<version>/*
```
* Configure the kernel for the Ci40 using the pistachio_defconfig
```
linux $ ARCH=mips make pistachio_defconfig
```
* Build the kernel uImage
```
linux $ ARCH=mips CROSS_COMPILE=mips-mti-linux-gnu- make uImage
```
* Boot the kernel, and mount the root filesystem (you might need to tftp the kernel, and mount the root filesystem via NFS, for example).
* Build the example firmware
```
example $ CROSS_COMPILE=mips-mti-linux-gnu- make
```
* Copy the firmware image to the Ci40, placing it in the /lib/firmware/ directory with the name 'rproc-mips-cpu3-fw'
* Log into the Ci40, and offline CPU3 from Linux:
```
# echo 0 > /sys/devices/system/cpu/cpu3/online
```
* Hand CPU3 to the MIPS remoteproc driver. The output should show the firmware starting as shown
```
# echo 3 > /sys/devices/mips-rproc/cpus
remoteproc remoteproc0: mips-cpu3 is available
remoteproc remoteproc0: Note: remoteproc is still under development and considered experimental.
remoteproc remoteproc0: THE BINARY FORMAT IS NOT YET FINALIZED, and backward compatibility isn't yet guaranteed.
remoteproc remoteproc0: powering up mips-cpu3
remoteproc remoteproc0: Booting fw image rproc-mips-cpu3-fw, size 99080
remoteproc remoteproc0: registered virtio0 (type 11)
remoteproc remoteproc0: remote processor mips-cpu3 is now up
remoteproc remoteproc0: mips-cpu3 booting firmware rproc-mips-cpu3-fw
```
The firmware is now running on CPU3, and the WS2812 LED string should light up: https://github.com/MIPS/mips-rproc-example/blob/master/img/VID_20161208_165816.mp4
* ...
* Profit