# Software description

## head.S
Handles the startup of the firmware running on the CPU. It sets the CPUs EBASE register to the value of _exception_vector, a symbol defined in the linker script set to the base of the firmware image (0x10000000). Next it sets the stack pointer to the value of _stack_top, another symbol defined in the linker script above space reserved for the stack. Finally the bss section is cleared to 0. This uses the _bss_start and _bss_end symbols from the linker script to get the memory range. With all set up complete, it jumps to main().

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