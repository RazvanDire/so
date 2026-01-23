/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <stdio.h>
#include "printf.h"

#define DIE(assertion, call_description)									\
	do {													\
		if (assertion) {										\
			fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);					\
			perror(call_description);								\
			exit(errno);										\
		}												\
	} while (0)



/* Structure to hold memory block metadata */
struct block_meta {
	size_t size;
	int status;
	struct block_meta *prev;
	struct block_meta *next;
};

void *prealloc_heap(void);
void *find_best(size_t size);
void split_block(struct block_meta *addr, size_t size);
void coalesce_blocks(struct block_meta *addr);
void coalesce_all(void);
void add_block(struct block_meta *addr, size_t size, int status);
void remove_block(struct block_meta *addr);
void *last_heap_block(void);

/* Block metadata status values */
#define STATUS_FREE   0
#define STATUS_ALLOC  1
#define STATUS_MAPPED 2

#define METADATA_SIZE		(sizeof(struct block_meta))
#define MMAP_THRESHOLD		(128 * 1024)
