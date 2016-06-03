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

#include <printf.h>
#include <vring.h>

void vring_init(struct vring *vring, volatile struct fw_rsc_vdev_vring *rsc)
{
	int used;

	vring->num_descriptors = rsc->num;
	vring->desc = (void*)rsc->da;
	vring->avail = (void*)rsc->da + rsc->num * sizeof(struct vring_desc);

	used = (int)vring->avail + sizeof(struct vring_avail) + rsc->num * sizeof(u16);
	vring->used = (void*)((used + rsc->align) & ~(rsc->align-1));
}

void vring_print(struct vring *vring)
{
	int i, j;

	printf("vring at 0x%08x\n", (int)vring);
	printf(" avail_index: %d\n", vring->avail_index);
	printf(" used_index: %d\n", vring->used_index);

	printf("\n descriptors:\n");
	for (i = 0; i < vring->num_descriptors; i++) {
		struct vring_desc *desc = &vring->desc[i];
		printf("  desc %d (0x%08x)\n", i, desc);
		printf("   address: 0x%08x\n", (int)desc->address);
		printf("   length: 0x%x\n", desc->length);
		printf("   flags: 0x%04x\n", desc->flags);
		printf("   next: 0x%04x\n", desc->next);
	}

	printf("\n avail (0x%x)\n", vring->avail);
	printf("  flags: 0x%04x\n", vring->avail->flags);
	printf("  index: 0x%04x\n", vring->avail->index);
	for (i = 0; i < vring->num_descriptors; i++) {
		printf("   %d: 0x%x\n", i, vring->avail->ring[i]);
	}

	printf("\n used (0x%x)\n", vring->used);
	printf("  flags: 0x%04x\n", vring->used->flags);
	printf("  index: 0x%04x\n", vring->used->index);
	for (i = 0; i < vring->num_descriptors; i++) {
		printf("   %d: 0x%x\n", i, vring->used->ring[i].index);
	}
}

int vring_get_buffer(struct vring *vring, void **buf, int *length)
{
	if (vring->avail_index != vring->avail->index) {
		int avail_index = vring->avail_index & (vring->num_descriptors - 1);
		int desc_index = vring->avail->ring[avail_index];
		struct vring_desc *desc = &vring->desc[desc_index];

		printf("avail ring %d, desc %d available\n", avail_index, desc_index);
		printf("  address: 0x%08x\n", (int)desc->address);
		printf("  length: 0x%x\n", desc->length);
		printf("  flags: 0x%04x\n", desc->flags);
		printf("  next: 0x%04x\n", desc->next);

		*buf = (void*)(long)desc->address;
		*length = desc->length;

		vring->avail_index++;
		return 1;
	}
	return 0;
}

int vring_put_buffer(struct vring *vring, void *buf, int length)
{
	int i;

	for (i = 0; i < vring->num_descriptors; i++) {
		struct vring_desc *desc = &vring->desc[i];
		if ((void*)(long)desc->address == buf) {
			int index = vring->used_index & (vring->num_descriptors - 1);
			printf("desc %d is used\n", i);

			vring->used->ring[index].index = i;
			vring->used->ring[index].length = length;

			/* Barrier before updating the index */
			__asm__("ehb");
			__asm__ __volatile__("sync" : : :"memory");

			vring->used_index++;
			vring->used->index = vring->used_index;
			printf("used ring %d = desc %d\n", index, i);
			printf("used ring index = %d\n", vring->used->index);
			return 1;
		}
	}
	return 0;
}
