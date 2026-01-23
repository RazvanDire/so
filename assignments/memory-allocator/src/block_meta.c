// SPDX-License-Identifier: BSD-3-Clause

#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include "block_meta.h"

struct block_meta *head;
struct block_meta *tail;
int prealloced;

void *last_heap_block(void)
{
	struct block_meta *aux = tail;

	while (aux != NULL && aux->status == STATUS_MAPPED)
		aux = aux->prev;

	return aux;
}

void add_block(struct block_meta *addr, size_t size, int status)
{
	struct block_meta *new = addr;

	new->size = size;
	new->status = status;
	new->next = NULL;

	if (!head) {
		head = tail = new;
		head->prev = NULL;

		return;
	}

	if (head == tail) {
		tail = new;
		tail->prev = head;
		head->next = tail;

		return;
	}

	tail->next = new;
	new->prev = tail;
	tail = new;
}

void remove_block(struct block_meta *addr)
{
	if (head == tail) {
		head = tail = NULL;
		return;
	}

	if (addr == head) {
		head = head->next;
		head->prev = NULL;

		return;
	}

	if (addr == tail) {
		tail = tail->prev;
		tail->next = NULL;

		return;
	}

	addr->prev->next = addr->next;
	addr->next->prev = addr->prev;
}

void coalesce_blocks(struct block_meta *addr)
{
	if (addr == tail)
		return;

	addr->size += addr->next->size + METADATA_SIZE;

	if (addr->next != tail)
		addr->next->next->prev = addr;
	else
		tail = addr;

	addr->next = addr->next->next;
}

void coalesce_all(void)
{
	struct block_meta *aux = head;

	while (aux != NULL && aux->next != NULL) {
		if (aux->status == STATUS_FREE && aux->next->status == STATUS_FREE)
			coalesce_blocks(aux);
		else
			aux = aux->next;
	}
}

void *prealloc_heap(void)
{
	struct block_meta *new = (struct block_meta *)sbrk(MMAP_THRESHOLD);

	add_block(new, MMAP_THRESHOLD - METADATA_SIZE, STATUS_FREE);
	return new;
}

void split_block(struct block_meta *addr, size_t size)
{
	if (addr->size >= size + METADATA_SIZE + 8) {
		struct block_meta *new =
			(struct block_meta *)((void *)addr + METADATA_SIZE + size);

		new->status = STATUS_FREE;
		new->size = addr->size - METADATA_SIZE - size;

		if (addr == tail)
			tail = new;
		else
			addr->next->prev = new;

		new->next = addr->next;
		addr->next = new;
		new->prev = addr;
		addr->size = size;
	}
}

void *find_best(size_t size)
{
	struct block_meta *aux = head, *best = NULL;
	size_t diff = LONG_MAX;

	while (aux != NULL) {
		if (aux->size >= size && aux->size - size < diff &&
			aux->status == STATUS_FREE) {
			diff = aux->size - size;
			best = aux;
		}

		aux = aux->next;
	}

	if (diff != LONG_MAX)
		return best;
	else
		return NULL;
}
