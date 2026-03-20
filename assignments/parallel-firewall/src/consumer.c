// SPDX-License-Identifier: BSD-3-Clause

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include "consumer.h"
#include "ring_buffer.h"
#include "packet.h"
#include "utils.h"

void *consumer_thread(void *args)
{
	/* TODO: implement consumer thread */
	dequeue_arg arg;
	char buf[PKT_SZ];
	so_consumer_ctx_t *ctx = (so_consumer_ctx_t *)args;

	while (true) {
		int rc = ring_buffer_dequeue(ctx->producer_rb, &arg, 0);

		if (!rc) {
			break;
		}

		int action = process_packet(&arg.pkt);
		unsigned long hash = packet_hash(&arg.pkt);
		unsigned long timestamp = arg.pkt.hdr.timestamp;

		int len = snprintf(buf, 256, "%s %016lx %lu\n",
			RES_TO_STR(action), hash, timestamp);

		off_t offset;

		// pthread_mutex_lock(&ctx->file_mutex);
		// offset = ctx->offset;
		// (ctx->offset) += len;
		// pthread_mutex_unlock(&ctx->file_mutex);

		pthread_mutex_lock(&ctx->sort_mutex);

		while (arg.read_pos != ctx->packet_index) {
			pthread_cond_wait(&ctx->sort_cond, &ctx->sort_mutex);
		}

		(ctx->packet_index)++; 

		offset = ctx->offset;
		(ctx->offset) += len;

		pthread_cond_broadcast(&ctx->sort_cond);

		pthread_mutex_unlock(&ctx->sort_mutex);

		pwrite(ctx->out_fd, buf, len, offset);
	}

	return NULL;
}

int create_consumers(pthread_t *tids,
					 int num_consumers,
					 so_consumer_ctx_t *ctx)
{
	for (int i = 0; i < num_consumers; i++) {
		/*
		 * TODO: Launch consumer threads
		 **/
		int rc = pthread_create(&tids[i], NULL, consumer_thread, ctx);
		DIE(rc < 0, "pthread_create");
	}

	return num_consumers;
}
