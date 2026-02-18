/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __SO_RINGBUFFER_H__
#define __SO_RINGBUFFER_H__

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "packet.h"

typedef struct so_ring_buffer_t {
	char *data;

	size_t read_pos;
	size_t write_pos;

	size_t len;
	size_t cap;

	int producer_stopped;

	/* TODO: Add syncronization primitives */
	pthread_cond_t empty_cond;
	pthread_mutex_t empty_mutex;

	pthread_cond_t full_cond;
	pthread_mutex_t full_mutex;
} so_ring_buffer_t;

int     ring_buffer_init(so_ring_buffer_t *rb, size_t cap);
ssize_t ring_buffer_enqueue(so_ring_buffer_t *rb, void *data, size_t size);
ssize_t ring_buffer_dequeue(so_ring_buffer_t *rb, void *data, size_t size);
void    ring_buffer_destroy(so_ring_buffer_t *rb);
void    ring_buffer_stop(so_ring_buffer_t *rb);

#endif /* __SO_RINGBUFFER_H__ */
