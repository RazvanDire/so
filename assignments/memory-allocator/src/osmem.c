// SPDX-License-Identifier: BSD-3-Clause

#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>

#include "block_meta.h"
#include "osmem.h"

extern struct block_meta *head;
extern struct block_meta *tail;
extern int prealloced;

struct block_meta *generic_alloc(size_t size, size_t threshold)
{
	struct block_meta *addr;
	size_t padded_size = size;

	if (padded_size % 8)
		padded_size = (padded_size / 8 + 1) * 8;

	if (padded_size + METADATA_SIZE > threshold) {
		addr = (struct block_meta *)mmap(NULL, padded_size + METADATA_SIZE,
										 PROT_READ | PROT_WRITE,
										 MAP_PRIVATE | MAP_ANON, -1, 0);

		if (addr == MAP_FAILED)
			return NULL;

		add_block(addr, padded_size, STATUS_MAPPED);
		return addr;
	}

	if (!prealloced) {
		prealloced = 1;
		if (prealloc_heap() == MAP_FAILED)
			return NULL;
	}

	coalesce_all();
	addr = (struct block_meta *)find_best(padded_size);
	if (addr) {
		split_block(addr, padded_size);
		addr->status = STATUS_ALLOC;
	} else if (((struct block_meta *)last_heap_block())->status == STATUS_FREE) {
		addr = last_heap_block();
		if (sbrk(padded_size - addr->size) == MAP_FAILED)
			return NULL;
		addr->size = padded_size;
		addr->status = STATUS_ALLOC;
	} else {
		addr = (struct block_meta *)sbrk(padded_size + METADATA_SIZE);
		if (addr == MAP_FAILED)
			return NULL;
		add_block(addr, padded_size, STATUS_ALLOC);
	}

	return addr;
}

void *os_malloc(size_t size)
{
	if (!size)
		return NULL;

	return (void *)(generic_alloc(size, MMAP_THRESHOLD) + 1);
}

void os_free(void *ptr)
{
	if (!ptr)
		return;

	struct block_meta *addr = ptr - METADATA_SIZE;

	if (addr->status == STATUS_MAPPED) {
		remove_block(addr);
		munmap(addr, addr->size + METADATA_SIZE);
	} else if (addr->status == STATUS_ALLOC) {
		addr->status = STATUS_FREE;
	} else {
		return;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (!nmemb || !size)
		return NULL;

	struct block_meta *addr =
		generic_alloc(size * nmemb, (size_t)getpagesize());

	if (addr == NULL)
		return NULL;

	memset(addr + 1, 0, addr->size);
	return (void *)(addr + 1);
}

void *os_realloc(void *ptr, size_t size)
{
	if (!size) {
		if (ptr != NULL)
			os_free(ptr);

		return NULL;
	}

	if (!ptr)
		return os_malloc(size);

	struct block_meta *addr = ptr - METADATA_SIZE;

	if (addr->status == STATUS_FREE)
		return NULL;

	size_t padded_size = size;

	if (padded_size % 8)
		padded_size = (padded_size / 8 + 1) * 8;

	if (padded_size == addr->size)
		return ptr;

	struct block_meta *new_addr;
	size_t old_size = addr->size;

	if (addr->status == STATUS_MAPPED ||
		padded_size + METADATA_SIZE > MMAP_THRESHOLD) {
		new_addr = os_malloc(padded_size);
		memcpy(new_addr, addr + 1, MIN(old_size, padded_size));
		os_free(addr + 1);
	} else if (padded_size < addr->size) {
		split_block(addr, padded_size);
		new_addr = addr + 1;
	} else {
		if (addr == last_heap_block()) {
			if (sbrk(padded_size - addr->size) == MAP_FAILED)
				return NULL;
			addr->size = padded_size;
			new_addr = addr + 1;
		} else {
			while (addr->size < padded_size && addr != tail &&
			   addr->next->status == STATUS_FREE)
				coalesce_blocks(addr);

			split_block(addr, padded_size);
			if (addr->size >= padded_size) {
				new_addr = addr + 1;
			} else {
				coalesce_all();

				new_addr = find_best(padded_size);
				if (new_addr) {
					memcpy(new_addr + 1, addr + 1, MIN(old_size, padded_size));
					os_free(addr + 1);
					split_block(new_addr, padded_size);
					new_addr->status = STATUS_ALLOC;
					new_addr = new_addr + 1;
				} else {
					new_addr = os_malloc(padded_size);
					memcpy(new_addr, addr + 1, MIN(old_size, padded_size));
					os_free(addr + 1);
				}
			}
		}
	}

	return (void *)new_addr;
}
