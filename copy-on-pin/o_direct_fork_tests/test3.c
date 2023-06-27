/*
 * This test case triggers taking a short-term R/W reference on an anonymous
 * page for DMA (read from device, write to page device) in the parent process,
 * followed by sharing of the page via fork() with a child process.
 *
 * The parent process then modifies an unrelated part of the shared page,
 * possibly concurrent with DMA.
 *
 * On problematic OS implementations of COW and fork(), the data read from
 * the device will be lost, because the write access replaces the page in
 * the page table when breaking COW. Also, we might be able to trigger
 * unexpected behavior in the OS.
 *
 * This usually reproduced fairly easily.
 *
 *
 * Sequence #1: Short-term R/W reference in parent before fork:
 *
 * 1) P0/T1: R/W reference
 * 2) P0/T0: fork()
 * 3) P0/T0: Write access
 * 4) P0/T1: DMA, unreference
 *
 * If 2) ends up sharing the page, 3) will break COW and the DMA in 4)
 * will be lost.
 *
 * A possible fix would be to not share the page during 2).
 *
 * Sequence #2: Short-term R/W reference in parent concurrent with fork:
 *
 * Corner-case of #1 that might theoretically be problematic with advanced
 * locking, for example, under Linux that supports lockless GUP, whereby
 * basically no locks are taken.
 *
 *
 * Copyright 2022 Red Hat, Inc.
 * Author(s): David Hildenbrand <david@redhat.com>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define BLKSIZE 512

size_t pagesize;
unsigned char *page;
int fd;

static int memtest(const unsigned char * const mem, unsigned char data, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++) {
		if (mem[i] != data) {
			fprintf(stderr, "mem[%zu] != 0x%x, mem[%zu] == 0x%x\n",
				i, data, i, mem[i]);
			return 1;
		}
	}
	return 0;
}

static void *thread_fn(void *arg)
{
	while (true) {
		/* Start with a clean slate. */
		memset(page, 0x0, pagesize);

		/* Read from device and write to the page. */
		if (pread(fd, page, BLKSIZE, 0) != BLKSIZE) {
			fprintf(stderr, "pread(BLKSIZE) failed\n");
			continue;
		}

		/* Verify the right values ended up in our buffer. */
		if (memtest(page, 0xff, BLKSIZE)) {
			fprintf(stderr, "Wrong data read\n");
			exit(1);
		}
	}

	return NULL;
}

int main(int argc, char **argv)
{
	unsigned int flags = O_CREAT|O_RDWR|O_TRUNC;
	bool error = false;
	pthread_t thread;

#ifdef O_DIRECT
	flags |= O_DIRECT;
	if (argc == 3) {
		if (!strcmp(argv[2], "0") || !strcmp(argv[2], "n") || !strcmp(argv[2], "N"))
			flags &= ~O_DIRECT;
		else if (strcmp(argv[2], "1") && strcmp(argv[2], "y") && strcmp(argv[2], "Y"))
			error = true;
	}

	if (argc < 2 || argc > 3 || error) {
		fprintf(stderr, "Usage: %s FILE [USE_O_DIRECT]\n", argv[0]);
		return 1;
	}

	fprintf(stderr, "Using '%s' %s O_DIRECT.\n", argv[1],
		(flags & O_DIRECT) ? "with" : "without");
#else
	if (argc != 2 || error) {
		fprintf(stderr, "Usage: %s FILE\n", argv[0]);
		return 1;
	}

	fprintf(stderr, "Using '%s'. O_DIRECT is not supported.\n", argv[1]);
#endif /* O_DIRECT */

	/* We use a single target page. */
	pagesize = getpagesize();
	page = mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
		    -1, 0);
	if (page == MAP_FAILED) {
		fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
		return 1;
	}

	/* Open our file and store our expected values. */
	fd = open(argv[1], flags, 0600);
	if (fd < 0) {
		fprintf(stderr, "open() failed: %s\n", strerror(errno));
		return 1;
	}
	memset(page, 0xff, BLKSIZE);
	if (pwrite(fd, page, BLKSIZE, 0) != BLKSIZE) {
		fprintf(stderr, "pwrite(BLKSIZE) failed\n");
		return 1;
	}

	/* Quickly verify that the file content is as expected. */
	memset(page, 0x0, pagesize);
	if (pread(fd, page, BLKSIZE, 0) != BLKSIZE) {
		fprintf(stderr, "pread(BLKSIZE) failed\n");
		return 1;
	}
	if (memtest(page, 0xff, BLKSIZE)) {
		fprintf(stderr, "Unexcpected file content read\n");
		return 1;
	}

	/* Start with a clean slate. */
	memset(page, 0x0, pagesize);

	if (pthread_create(&thread, NULL, thread_fn, NULL)) {
		fprintf(stderr, "pthread_create() failed\n");
		return 1;
	}

	while (true) {
		volatile unsigned char *ptr = &page[pagesize - 1];
		int ret;

		/*
		 * Optimized fork() will share anonymous pages between both
		 * processes via COW.
		 */
		ret = fork();
		if (ret < 0) {
			fprintf(stderr, "fork() failed: %s\n", strerror(errno));
			return 1;
		} else if (!ret) {
			/*
			 * Just stay alive for a while such that the anonymous
			 * page will remain shared.
			 */
			usleep(random() % 1000);
			return 0;
		}

		/*
		 * Modify a part of the page that's not expected to be
		 * modified via DMA.
		 */
		(*ptr)++;

		wait(&ret);
		if (!WIFEXITED(ret)) {
			fprintf(stderr, "wait() failed: %s\n", strerror(errno));
			return 1;
		}
	}

	return 0;
}
