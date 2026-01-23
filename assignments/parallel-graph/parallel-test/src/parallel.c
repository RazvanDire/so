// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "log/log.h"
#include "os_graph.h"
#include "os_threadpool.h"
#include "utils.h"

#define NUM_THREADS 4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* TODO: Define graph synchronization mechanisms. */

/* TODO: Define graph task argument. */
typedef struct thread_arg {
	int idx;
	unsigned int *count;
} thread_arg;

struct timespec ts = {
	.tv_sec = 0,
	.tv_nsec = 1000000
};

/* TODO: Call nanosleep(&ts, NULL); when adding to sum */
/* Make call outside a lock / unlock section. */

void destroy_arg(void *arg)
{
	free(arg);
}

static void do_task(void *arg)
{
	thread_arg *targ = (thread_arg *)arg;

	typeof(*graph->visited) not_visited = NOT_VISITED, processing = PROCESSING;
	int is_not_visited = atomic_compare_exchange_strong(&graph->visited[targ->idx], &not_visited, processing);

	if (!is_not_visited) {
		atomic_fetch_sub(&tp->remaining, 1);

		pthread_mutex_lock(&tp->done_mutex);
		if (!tp->remaining)
			pthread_cond_signal(&tp->done_cond);
		pthread_mutex_unlock(&tp->done_mutex);

		return;
	}

	for (unsigned int i = 0; i < graph->nodes[targ->idx]->num_neighbours; i++)
		if (graph->visited[graph->nodes[targ->idx]->neighbours[i]] == NOT_VISITED) {
			thread_arg *temp = malloc(sizeof(thread_arg));

			temp->idx = graph->nodes[targ->idx]->neighbours[i];

			atomic_fetch_add(&tp->remaining, 1);
			enqueue_task(tp, create_task(do_task, (void *)temp, destroy_arg));
		}

	atomic_fetch_add(&sum, graph->nodes[targ->idx]->info);
	nanosleep(&ts, NULL);
	*(targ->count) = *(targ->count) + 1;
	atomic_fetch_sub(&tp->remaining, 1);

	graph->visited[targ->idx] = DONE;

	pthread_mutex_lock(&tp->done_mutex);
	if (!tp->remaining)
		pthread_cond_signal(&tp->done_cond);
	pthread_mutex_unlock(&tp->done_mutex);
}

static void process_node(int idx)
{
	/* TODO: Implement thread-pool based processing of graph. */
	thread_arg *temp = malloc(sizeof(thread_arg));

	temp->idx = idx;

	enqueue_task(tp, create_task(do_task, (void *)temp, destroy_arg));
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* TODO: Initialize graph synchronization mechanisms. */

	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	printf("%d", sum);

	return 0;
}
