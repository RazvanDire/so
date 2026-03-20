/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __SO_CONSUMER_H__
#define __SO_CONSUMER_H__

#include "ring_buffer.h"
#include "packet.h"

typedef struct so_consumer_ctx_t {
	struct so_ring_buffer_t *producer_rb;
	int out_fd;
	int offset;
	int packet_index;

    /* TODO: add synchronization primitives for timestamp ordering */
	pthread_mutex_t file_mutex;

	pthread_mutex_t sort_mutex;
	pthread_cond_t sort_cond;
} so_consumer_ctx_t;

int create_consumers(pthread_t *tids,
					int num_consumers,
					so_consumer_ctx_t *ctx);

#endif /* __SO_CONSUMER_H__ */
