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

#define POLLED_MODE 1

/*
 * If your kernel is configured to use coherent, or per-device DMA, set this to 1.
 * If the kernel is using coherent DMA, it will access shared buffers cached,
 * and the firmware must do the same to see consistent data.
 * If the kernel is configured for non-coherent DMA, it will access shared buffers
 * uncached, so the firmware must do the same to see consistent data.
 */
#define DMA_COHERENT 1

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
		__asm__("sync");
		__asm__("ehb");
	}
#endif


	__asm__("mfc0 %0, $12, 0" : "=r" (flags));
#if POLLED_MODE == 0
	/* Enable interrupts! */
	flags |= 1 << 10; /* IM2 */
	flags |= 1; /* IE */
#else
	flags &= ~1; /* IE */
#endif /* POLLED_MODE */
	__asm__("mtc0 %0, $12, 0" : : "r" (flags));
	__asm__("ehb");
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

#define NUM_LEDS 144

static uint8_t rgb_data[NUM_LEDS * 3];

#define GPIO_BIT_EN                    0x00
#define GPIO_OUTPUT_EN                 0x04
#define GPIO_OUTPUT                    0x08

/* Ci40 GPIO base address */
static volatile void* gpio_base = (void*)(0xb8101e00);
static const int WS2812 = 14; /* GPIO14 - pin 8 on Ci40 expansion header */

static inline void writel(u32 val, void* reg)
{
	*(volatile u32*)reg = val;
}


static inline void gpio_writel(u32 gpio, u32 r, u32 val)
{
	void *reg = gpio_base + (0x24 * (gpio / 16)) + r;
       /*
        * For most of the GPIO registers, bit 16 + X must be set in order to
        * write bit X.
        */
       writel((0x10000 | val) << (gpio % 16), reg);
}

static void mips_ws2812_enable(void)
{
       gpio_writel(WS2812, GPIO_OUTPUT, 0);
       gpio_writel(WS2812, GPIO_OUTPUT_EN, 1);
       gpio_writel(WS2812, GPIO_BIT_EN, 1);
}

static long clock = 546000000; /* 2ns / clock */

#define NS_TO_LOOPS(x) (x / 4) /* 2ns / clock, count incremented every 2 cycles */


#define __read_32bit_c0_register(source, sel)                           \
({ unsigned int __res;                                                  \
        if (sel == 0)                                                   \
                __asm__ __volatile__(                                   \
                        "mfc0\t%0, " #source "\n\t"                     \
                        : "=r" (__res));                                \
        else                                                            \
                __asm__ __volatile__(                                   \
                        ".set\tmips32\n\t"                              \
                        "mfc0\t%0, " #source ", " #sel "\n\t"           \
                        ".set\tmips0\n\t"                               \
                        : "=r" (__res));                                \
        __res;                                                          \
})

#define __write_32bit_c0_register(register, sel, value)			\
do {									\
	if (sel == 0)							\
		__asm__ __volatile__(					\
			"mtc0\t%z0, " #register "\n\t"			\
			: : "Jr" ((unsigned int)(value)));		\
	else								\
		__asm__ __volatile__(					\
			".set\tmips32\n\t"				\
			"mtc0\t%z0, " #register ", " #sel "\n\t"	\
			".set\tmips0"					\
			: : "Jr" ((unsigned int)(value)));		\
} while (0)

#define read_c0_count()         __read_32bit_c0_register($9, 0)
#define write_c0_compare(val)	__write_32bit_c0_register($11, 0, val)
#define READ_COUNT __asm__

static void ws2812_delay(unsigned int len)
{
	unsigned int target = read_c0_count() + len;

	while (read_c0_count() < target);
}

static void ws2812_drive_led_bit(unsigned bit)
{
       gpio_writel(WS2812, GPIO_OUTPUT, 1);
       if (bit == 0) {
	       ws2812_delay(NS_TO_LOOPS(350));
	       gpio_writel(WS2812, GPIO_OUTPUT, 0);
               ws2812_delay(NS_TO_LOOPS(800));
       } else {
               ws2812_delay(NS_TO_LOOPS(700));
	       gpio_writel(WS2812, GPIO_OUTPUT, 0);
               ws2812_delay(NS_TO_LOOPS(450));
       }
}

