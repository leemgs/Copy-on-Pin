/*
 * This test case triggers taking a short-term R/O reference on an anonymous
 * page for DMA (read from page, write to device) in the child process after
 * fork.
 *
 * The child process then modifies and unrelated part of the shared page,
 * followed by the parent process storing a secret in the page. Both actions
 * can possibly run concurrent to DMA.
 *
 * On problematic OS implementations of COW and fork(), the data ending up
 * on the device isn't the expected data, however, (sensitive?) data from
 * the parent process or corrupted data. Also, we might be able to trigger
 * unexpected behavior in the OS.
 *
 * This usually takes a long time to reproduce.
  *
 * 1) P0/T0: fork()
 * 2) P1/T0: R/O reference
 * 3) P1/T1: Write access
 * 4) P0/T0: Write access
 * 5) P1/T1: DMA, unreference
 *
 * If 2) doesn't break COW, 3) will break COW and 4) might reuse/modify the page
 * that's still under DMA. Wrong (sensitive?) data from the parent ends up in
 * the file.
 *
 * Possible fixes would be either to
 * a) Break COW during 2)
 * b) Not reuse a page during 4) that has unknown references
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
#include <aio.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define BLKSIZE 512

volatile int *shared;
unsigned char *page;
size_t pagesize;
int fd;

static void barrier(void)
{
	asm volatile("": : :"memory");
}

static void *write_thread_fn(void *arg)
{
	volatile unsigned char *ptr = &page[pagesize - 1];

	*shared = 1;
	barrier();
	while (*shared != 2)
		barrier();

	barrier();
	(*ptr)++;
	barrier();
	*shared = 3;
	return 0;
}

static int child_fn(void)
{
	pthread_t thread;
	char *tmp;
	int ret;

	tmp = mmap(0, pagesize, PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_PRIVATE, -1, 0);
	if (tmp == MAP_FAILED) {
		fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
		return 1;
	}

	/* Allocate sufficient file blocks. */
	if (pwrite(fd, tmp, pagesize, 0) != pagesize) {
		fprintf(stderr, "pwrite() failed\n");
		return 1;
	}

	/* Start a thread that will modify an unrelated part of the page. */
	if (pthread_create(&thread, NULL, write_thread_fn, &fd)) {
		fprintf(stderr, "pthread_create() failed\n");
		return 1;
	}

	/* Wait until the thread is ready. */
	while (*shared != 1)
		barrier();
	barrier();
	*shared = 2;
	barrier();

	/* Read from the page and write to device. */
	if (pwrite(fd, page, BLKSIZE, 0) != BLKSIZE) {
		fprintf(stderr, "pwrite() failed\n");
		return 1;
	}

	/* Read back the content to verify it's as expected. */
	if (pread(fd, tmp, BLKSIZE, 0) != BLKSIZE) {
		fprintf(stderr, "pread() failed\n");
		return 1;
	}

	if (strcmp(tmp, "Boring Data")) {
		fprintf(stderr, "Wrong data read: %s\n", tmp);
		return 1;
	}

	ret = pthread_join(thread, NULL);
	if (ret) {
		fprintf(stderr, "pthread_join() failed\n");
		return 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	unsigned int flags = O_CREAT|O_RDWR|O_TRUNC;
	bool error = false;

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

	pagesize = getpagesize();

	/*
	 * To reproduce, we have to squeeze two write operations from two
	 * threads in the right sequence into the race window. Let's sync up
	 * the two writes to make it easier to reproduce.
	 */
	shared = mmap(0, pagesize, PROT_READ | PROT_WRITE,
		      MAP_ANON | MAP_SHARED, -1, 0);
	if (shared == MAP_FAILED) {
		fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
		return 1;
	}

	fd = open(argv[1], flags, 0600);
	if (fd < 0) {
		fprintf(stderr, "open() failed: %s\n", strerror(errno));
		return 1;
	}

	while (true) {
		int ret;

		page = mmap(0, pagesize, PROT_READ | PROT_WRITE,
			    MAP_ANON | MAP_PRIVATE, -1, 0);
		if (page == MAP_FAILED) {
			fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
			return 1;
		}

		barrier();
		strcpy((char *)page, "Boring Data");
		*shared = 0;
		barrier();

		/*
		 * Optimized fork() will share anonymous pages between both
		 * processes via COW.
		 */
		ret = fork();
		if (ret < 0) {
			fprintf(stderr, "fork() failed: %s\n", strerror(errno));
			return 1;
		} else if (!ret) {
			return child_fn();
		}

		while(*shared != 3)
			barrier();
		strcpy((char *)page, "Sectret Data");

		wait(&ret);
		if (!WIFEXITED(ret)) {
			fprintf(stderr, "wait() failed: %s\n", strerror(errno));
			return 1;
		}
		if (WEXITSTATUS(ret))
			return WEXITSTATUS(ret);

		munmap(page, pagesize);
		page = NULL;
	}

	return 0;
}
