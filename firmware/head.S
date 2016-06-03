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

#define zero $0
#define t0 $13
#define t1 $14
#define sp $29

#define CP0_STATUS $12
#define ST0_CU0                 0x10000000

#define CP0_CAUSE $13
#define CP0_EBASE $15, 1

.section .text.exception_vector.reset

.globl  __start;
.type   __start, @function;
.ent    __start, 0;
__start:
	.set	push
	mfc0	t0, CP0_STATUS
	or	t0, ST0_CU0|0x1f
	xor	t0, 0x1f
	mtc0	t0, CP0_STATUS
	.set	noreorder
	ehb
	.set pop

	mtc0	zero, CP0_CAUSE

	/* Set ebase */
	la	t0, _exception_vector
	ori	t0, 1 << 11 /* Set WG to allow EBASE into mapped memory */
	mtc0	t0, CP0_EBASE

	/* Initialise stack pointer */
	la	sp, _stack_top

	/* Zero the .bss */
	la	t0, _bss_start
	la	t1, _bss_end
1:
	sw	zero, 0(t0)
	addiu	t0, t0, 4
	blt	t0, t1, 1b

	/* Off we go! */
	la	t0, main
	jr	t0

	/* Shouldn't get here */
1:
	j	1b

.end    __start;


.section .text.exception_vector.xtlb_refill

.globl  __xtlb_refill_loop;
.type   __xtlb_refill_loop, @function;
.ent    __xtlb_refill_loop, 0;
__xtlb_refill_loop:
	/* Not much to do */
1:
	j	1b

.end    __xtlb_refill_loop;


.section .text.exception_vector.cache_error

.globl  __cache_error;
.type   __cache_error, @function;
.ent    __cache_error, 0;
__cache_error:
	/* Not much to do */
1:
	j	1b

.end    __cache_error;


.section .text.exception_vector.exception

.globl  __exception;
.type   __exception, @function;
.ent    __exception, 0;
__exception:
	/* Not much to do */
	jal	handle_interrupt
	eret

.end    __exception;
