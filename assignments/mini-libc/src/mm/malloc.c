// SPDX-License-Identifier: BSD-3-Clause

#include <internal/essentials.h>
#include <internal/mm/mem_list.h>
#include <internal/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

void *malloc(size_t size) {
	if (size <= 0) {
		return NULL;
	}

	void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
					  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	mem_list_add(addr, size);

	return addr;
}

void *calloc(size_t nmemb, size_t size) {
	/* TODO: Implement calloc(). */
	if (size <= 0) {
		return NULL;
	}

	void *addr = mmap(NULL, size * nmemb, PROT_READ | PROT_WRITE,
					  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	mem_list_add(addr, size * nmemb);
	// memset(addr, 0, size);

	return addr;
}

void free(void *ptr) {
	/* TODO: Implement free(). */
	struct mem_list *item = mem_list_find(ptr);
	if (ptr && item) {
		munmap(ptr, item->len);
		mem_list_del(ptr);
	}
}

void *realloc(void *ptr, size_t size) {
	/* TODO: Implement realloc(). */
	struct mem_list *item = mem_list_find(ptr);

	if (!item) {
		return NULL;
	}

	void *addr = mremap(ptr, item->len, size, MREMAP_MAYMOVE);

	item->len = size;
	item->start = addr;

	return addr;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size) {
	/* TODO: Implement reallocarray(). */
	struct mem_list *item = mem_list_find(ptr);

	if (!item) {
		return NULL;
	}

	void *addr = mremap(ptr, item->len, size * nmemb, MREMAP_MAYMOVE);

	item->len = size * nmemb;
	item->start = addr;

	return addr;
}
