# Introduction

This document describes the MIPS remote processor driver and how it is useful to allow realtime tasks to run on MIPS CPUs and cooperate with Linux.

Embedded systems become more complicated year by year, with increasing requirements for highspeed communications and network stacks to address many different functions and services. These needs are increasingly being met by using a Linux kernel but in most cases the system still needs to deliver the kind of real-time response and determinism associated with simpler Real-time Operating Systems (RTOS). This can be implemented using multiple processors with one running Linux, the other an RTOS, and with communication between them. However, this forces an inflexible partitioning of the system that makes it hard to adapt the product to different market segments and product lines, particularly as price pressure usually dictates that the whole system is on one chip. There have been several attempts to run Linux under an RTOS so that Linux is always pre-emptible, but the complexity and often proprietary nature of these products has limited their adoption. This document describes a different approach that allows a modern multi-core, and optionally multithreaded, MIPS processor to be dynamically configured between the needs of Linux and the demands of an RTOS. This approach is the ‘MIPS remote processor driver’.

Consider a system with short-term real-time processing requirements, that also requires a level of general networking, reporting and data processing. For example an ultra-high speed camera system, that takes short bursts of high speed video then compresses it and uploads the resulting file to a server, or an environmental monitoring system that monitors background levels at a low rate, then collects data at a high rate whenever an increase is detected. Traditionally a microprocessor would be provisioned, scaled for the highest rate of processing required for real-time data capture, and another processor would be provisioned for the background processing and network operations required to upload and report the data to the server.

The MIPS remote processor driver enables MIPS CPUs to be flexibly allocated between the Linux general purpose operating system and one or more CPUs running bare metal code or a real-time operating system. For our example environmental monitoring system with a typical four CPU processor (e.g. 2 cores, 2 threads MIPS processor), in normal operation 1 CPU would monitor the sensor, whilst 3 CPUs would be handling networking and data processing. When a pollution event is detected, the system would increase the number of CPUs monitoring the sensors, whilst reducing the number of CPUs processing and networking. When the event has completed, the system would swap back to 3 CPUs processing the data and uploading results to the servers. This flexible allocation could reduce the total processing power required, saving the cost and complexity of a system with multiple processors or additional cores, or improving system performance with the ability to increase processing power on demand.
Equally, each of the CPUs may be continuously configured for real-time or Linux use if flexible allocation is not required, and the benefits of using a single multi-core multi-threaded microprocessor will still be gained.

## Related Documents
The MIPS® MT Module for the MIPS32® Architecture, MD00378

## Background
A differentiating feature of the MIPS architecture vs other embedded CPU cores is hardware multi-threading (MT) which is included in the MIPS architecture and several MIPS cores including the interAptiv™ and Warrior™ cores. MT can make a single processor core appear to an SMP operating system like two separate cores. Furthermore, these two virtual processors can support multiple operating systems or a combination of an OS and a “bare metal” environment.
Typically, when Linux is booted on a system containing MIPS MT capable cores, it will run across all available virtual processors and cores unless configured to always make certain logical CPUs available for other tasks. This set up is restrictive because it does not allow flexibility in the allocation of CPUs to tasks.
The remote processor driver allows logical CPUs within the system to be managed dynamically. CPUs may run Linux and be available for scheduling Linux tasks, or they may be removed from Linux and given a separate program to run. That could be bare metal real time code, or even an RTOS. This may be useful in system designes which require realtime control of hardware, or for providing a coprocessor dedicated to media playback or data processing, for example.

