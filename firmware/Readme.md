This example MIPS remote proc firmware implements a virtio serial port which receives strings, case inverts them, and writes them back. There is also a Linux userspace program which opens the virtual serial port and exchanges messages with the firmware.
The firmware can be configured to either poll for incoming data (which may be preferable in the case of the main loop doing real time processing), or to be interrupted when data is available.

## head.S
Handles the startup of the firmware running on the CPU. It sets the CPUs EBASE register to the value of _exception_vector, a symbol defined in the linker script set to the base of the firmware image (0x10000000). Next it sets the stack pointer to the value of _stack_top, another symbol defined in the linker script above space reserved for the stack. Finally the bss section is cleared to 0. This uses the _bss_start and _bss_end symbols from the linker script to get the memory range. With all set up complete, it jumps to main().

## main.c
This file contains the main implementation, and the resource table. The resource table is placed in the special ELF section ".resource_table", where the remote processor core code will find it. The resource table specifies
- A carveout region covering the firmware location in memory
- A trace buffer for debug
- A Virtio serial port vdev with 2 vrings
Within the main() function, first the internal vring structures for the incoming and outgoing rings are initialised using values that Linux has filled in in the resource table.
The interrupts are then configured. This involves finding the address of the GIC from the CM (which first has to be found using the CP0 register CMGGRBase). From this the address of the relevant pending register for the incoming interrupt can be determined. If POLLED_MODE is defined to 0, then here the incoming interrupt will be unmasked. All local interrupts, such as the timer, that Linux may have left unmasked, are disabled before enabling global interrupts.
When the incoming interrupt flag is detected, either by polling for it when POLLED_MODE is defined to 1, or in processing the resultant interrupt, the incoming vring is inspected for newly available buffers. Each one found is handed to the handle_buffer() function.
The handle_buffer function gets an available from the buffer from the outgoing vring and copies the incoming data to it, while case converting ASCII alphabetical characters. The outgoing buffer is then placed in the used ring of the outgoing vring. The incoming buffer is placed in the used ring of the incoming vring. Linux is then signaled by asserting the IRQ flag associated with the firmware to Linux interrupt.
Linux will then free the used buffer that it made available to the firmware, and handle the incoming buffer from the firmware.

## printf.c
A simple printf implementation, used with the trace buffer.

## trace.c
The printf implementation is directed to output characters into the trace_buf buffer. This buffers address is associated with the trace entry in the resource table. If Linux is configured with CONFIG_DEBUGFS, then the remote processor core code will create a debugfs file, which when read will read the string contained in this buffer.

## vring.c
This file contains generic functions for dealing with vrings.
### vring_init
This function intialises the internal struct vring representation from the values that have been provided to the firmware by Linux in the resource table.
###  vring_print
A debug function to print the state of a vring
###  vring_get_buffer
This function attempts to retrieve a buffer of data from a vring. It inspects the vring_avail structure in memory shared with Linux and compares the index member with a local copy. If Linux has made a buffer available, the index will have been incremented. The available index indicates which descriptor index contains the new data. A pointer to the buffer of data and it's length can then be found in the indicated descriptor. The pointer is a pointer into physical memory, so this must be mapped into addressable virtual memory first. The code in handle_buffer gets a KSEG0 address for the buffer to access it without needing a TLB entry for it.
##  vring_put_buffer
This function marks a buffer of data in a vring as used. It looks for the buffer pointer in one of the descriptors. When it finds the descriptor, it places it's index and length in the used ring at the used index. The used index is then incremented.