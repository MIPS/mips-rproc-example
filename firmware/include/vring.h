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

#ifndef _VRING_H_
#define _VRING_H_

#include <stdint.h>

#include <asm/remoteproc.h>

/* Virtio ring descriptor */
struct vring_desc {
	uint64_t address;		/* Physical address of buffer */
	uint32_t length;		/* Length of buffer */
	uint16_t flags;
	uint16_t next;
};

/* Virtio available ring */
struct vring_avail {
	uint16_t flags;
	uint16_t index;			/* Current ring index */
	uint16_t ring[];		/* Ring of descriptor indexes */
} __packed;

/* Used ring entry */
struct vring_used_entry {
	uint32_t index;			/* Descriptor index */
	uint32_t length;		/* Length of buffer used */
} __packed;

/* Virtio used ring */
struct vring_used {
	uint16_t flags;
	uint16_t index;			/* Current ring index */
	struct vring_used_entry ring[];
} __packed;

struct vring {
	unsigned int num_descriptors;
	uint16_t avail_index;		/* Local shadow of available ring index */
	uint16_t used_index;		/* Local shadow of used ring index */

	struct vring_desc *desc;	/* ring descriptor array */

	struct vring_avail *avail;	/* Pointer to available ring */

	struct vring_used *used;	/* Pointer to used ring */
};


/*
 * Initialise a struct vring from the provided fw resource
 * \param vring	vring to initialise
 * \param rsc	resource table entry for the vring
 */
void vring_init(struct vring *vring, volatile struct fw_rsc_vdev_vring *rsc);

/*
 * Print the vring state
 */
void vring_print(struct vring *vring);

/*
 * Get the next available buffer from the vring.
 * The vrings available index is incremented so this buffer
 * will not be returned again. The buffer must subsequently
 * be returned to the host via the used ring (vring_put_buffer)
 * \param vring	ring containing buffer
 * \param buf	Contents will be updated with the pointer to the buffer
 * \param len	Contents will be updated with the buffer length
 * \return non-zero when buffer is available and returned or 0 if no buffer.
 */
int vring_get_buffer(struct vring *vring, void **buf, int *len);

/*
 * Put a previously retrieved buffer (vring_get_buffer) onto the used ring
 * The buffer is found in the vring buffer descriptors and the next entry
 * in the used ring updated to that descriptor index.
 * The vrings used index is then incremented.
 * \param vring	ring containing buffer
 * \param buf	Pointer to the buffer
 * \param len	Length of the buffer being returned. Note that in the case of
 * 		outgoing buffers where the host has provided memory to be written,
 * 		the length will be the amount of data actually written.
 * \return non-zero when buffer has been returned or 0 on error.
 */
int vring_put_buffer(struct vring *vring, void *buf, int len);

#endif /* _VRING_H_ */
