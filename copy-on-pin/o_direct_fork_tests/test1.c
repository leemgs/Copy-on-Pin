/*
 * This test case triggers various sequences of taking a short-term
 * R/O reference on an anonymous page for DMA (read from page, write to
 * device) in the parent process, followed by sharing of the page via
 * fork() with the child process.
 *
 * The parent process then modifies and unrelated part of the shared page,
 * followed by the child process storing a secret in the page. Both actions
 * can possibly run concurrent to DMA.
 *
 * On problematic OS implementations of COW and fork(), the data ending up
 * on the device isn't the expected data, however, (sensitive?) data from
 * the child process or corrupted data. Also, we might be able to trigger
 * unexpected behavior in the OS.
 *
 * This usually takes a long time to reproduce.
 *
 *
 * Sequence #1: Short-term R/O reference in parent after fork:
 *
 * 1) P0/T0: fork()
 * 2) P0/T1: R/O reference
 * 3) P0/T0: Write access
 * 4) P1/T0: Write access
 * 5) P0/T1: DMA, unreference
 *
 * If 2) doesn't break COW, but 3) does, 4) might reuse/modify the page
 * that's still under DMA. Wrong (sensitive?) data from the child ends up
 * in the file.
 *
 * Possible fixes would be either to
 * a) Break COW during 2)
 * b) Not reuse a page during 4) that has unknown references
 *
 * Sequence #2: Short-term R/O reference before fork:
 *
 * 1) P0/T1: R/O reference
 * 2) P0/T0: fork()
 * 3) P0/T0: Write access
 * 4) P1/T0: Write access
 * 5) P0/T1: DMA, unreference
 *
 * If 1) ends up sharing the page, 3) will break COW and 4) might
 * reuse/modify the page that's still under DMA. Wrong (sensitive?) data
 * from the child ends up in the file.
 *
 * Possible fixes would be either to
 * a) Not share the page during 2)
 * b) Not reuse a page during 4) that has unknown references
 *
 * Sequence #3: Short-term R/O reference concurrent with fork:
 *
 * Corner-case in-between #1 and #2 that might theoretically be problematic
 * with advanced locking, for example, under Linux that supports lockless GUP,
 * whereby basically no locks are taken.
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

volatile int *shared;
size_t pagesize;
char *page;
int fd;

static void barrier(void)
{
	asm volatile("": : :"memory");
}

static void *thread_fn(void *arg)
{
	char *tmp;

	tmp = mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
		    -1, 0);
	if (tmp == MAP_FAILED) {
		fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
		exit(1);
	}

	/* Data we want to store to our file. */
	strcpy(page, "Boring data");

	while (true) {
		/* Read from the page and write to device. */
		if (pwrite(fd, page, BLKSIZE, 0) != BLKSIZE) {
			fprintf(stderr, "pwrite(BLKSIZE) failed\n");
			continue;
		}

		/* Read back the content to verify it's as expected. */
		if (pread(fd, tmp, BLKSIZE, 0) != BLKSIZE) {
			fprintf(stderr, "pread(BLKSIZE) failed\n");
			continue;
		}

		if (strcmp(tmp, "Boring data")) {
			fprintf(stderr, "Wrong data read: %s\n", tmp);
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

	pagesize = getpagesize();

	/*
	 * To reproduce, we have to squeeze two write operations from two
	 * threads in the right sqeuence into the race window. Let's sync up
	 * the two writes to make it easier to reproduce.
	 */
	shared = mmap(0, pagesize, PROT_READ | PROT_WRITE,
		      MAP_ANON | MAP_SHARED, -1, 0);
	if (shared == MAP_FAILED) {
		fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
		return 1;
	}

	/* We use a single target page. */
	page = mmap(0, pagesize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE,
		    -1, 0);
	if (page == MAP_FAILED) {
		fprintf(stderr, "mmap() failed: %s\n", strerror(errno));
		return 1;
	}

	fd = open(argv[1], flags, 0600);
	if (fd < 0) {
		fprintf(stderr, "open() failed: %s\n", strerror(errno));
		return 1;
	}

	if (pthread_create(&thread, NULL, thread_fn, NULL)) {
		fprintf(stderr, "pthread_create() failed\n");
		return 1;
	}

	while (true) {
		volatile char *ptr = &page[pagesize - 1];
		int ret;

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
			barrier();
			while(*shared != 1)
				continue;
			barrier();
			/*
			 * Let's modify the process-private page. We expect
			 * this data to not leak somewhere else.
			 */
			strcpy(page, "Secret data");
			return 0;
		}

		/*
		 * Modify a part of the page that's not expected to be
		 * written out.
		 */
		barrier();
		(*ptr)++;
		barrier();
		*shared = 1;

		wait(&ret);
		if (!WIFEXITED(ret)) {
			fprintf(stderr, "wait() failed: %s\n", strerror(errno));
			return 1;
		}
	}

	return 0;
}
