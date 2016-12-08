This example MIPS remote proc firmware implements a virtio serial port which receives strings, case inverts them, and writes them back. There is also a Linux userspace program which opens the virtual serial port and exchanges messages with the firmware.
The firmware can be configured to either poll for incoming data (which may be preferable in the case of the main loop doing real time processing), or to be interrupted when data is available.

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
