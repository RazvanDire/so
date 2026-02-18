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
	so_packet_t pkt;
	char buf[PKT_SZ];
	so_consumer_ctx_t *ctx = (so_consumer_ctx_t *)args;

	while (true) {
		int rc = ring_buffer_dequeue(ctx->producer_rb, &pkt, 0);

		if (!rc) {
			break;
		}

		int action = process_packet(&pkt);
		unsigned long hash = packet_hash(&pkt);
		unsigned long timestamp = pkt.hdr.timestamp;

		int len = snprintf(buf, 256, "%s %016lx %lu\n",
			RES_TO_STR(action), hash, timestamp);

		pthread_mutex_lock(&ctx->file_mutex);
		write(ctx->out_fd, buf, len);
		pthread_mutex_unlock(&ctx->file_mutex);
	}
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
