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

#include <trace.h>

static int trace_pos = 0;

char trace_buf[TRACE_BUFFER_SIZE];

void trace_clear(void)
{
	int i;
	for (i = 0; i < sizeof(trace_buf); i++)
		trace_buf[i] = 0;
	trace_pos = 0;
}

void trace_putc(char c)
{
	trace_buf[trace_pos] = c;
	if (++trace_pos >= sizeof(trace_buf))
		trace_pos = 0;
}
