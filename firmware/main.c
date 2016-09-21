/*
 * Copyright (c) 2016, Imagination Technologies LLC and Imagination
 * Technologies Limited.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions in binary form must be built to execute on machines
 *    implementing the MIPS32(R), MIPS64 and/or microMIPS instruction set
 *    architectures.
 *
 * 2. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 3. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 4. Neither the name of Imagination Technologies LLC, Imagination
 *    Technologies Limited nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL IMAGINATION TECHNOLOGIES LLC OR
 * IMAGINATION TECHNOLOGIES LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define POLLED_MODE 0

/*
 * If your kernel is configured to use coherent DMA, set this to 1.
 * If the kernel is using coherent DMA, it will access shared buffers cached,
 * and the firmware must do the same to see consistent data.
 * If the kernel is configured for non-coherent DMA, it will access shared buffers
 * uncached, so the firmware must do the same to see consistent data.
 */
#define DMA_COHERENT 0

#include <asm/remoteproc.h>
#include <printf.h>
#include <stddef.h>
#include <stdint.h>
#include <trace.h>
#include <vring.h>

#define GIC_LOCAL_INTERRUPTS 7

extern const char _start[], _end[];

/*
 * Resource table describe to remoteproc core the capabilities of
 * this firmware
 */
struct __resource_table {
	struct resource_table			header;

	uint32_t				offset[3];

	struct {
		struct fw_rsc_hdr		header;
		struct fw_rsc_carveout		carveout;
	} carveout;

	struct {
		struct fw_rsc_hdr		header;
		struct fw_rsc_trace		trace;
	} trace;

	struct {
		struct fw_rsc_hdr		header;
		struct fw_rsc_vdev		vdev;
		struct fw_rsc_vdev_vring	vring[2];
		uint8_t				config[0xc];
	} vdev;
} volatile resource_table __attribute__ ((section (".resource_table"))) = 
{
	.header = {
		.ver = 1,	/* Verison 1 */
		.num = 3,	/* 3 resources */
	},

	/* Offsets of the 3 resource headers */
	.offset[0] = offsetof(struct __resource_table, carveout),
	.offset[1] = offsetof(struct __resource_table, trace),
	.offset[2] = offsetof(struct __resource_table, vdev),

	/* Carveout resource to map firmware image into */
	.carveout = {
		.header = {
			.type = RSC_CARVEOUT,
		},
		.carveout = {
			.da = (uint32_t)&_start,
			.pa = (uint32_t)&_start,
			.len = 0x10000,//(long)(&_end) - (long)(&_start),
			.name = "firmware",
		},
	},

	/* Trace resource to printf() into */
	.trace = {
		.header = {
			.type = RSC_TRACE,
		},
		.trace = {
			.da = (uint32_t)trace_buf,
			.len = TRACE_BUFFER_SIZE,
			.name = "trace",
		},
	},

	/* Virtual device resource for the virtual serial port */
	.vdev = {
		.header = {
			.type = RSC_VDEV,
		},
		.vdev = {
			.id = 11, /* VIRTIO_ID_RPROC_SERIAL */
			.notifyid = 4,
			.config_len = 0xc,
			.num_of_vrings = 2,
		},
		
		.vring[0] = {
			.align = 0x1000,
			.num = 0x4,
			.notifyid = 1,
		},
		
		.vring[1] = {
			.align = 0x1000,
			.num = 0x4,
			.notifyid = 0,
		},
	},
};

struct vring vring_incoming;
struct vring vring_outgoing;

int interrupt_from_linux, interrupt_to_linux;

void *cm_base;
void *gic_base;

static inline void *phys_to_virt(void *phys, int cached)
{
	/* Calculate a KSEG0/KSEG1 address for a pointer */
	if (cached)
		return phys + 0xFFFFFFFF80000000;
	else
		return phys + 0xFFFFFFFFA0000000;
}