![VPE usage](https://github.com/MIPS/mips-rproc-example/blob/master/img/vpe_usage.png)

The remote processor framework within the Linux kernel provides mechanisms facilitating the allocation of memory, resources and communication paths to the remote CPU.
##Remote Processor Framework
The remote processor framework has been in the mainline Linux kernel for several [years](https://lwn.net/Articles/464391/). It provides a generic interface for starting and stopping processors, loading firmware, managing memory and allowing standard virtio device based communication. The mainline kernel also provides rpmsg, a generic virtio based messaging framework for communicating with firmware running on the remote processor.
The MIPS remote processor driver implements the remote processor [API](https://www.kernel.org/doc/Documentation/remoteproc.txt) to allow CPUs that are offline in Linux to be used as a remote processor running separate firmware.
Other remote processor implementations typically use device tree nodes to specify the firmware name that each remote processor should be running. Since the MIPS driver treats one of the generic MIPS CPUs which also runs Linux as the remote processor, instead a sysfs interface is used to allow dynamic management of the firmware running on the remote processor.

## Security
A VPE using the MIPS remote processor driver runs in kernel mode with full access to the system memory. It may be impossible for an RTOS, for example, to function without this privileged access. The firmware and Linux are equally privileged and must trust each other with full access to hardware. Since any firmware running has access to the whole of system memory and is in kernel mode, it effectively runs with Linux root privileges. Loading and interaction with firmware should be subject to the necessary security policy in the target system to prevent possible root exploits.
The system must also be designed to ensure that there are no resource conflicts between users, as both Linux and firmware accessing a peripheral concurrently would likely have unpleasant consequences and be difficult to debug.

# Using the MIPS Remote Processor Driver
## Control interface - sysfs
The MIPS remote processor interface is designed to allow dynamic management of the system resources. CPUs can be hotplugged from Linux and reassigned to firmware, then stopped and brought back online in Linux once the task is complete.
First a CPU must be made available by hotplugging it from Linux, via the "online" sysfs interface, e.g.
`# echo 0 > /sys/devices/system/cpu/cpu1/online`
Once the CPU has been removed from Linux control, the MIPS remote processor driver takes hold of it and creates a sysfs interface for it:
```
# ls /sys/class/mips-rproc/rproc1/
firmware     remoteproc0  stop         subsystem    uevent
```
The firmware file can be written with the name of a firmware file, which should be located in /lib/firmware. When this sysfs file is written, the firmware will be loaded and appropriate virtio devices created for it as described by the firmware image. The CPU will then begin executing the firmware.

## Communication via virtio devices
The remote processor framework supports the creation of virtio devices for communication with firmware running on the remote CPU. These are the same types of devices that are used by virtualisation software to create pseudo devices implemented in software. For example, a firmware may provide a virtual serial port, virtual ethernet adaptor, etc which is implemented by the firmware and which will be instantiated as a device on the Linux side. In the case of a virtual serial port, a device node such as /dev/vport0p0 is created to represent the virtual device implemented in firmware.

## Memory layout
When a firmware image is linked to run on a MIPS remote processor, it is statically linked to a particular address. If that region were in one of the unmapped regions of the MIPS address space (KSEG0/KSEG1), then that area would have to be reserved for the firmware image and Linux prevented from using it. If multiple remote processors were to be used, each one would have to have a separate reservation in the virtual address space. This would be difficult to guarantee and unwieldy to manage. For that reason, the MIPS remote processor implementation is designed to allocate the remote processor memory in the mapped region (KUSEG). This means that the firmware can have constant addresses even though the physical memory allocated for it by Linux will not be.
Before the firmware is booted, Linux will create wired TLB entries covering all of this memory to ensure that the firmware has access to it without any TLB refills being necessary.

## Remote processor resource table
The remote processor core code handles a section within the elf image (".resource_table") which has a structure described in [API](https://www.kernel.org/doc/Documentation/remoteproc.txt) and [remoteproc.h](http://lxr.free-electrons.com/source/include/linux/remoteproc.h). Basically this table specifies one or more memory carveouts that the firmware requires, one or more virtio devices provided by the firmware, one or more device memeory records and one or more trace buffers.

### Carveouts
These records specify a chunk of memory that is made available to the firmware. The format is specified in [remoteproc.h](http://lxr.free-electrons.com/source/include/linux/remoteproc.h). If the fimware needs this memory mapped to particular virtual addresses, for example to hold sections of the statically linked firmware, then the address should be set in the da (device address) member. If the firmware just needs to allocate a chunk of memory, then the da member may be set to FW_RSC_ADDR_ANY, in which case Linux will allocate the memory and then put the address in the da member, which the firmware can check once it is running. Memory for these regions is allocated from the contiguous memory pool using the DMA API, so is physically and virtually contiguous.

### Virtio Devices
These records specify that the firmware provides a virtio device implementation and that Linux should allocate a driver for it. The format is specified in [remoteproc.h](http://lxr.free-electrons.com/source/include/linux/remoteproc.h). The id member should be set to one of the standard virtio device IDs to identify the device type. Dependent on the device, it will require a number of vring structures for passing data between Linux and firmware. These are described here. Linux will fill in the da (device address) member of each vring resource with the physical address of each vring that the kernel has allocated. The vring communication is decribed in section "Virtio".

### Device Memory
These records specify that an IO region of memory, for example a peripheral, be mapped into the remote processors memory space. The format is specified in [remoteproc.h](http://lxr.free-electrons.com/source/include/linux/remoteproc.h). In the case of the MIPS remote processor driver, the remote CPU has access to the full virtual address space anyway, so these records are not currently handled. If the firmware wishes to access a region of IO memory, it may access it directly via a KSEG1 address. Of course, care must be taken to ensure that the Linux kernel is not also accessing that peripheral.

### Trace Buffers
These records specify a memory region that the firmware will write to with trace information. The format is specified in [remoteproc.h](http://lxr.free-electrons.com/source/include/linux/remoteproc.h). The da (device address) member should be set to the virtual address within the remote processor of the buffer. The kernel will convert that into an offset within one of the memory carveout regions and access that buffer directly. If CONFIG_DEBUGFS is enabled, then a debugfs file will be created for each trace entry. These entries can be read, for example:
`# cat /sys/kernel/debug/remoteproc/remoteproc0/trace0`

## Exception / Interrupt Handling
To handle interrupts within the firmware, it must be able to point the exception base address (Coprocessor 0 EBASE register) into the memory region used by the remote processor. This relies on 2 features of the MIPS architecture being available in the target CPU. First, the EBASE register must be present (older MIPS CPUs used a fixed EBASE of 0x80000000). Secondly, the WG (write-gate) flag in the EBASE register must be available. Without this flag, the top 2 bits (MIPS32) of EBASE are read-only and 0b10, to ensure that EBASE is always within KSEG0. When the WG flag is implemented, these top bits may also be written, to point EBASE anywhere in virtual memory space - this is essential to be able to handle exceptions and interrupts within firmware running from mapped memory (KUSEG).
If the CPU supports the EBASE register and WG flag, then it should be written with the value of the exception handler routines implemented by the firmware.

## IPIs
Each MIPS remote processor uses 2 Inter-Processor Interrupts. One is used to interrupt the firmware when a message is available on one of its virtio vrings. The other is used to interrupt Linux when the firmware has made a message available to it. The interrupt numbers are passed as arguments(in registers a1 and a2 respectively). If these 2 interrupts are provided by the MIPS GIC, they are GIC shared interrupts and their number is the argument minus the number of local GIC interrupts, 7.

## Virtio
The virtio framework is described in [virtio specfication](http://docs.oasis-open.org/virtio/virtio/v1.0/csprd01/virtio-v1.0-csprd01.html). The firmware provides in its resource table a virtio device record and as many vring records as necessary for that device. The firmware fills in the required alignment, number of entries and notifyids of each vring. The Linux kernel will then allocate the necessary structures. Each of the vrings is mapped into the remote processors address space (KUSEG) using wired TLB entries. The kernel fills in the da field of the vring descriptor in the resource table with the address at which the vring has been mapped.

![VPE usage](https://github.com/MIPS/mips-rproc-example/blob/master/img/virtio.png)

### Sending a message to the firmware
When Linux has a message to send to the firmware, it places it in the available ring within the vring. The descriptor at the available index is updated such that it's address and length point to the outgoing buffer (the physical address of the buffer). The available index is then incremented. Note that the available index wraps at 65536, not the number of descriptors in the ring. The firmware is then interrupted to handle the buffer.
Once the firmware has handled the buffer of data, it places the descriptor index on the used ring at the used ring index, increments the index and sends an interrupt to Linux. Linux then frees the buffer.

![VPE usage](https://github.com/MIPS/mips-rproc-example/blob/master/img/vring.png)

The figure shows a vring where Linux has made available a buffer in memory, and placed the pointer to that buffer in descriptor index 2. The next index in the available ring was 3, so descriptor ID 2 is placed in the available ring index 3. The available index was then incremented to 4. After the firmware has processed this buffer, it places descriptor index 2 in the used ring at the next available index, which was 0 and then increments the used index to 1.

###  Sending a message to Linux
When Linux initialises the outgoing vring, it allocates buffers for each descriptor in the available ring. The firmware writes the message into one of the descriptor buffers and writes it's index on the used ring at the used ring index. It then increments the index and sends an interrupt to Linux. Linux processes the message in the buffer, frees it and replaces it with a new buffer.

# Conclusions
As we have seen the MIPS remote processor driver provides the framework for a system to flexibly implement a mix of real-time and Linux based processes on a single MIPS based platform. The ability to mix multiple cores and multiple threads is particularly suited to the hardware multi-threading available in MIPS interAptiv and Warrior cores. The MIPS remote processor driver enables a simplification of system design, and an increase in performance and flexibility of the system. It may be particularly beneficial to systems where the real time application requirements or general data processing and networking requirements are periodic, although it is equally beneficial for systems with a continuous mix of real-time and Linux based operations.
