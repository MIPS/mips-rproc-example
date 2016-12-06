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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fd_port;

static void print_usage_exit(char *name)
{
	printf("Usage: %s <-l> -p <port>\n", name);
	printf("  -l Activate a test loop\n");
	printf("  -p <port> Port is the virtio port created for the remote target like /dev/vport0p0\n");

	exit(-1);
}

static void echo_request(char *out_buf, int out_len)
{
	int in_len, in_total = 0;
	char *in_buf = alloca(out_len);

	printf("Sending '%s'\n", out_buf);
	if (write(fd_port, out_buf, out_len) != out_len) {
		perror("Error writing to port\n");
		exit(-1);
	}

	for (in_total = in_len = 0; in_total < out_len; in_total += in_len) {
		fd_set set;
		struct timeval timeout = {
			.tv_sec = 1,
		};

		FD_ZERO(&set);
		FD_SET(fd_port, &set);

		switch(select(fd_port + 1, &set, NULL, NULL, &timeout))
		{
		case -1:
			perror("Select");
			exit(-1);
		case (0):
			printf("Timeout waiting for response\n");
			exit(-1);
		default:
			break;
		}

		in_len = read(fd_port, &in_buf[in_total], sizeof(in_buf));
		if (in_len <= 0) {
			perror("Error reading from port\n");
			exit(-1);
		}
	}
	in_buf[in_total] = '\0';
	printf("Received '%s'\n", in_buf);
}

static void test_loop(int iterations)
{
	int i;
	char buf[256];

	printf("Looping %d times\n", iterations);

	for (i = 0; i < iterations; i++) {
		sprintf(buf, "Test %d", i);
		echo_request(buf, strlen(buf));
	}
}

static void test_interactive(void)
{
	int len;
	char buf[256];

	while(1) {
		printf("> ");
		fflush(stdout);

		len = read(STDIN_FILENO, buf, sizeof(buf));
		if (len <= 0) {
			perror("Reading stdin");
			exit(-1);
		}
		buf[--len] = '\0'; /* Remove trailing \n */

		echo_request(buf, len);
	}
}

int main(int argc, char*argv[])
{
	int c;
	int loop = 0;
	const char *port = NULL;


	opterr = 0;
	while ((c = getopt (argc, argv, "l:p:")) != -1)
	switch (c)
	{
	case 'l':
		loop = atoi(optarg);
		break;
	case 'p':
		port = optarg;
		break;
	default:
		print_usage_exit(argv[0]);
	}

	fd_port = open(port, O_RDWR);
	if (fd_port < 0) {
		perror("Couldn't open port");
		print_usage_exit(argv[0]);
	}

	if (loop)
		test_loop(loop);
	else
		test_interactive();
}