static void ws2812_drive_led(u8 value)
{
       int bit;
//       printf("LED->%d\n", value);
#ifdef TIMING_TEST
       ws2812_drive_led_bit(0);
       //ws2812_drive_led_bit(1);
#else
       for (bit = 7; bit >= 0; bit--)
               ws2812_drive_led_bit((value >> bit) & 1);
#endif
}

static void ws2812_drive(void)
{
       int led;
       unsigned long flags;

	__asm__(".set	push\n"
		".set	mt\n"
		"dvpe\n"
		".set pop\n"
	);

#ifdef TIMING_TEST
       ws2812_drive_led(0);
#else
       for (led = 0; led < (NUM_LEDS*3); led+=3)
       {
               u8 r = rgb_data[led+0];
               u8 g = rgb_data[led+1];
               u8 b = rgb_data[led+2];

	       ws2812_drive_led(b);
	       ws2812_drive_led(g);
               ws2812_drive_led(r);


       }
#endif

	__asm__(".set	push\n"
		".set	mt\n"
		"evpe\n"
		".set pop\n"
	);
}


#define HUE_DEGREE    512

hsvtorgb(unsigned int *r, unsigned int *g, unsigned int *b, unsigned int h, unsigned int s, unsigned int v)
{
	h = h * HUE_DEGREE;
	if(s == 0) {
		*r = *g = *b = v;
	} else {
		int i = h / (60*HUE_DEGREE);
		int p = (256*v - s*v) / 256;

		if(i & 1) {
		int q = (256*60*HUE_DEGREE*v - h*s*v + 60*HUE_DEGREE*s*v*i) / (256*60*HUE_DEGREE);
		switch(i) {
			case 1:   *r = q; *g = v; *b = p; break;
			case 3:   *r = p; *g = q; *b = v; break;
			case 5:   *r = v; *g = p; *b = q; break;
		}
		} else {
		int t = (256*60*HUE_DEGREE*v + h*s*v - 60*HUE_DEGREE*s*v*(i+1)) / (256*60*HUE_DEGREE);
		switch(i) {
			case 0:   *r = v; *g = t; *b = p; break;
			case 2:   *r = p; *g = v; *b = t; break;
			case 4:   *r = t; *g = p; *b = v; break;
		}
		}
	}
}


void main(int fw_arg0, int fw_arg1, int fw_arg2, int fw_arg3)
{
	unsigned int i, r, g, b, h, s, v;
	volatile int loop = 0;
	int shift = 0;
	/*
	 * Initialise the incoming and outgoing vrings from
	 * value passed to us in the resource table
	 */
	vring_init(&vring_outgoing, &resource_table.vdev.vring[0]);
	vring_init(&vring_incoming, &resource_table.vdev.vring[1]);

	/* Set up the GIC */
	configure_interrupts(fw_arg1, fw_arg2);

	mips_ws2812_enable();

	h = 0;
	s = 0xff;
	v = 0xff;

	while(1) {
		if (++loop >= 4000000) {
			loop = 0;
			if (++h >= 359)
				h = 0;

			hsvtorgb(&r, &g, &b, h, s, v);

			for (i = 0; i < NUM_LEDS * 3; i += 3)
			{
				rgb_data[i+0] = r >> shift;
				rgb_data[i+1] = g >> shift;
				rgb_data[i+2] = b >> shift;

				shift++;
				if (shift > 7)
					shift = 0;
			}
			ws2812_drive();

			shift++;
			if (shift > 7)
				shift = 0;

		}
#if POLLED_MODE == 1
		//check_and_handle_incoming_buffers();
#else
		//__asm__("wait");
#endif /* POLLED_MODE */
	}
}

int putchar(char c)
{
	/* Printf should be directed to the trace buffer */
	trace_putc(c);
}
