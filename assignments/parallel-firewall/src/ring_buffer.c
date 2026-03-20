// SPDX-License-Identifier: BSD-3-Clause

#include "ring_buffer.h"

int ring_buffer_init(so_ring_buffer_t *ring, size_t cap)
{
	/* TODO: implement ring_buffer_init */
	ring->cap = cap;
	ring->len = cap / PKT_SZ;
	ring->producer_stopped = 0;

	ring->read_pos = 0;
	ring->write_pos = 0;

	ring->data = calloc(ring->len, sizeof(so_packet_t));

	pthread_mutex_init(&ring->empty_mutex, NULL);
	pthread_cond_init(&ring->empty_cond, NULL);

	pthread_mutex_init(&ring->full_mutex, NULL);
	pthread_cond_init(&ring->full_cond, NULL);

	return 1;
}

ssize_t ring_buffer_enqueue(so_ring_buffer_t *ring, void *data, size_t size)
{
	/* TODO: implement ring_buffer_enqueue */
	pthread_mutex_lock(&ring->full_mutex);
	while (ring->len - ring->write_pos + ring->read_pos == 0) {
		pthread_cond_wait(&ring->full_cond, &ring->full_mutex);
	}
	pthread_mutex_unlock(&ring->full_mutex);

	((so_packet_t *) ring->data)[ring->write_pos % ring->len] = *((so_packet_t *) data);
	(ring->write_pos)++;

	// signal one thread that the buffer is not empty
	pthread_mutex_lock(&ring->empty_mutex);
	pthread_cond_signal(&ring->empty_cond);
	pthread_mutex_unlock(&ring->empty_mutex);

	return -1;
}

ssize_t ring_buffer_dequeue(so_ring_buffer_t *ring, void *data, size_t size)
{
	/* TODO: Implement ring_buffer_dequeue */
	pthread_mutex_lock(&ring->empty_mutex);

	// if (ring->producer_stopped) {
	// 	printf("write_pos = %d, read_pos = %d\n", ring->write_pos, ring->read_pos);
	// }

	while ((ring->write_pos - ring->read_pos == 0) && !ring->producer_stopped) {
		pthread_cond_wait(&ring->empty_cond, &ring->empty_mutex);
	}

	if ((ring->write_pos - ring->read_pos) == 0) {
		pthread_mutex_unlock(&ring->empty_mutex);
		return 0;
	}

	dequeue_arg *arg = (dequeue_arg *)data;

	size_t read_pos = ring->read_pos;
	(ring->read_pos)++;
	arg->pkt = ((so_packet_t *) ring->data)[read_pos % ring->len];
	arg->read_pos = read_pos;

	pthread_mutex_unlock(&ring->empty_mutex);

	// pthread_mutex_lock(&ring->read_mutex);
	// *((so_packet_t *) data) = ((so_packet_t *) ring->data)[ring->read_pos % ring->cap];
	// (ring->read_pos)++;
	// pthread_mutex_unlock(&ring->read_mutex);

	// signal one thread that the buffer is not full
	pthread_mutex_lock(&ring->full_mutex);
	pthread_cond_signal(&ring->full_cond);
	pthread_mutex_unlock(&ring->full_mutex);

	return -1;
}

void ring_buffer_destroy(so_ring_buffer_t *ring)
{
	/* TODO: Implement ring_buffer_destroy */
	free(ring->data);

	pthread_mutex_destroy(&ring->empty_mutex);
	pthread_cond_destroy(&ring->empty_cond);

	pthread_mutex_destroy(&ring->full_mutex);
	pthread_cond_destroy(&ring->full_cond);
}

void ring_buffer_stop(so_ring_buffer_t *ring)
{
	/* TODO: Implement ring_buffer_stop */
	ring->producer_stopped = 1;
	//pthread_cond_broadcast(&ring->empty_cond);
}