void configure_interrupts(int irq_from_host, int irq_to_host)
{
	long flags;
	int **gcr_gic_base;

	/* Determine the base address of the CM */
	__asm__("mfc0 %0, $15, 3" : "=r" (cm_base));
	cm_base = (void*)((int)cm_base << 4);
	cm_base = phys_to_virt(cm_base, 0);
	printf("CM base address: 0x%08x\n", (int)cm_base);

	/*
	 * The CM register GCR_GIC_BASE register contains the base address of
	 * the GIC - read the base address from it
	 */
	gcr_gic_base = cm_base + 0x80;
	gic_base = *gcr_gic_base;
	gic_base = phys_to_virt((void*)((int)gic_base & ~1), 0);
	printf("GIC base address: 0x%08x\n", (int)gic_base);

	/*
	 * Ensure all local interrupts (i.e the timer) are disabled
	 * Write to the GIC local reset mask register to clear all.
	 */
	*(int*)(gic_base + 0x8000 + 0xC) = 0x7F;

	/*
	 * The IPI numbers provided by Linux are offset by the number
	 * of local interrupts in the GIC
	 */
	interrupt_from_linux = irq_from_host - GIC_LOCAL_INTERRUPTS;
	interrupt_to_linux = irq_to_host - GIC_LOCAL_INTERRUPTS;

#if POLLED_MODE == 0
	/* Enable the incoming IRQ */
	{
		volatile int *gic_set_mask_reg = (int*)((int)gic_base + 0x0380 + ((interrupt_from_linux / 32) * 4));
		int gic_set_mask_bit = interrupt_from_linux % 32;

		/* Write to the GIC set mask register to enable interrupt */
		*gic_set_mask_reg = 1 << gic_set_mask_bit;
	}

	/* Enable interrupts! */
	__asm__("mfc0 %0, $12, 0" : "=r" (flags));
	flags |= 1 << 10; /* IM2 */
	flags |= 1; /* IE */
	__asm__("mtc0 %0, $12, 0" : : "r" (flags));
#endif /* POLLED_MODE */
}

/* Is the interrupt associated with linux -> remote asserted? */
int gic_irq_from_host(void)
{
	volatile int *gic_pending_reg = (int*)((int)gic_base + 0x0480 + ((interrupt_from_linux / 32) * 4));
	int gic_pending_bit = interrupt_from_linux % 32;

	if ((*gic_pending_reg) & (1 << gic_pending_bit)) {
		/* Ack the interrupt */
		volatile int *gic_wedge_reg = (int*)((int)gic_base + 0x0280);

		*gic_wedge_reg = interrupt_from_linux;

		return 1;
	}
	return 0;
}

/* Assert the interrupt associated with remote -> linux */
void gic_irq_to_host(void)
{
	volatile int *gic_wedge_reg = (int*)((int)gic_base + 0x0280);

	printf("Asserting IRQ %d\n", interrupt_to_linux);
	*gic_wedge_reg = (1 << 31) | interrupt_to_linux;
}


void handle_buffer(void *buffer, int len)
{
	uint8_t *out_buf;
	uint8_t *in_buf = phys_to_virt(buffer, DMA_COHERENT);
	int out_len;
	int i;

	/* Get a buffer in the outgoing vring */
	if (!vring_get_buffer(&vring_outgoing, &buffer, &out_len))
		return;
	printf("Got outgoing buffer length %d at 0x%08x\n", out_len, buffer);
	out_buf = phys_to_virt(buffer, DMA_COHERENT);

	printf("Incoming %d bytes at 0x%08x\n", len, (int)in_buf);
	for (i = 0; (i < len) && (i < out_len); i++) {
		printf(" 0x%02x: 0x%02x\n", i, in_buf[i]);

		/*
		 * Copy the incoming data to the outgoing buffer
		 * Swap the case of alphabetic characters
		 */
		if (in_buf[i] >= 'a' && in_buf[i] <= 'z')
			out_buf[i] = in_buf[i] - 0x20;
		else if (in_buf[i] >= 'A' && in_buf[i] <= 'Z')
			out_buf[i] = in_buf[i] + 0x20;
		else
			out_buf[i] = in_buf[i];
	}

	/* Send the outgoing buffer to the host */
	vring_put_buffer(&vring_outgoing, buffer, i);
}


void check_and_handle_incoming_buffers(void)
{
	int len;
	void *buf;

	if (gic_irq_from_host()) {
		/* Linux has asserted the incoming IPI */
		trace_clear();

		/* Handle all newly available buffers */
		while (vring_get_buffer(&vring_incoming, &buf, &len)) {
			handle_buffer(buf, len);

			vring_put_buffer(&vring_incoming, buf, len);
		}
		printf("Incoming vring:\n");
		vring_print(&vring_incoming);
		printf("Outgoing vring:\n");
		vring_print(&vring_outgoing);

		/* Send IPI to Linux to deal with consumed buffers */
		gic_irq_to_host();
	}
}

void handle_interrupt(void)
{
	check_and_handle_incoming_buffers();
}

void main(int fw_arg0, int fw_arg1, int fw_arg2, int fw_arg3)
{
	/*
	 * Initialise the incoming and outgoing vrings from
	 * value passed to us in the resource table
	 */
	vring_init(&vring_outgoing, &resource_table.vdev.vring[0]);
	vring_init(&vring_incoming, &resource_table.vdev.vring[1]);

	/* Set up the GIC */
	configure_interrupts(fw_arg1, fw_arg2);

	while(1) {
#if POLLED_MODE == 1
		check_and_handle_incoming_buffers();
#else
		__asm__("wait");
#endif /* POLLED_MODE */
	}
}

int putchar(char c)
{
	/* Printf should be directed to the trace buffer */
	trace_putc(c);
}
