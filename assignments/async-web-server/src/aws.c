// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/eventfd.h>
#include <libaio.h>
#include <errno.h>

#include "aws.h"
#include "utils/util.h"
#include "utils/debug.h"
#include "utils/sock_util.h"
#include "utils/w_epoll.h"

/* server socket file descriptor */
static int listenfd;

/* epoll file descriptor */
static int epollfd;

static io_context_t ctx;

static int aws_on_path_cb(http_parser *p, const char *buf, size_t len)
{
	struct connection *conn = (struct connection *)p->data;

	memcpy(conn->request_path, buf, len);
	conn->request_path[len] = '\0';
	conn->have_path = 1;

	return 0;
}

static void connection_prepare_send_reply_header(struct connection *conn)
{
	/* Prepare the connection buffer to send the reply header. */
	sprintf(conn->send_buffer, "HTTP/1.1 200 OK\r\n"
		"Date: Sun, 08 May 2011 09:26:16 GMT\r\n"
		"Server: Apache/2.2.9\r\n"
		"Last-Modified: Mon, 02 Aug 2010 17:55:28 GMT\r\n"
		"Accept-Ranges: bytes\r\n"
		"Vary: Accept-Encoding\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: %ld\r\n\r\n", conn->file_size);

	conn->send_len = strlen(conn->send_buffer);
	conn->state = STATE_SENDING_HEADER;
}

static void connection_prepare_send_404(struct connection *conn)
{
	/* Prepare the connection buffer to send the 404 header. */
	char buffer[BUFSIZ] = "HTTP/1.1 404 Not Found\r\n"
		"Date: Sun, 08 May 2011 09:26:16 GMT\r\n"
		"Server: Apache/2.2.9\r\n"
		"Last-Modified: Mon, 02 Aug 2010 17:55:28 GMT\r\n"
		"Accept-Ranges: bytes\r\n"
		"Vary: Accept-Encoding\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: 0\r\n\r\n";

	conn->send_len = strlen(buffer);
	memcpy(conn->send_buffer, buffer, conn->send_len);
	conn->state = STATE_SENDING_404;
}

static void connection_prepare_send_403(struct connection *conn)
{
	char buffer[BUFSIZ] = "HTTP/1.1 403 Forbidden\r\n"
		"Date: Sun, 08 May 2011 09:26:16 GMT\r\n"
		"Server: Apache/2.2.9\r\n"
		"Last-Modified: Mon, 02 Aug 2010 17:55:28 GMT\r\n"
		"Accept-Ranges: bytes\r\n"
		"Vary: Accept-Encoding\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: 0\r\n\r\n";

	conn->send_len = strlen(buffer);
	memcpy(conn->send_buffer, buffer, conn->send_len);
	conn->state = STATE_SENDING_403;
}

static enum resource_type connection_get_resource_type(struct connection *conn)
{
	/* Get resource type depending on request path/filename. Filename should
	 * point to the static or dynamic folder.
	 */
	if (strstr(conn->request_path, AWS_REL_DYNAMIC_FOLDER))
		return RESOURCE_TYPE_DYNAMIC;

	if (strstr(conn->request_path, AWS_REL_STATIC_FOLDER))
		return RESOURCE_TYPE_STATIC;

	return RESOURCE_TYPE_NONE;
}


struct connection *connection_create(int sockfd)
{
	/* Initialize connection structure on given socket. */
	struct connection *conn = calloc(1, sizeof(*conn));

	DIE(conn == NULL, "malloc");

	conn->sockfd = sockfd;
	memset(conn->recv_buffer, 0, BUFSIZ);
	memset(conn->send_buffer, 0, BUFSIZ);
	conn->recv_len = 0;
	conn->send_pos = 0;
	conn->fd = -1;
	conn->state = STATE_INITIAL;

	return conn;
}

void connection_start_async_io(struct connection *conn)
{
	/* Start asynchronous operation (read from file).
	 * Use io_submit(2) & friends for reading data asynchronously.
	 */
	int rc;

	rc = io_setup(1, &conn->ctx);
	DIE(rc < 0, "io_setup");

	conn->async_buffer = malloc(conn->file_size * sizeof(char));

	io_prep_pread(&conn->iocb, conn->fd, conn->async_buffer, conn->file_size, 0);
	conn->piocb[0] = &conn->iocb;

	conn->eventfd = eventfd(0, EFD_NONBLOCK);
	io_set_eventfd(&conn->iocb, conn->eventfd);

	rc = w_epoll_add_ptr_in(epollfd, conn->eventfd, conn);
	DIE(rc < 0, "asyncio w_epoll_add");

	rc = w_epoll_remove_ptr(epollfd, conn->sockfd, conn);
	DIE(rc < 0, "temporarily remove sockfd");

	rc = io_submit(conn->ctx, 1, conn->piocb);
	DIE(rc < 0, "io_submit");

	conn->send_pos = 0;
	conn->state = STATE_ASYNC_ONGOING;
}

void connection_remove(struct connection *conn)
{
	/* Remove connection handler. */
	int rc;

	rc = w_epoll_remove_ptr(epollfd, conn->sockfd, conn);
	DIE(rc < 0, "w_epoll_remove_ptr");

	if (conn->fd != -1)
		close(conn->fd);

	close(conn->sockfd);
	conn->state = STATE_CONNECTION_CLOSED;
	free(conn);
}

void handle_new_connection(void)
{
	/* Handle a new connection request on the server socket. */
	static int sockfd;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	struct connection *conn;
	int rc;

	/* Accept new connection. */
	sockfd = accept(listenfd, (SSA *) &addr, &addrlen);
	DIE(sockfd < 0, "accept");

	/* Set socket to be non-blocking. */
	int status = fcntl(sockfd, F_SETFL, O_NONBLOCK);

	DIE(status < 0, "non blocking");

	/* Instantiate new connection handler. */
	conn = connection_create(sockfd);

	/* Add socket to epoll. */
	rc = w_epoll_add_ptr_in(epollfd, sockfd, conn);
	DIE(rc < 0, "w_epoll_add_in");

	/* Initialize HTTP_REQUEST parser. */
	http_parser_init(&conn->request_parser, HTTP_REQUEST);
}

void receive_data(struct connection *conn)
{
	/* Receive message on socket.
	 * Store message in recv_buffer in struct connection.
	 */
	ssize_t bytes_recv;
	int rc;
	char abuffer[64];

	rc = get_peer_address(conn->sockfd, abuffer, 64);
	if (rc < 0) {
		ERR("get_peer_address");
		goto remove_connection;
	}

	bytes_recv = recv(conn->sockfd, conn->recv_buffer + conn->recv_len, BUFSIZ, 0);

	if (bytes_recv < 0) {		/* error in communication */
		dlog(LOG_ERR, "Error in communication from: %s\n", abuffer);
		goto remove_connection;
	}

	if (bytes_recv == 0) {		/* connection closed */
		dlog(LOG_INFO, "Connection closed from: %s\n", abuffer);
		goto remove_connection;
	}

	dlog(LOG_DEBUG, "Received message from: %s\n", abuffer);
	dlog(LOG_DEBUG, "--\n%s--\n", conn->recv_buffer);

	conn->recv_len += bytes_recv;

	if (strstr(conn->recv_buffer, "\r\n\r\n")) {
		rc = w_epoll_update_ptr_out(epollfd, conn->sockfd, conn);
		DIE(rc < 0, "w_epoll_add_ptr_inout");
		conn->state = STATE_REQUEST_RECEIVED;
	} else {
		conn->state = STATE_RECEIVING_DATA;
	}

	return;

remove_connection:
	/* remove current connection */
	connection_remove(conn);
}

int connection_open_file(struct connection *conn)
{
	/* Open file and update connection fields. */
	sprintf(conn->filename, "%s%s", AWS_DOCUMENT_ROOT, conn->request_path + 1);

	int fd = open(conn->filename, O_RDONLY);

	if (fd == -1)
		return -1;

	struct stat st;

	conn->fd = fd;
	fstat(fd, &st);
	conn->file_size = st.st_size;

	return fd;
}

void connection_complete_async_io(struct connection *conn)
{
	/* Complete asynchronous operation; operation returns successfully.
	 * Prepare socket for sending.
	 */
	int rc;

	dlog(LOG_DEBUG, "complete aio %s\n", conn->send_buffer);

	rc = w_epoll_remove_ptr(epollfd, conn->eventfd, conn);
	DIE(rc < 0, "remove eventfd");

	rc = w_epoll_add_ptr_out(epollfd, conn->sockfd, conn);
	DIE(rc < 0, "add sockfd back");

	conn->state = STATE_SENDING_DATA;

	io_destroy(conn->ctx);
	close(conn->eventfd);
}

int parse_header(struct connection *conn)
{
	/* Parse the HTTP header and extract the file path. */
	/* Use mostly null settings except for on_path callback. */
	http_parser_settings settings_on_path = {
		.on_message_begin = 0,
		.on_header_field = 0,
		.on_header_value = 0,
		.on_path = aws_on_path_cb,
		.on_url = 0,
		.on_fragment = 0,
		.on_query_string = 0,
		.on_body = 0,
		.on_headers_complete = 0,
		.on_message_complete = 0
	};

	conn->request_parser.data = conn;
	return http_parser_execute(&conn->request_parser, &settings_on_path, conn->recv_buffer, conn->recv_len);
}

enum connection_state connection_send_static(struct connection *conn)
{
	/* Send static data using sendfile(2). */
	ssize_t bytes_sent;

	bytes_sent = sendfile(conn->sockfd, conn->fd, NULL, conn->file_size);
	conn->file_size -= bytes_sent;

	if (conn->file_size <= 0)
		conn->state = STATE_DATA_SENT;

	return conn->state;
}

int connection_send_data(struct connection *conn)
{
	/* May be used as a helper function. */
	/* Send as much data as possible from the connection send buffer.
	 * Returns the number of bytes sent or -1 if an error occurred
	 */

	ssize_t bytes_sent;
	int rc;
	char abuffer[64];

	rc = get_peer_address(conn->sockfd, abuffer, 64);
	if (rc < 0) {
		ERR("get_peer_address");
		goto remove_connection;
	}

	bytes_sent = send(conn->sockfd, conn->send_buffer + conn->send_pos, conn->send_len, 0);

	if (bytes_sent < 0) {		/* error in communication */
		dlog(LOG_ERR, "Error in communication to %s\n", abuffer);
		goto remove_connection;
	}

	if (bytes_sent == 0) {		/* connection closed */
		dlog(LOG_INFO, "Connection closed to %s\n", abuffer);
		goto remove_connection;
	}

	dlog(LOG_DEBUG, "Sending message to %s\n", abuffer);

	dlog(LOG_DEBUG, "--\n%s--\n", conn->send_buffer);
	conn->send_len -= bytes_sent;
	conn->send_pos += bytes_sent;

	if (conn->send_len <= 0) {
		switch (conn->fd) {
		case -1:
			conn->state = STATE_404_SENT;
			break;

		default:
			switch (conn->res_type) {
			case RESOURCE_TYPE_NONE:
				conn->state = STATE_403_SENT;
				break;

			default:
				conn->state = STATE_HEADER_SENT;
			}
		}
	}

	return bytes_sent;

remove_connection:
	/* remove current connection */
	connection_remove(conn);
	return -1;
}

int connection_send_dynamic(struct connection *conn)
{
	/* Read data asynchronously.
	 * Returns 0 on success and -1 on error.
	 */
	ssize_t bytes_sent;
	int rc;
	char abuffer[64];

	rc = get_peer_address(conn->sockfd, abuffer, 64);
	if (rc < 0) {
		ERR("get_peer_address");
		goto remove_connection;
	}

	bytes_sent = send(conn->sockfd, conn->async_buffer + conn->send_pos, conn->file_size, 0);

	if (bytes_sent < 0) {		/* error in communication */
		dlog(LOG_ERR, "Error in communication to %s\n", abuffer);
		goto remove_connection;
	}

	if (bytes_sent == 0) {		/* connection closed */
		dlog(LOG_INFO, "Connection closed to %s\n", abuffer);
		goto remove_connection;
	}

	dlog(LOG_DEBUG, "Sending message to %s\n", abuffer);
	dlog(LOG_DEBUG, "--\n%s--\n", conn->send_buffer);

	conn->send_pos += bytes_sent;
	conn->file_size -= bytes_sent;

	if (conn->file_size <= 0) {
		conn->state = STATE_DATA_SENT;
		free(conn->async_buffer);
	}

	return 0;

remove_connection:
	/* remove current connection */
	connection_remove(conn);
	return -1;
}


void handle_input(struct connection *conn)
{
	/* Handle input information: may be a new message or notification of
	 * completion of an asynchronous I/O operation.
	 */

	switch (conn->state) {
	case STATE_INITIAL:
		conn->state = STATE_RECEIVING_DATA;
		receive_data(conn);
		break;

	case STATE_RECEIVING_DATA:
		receive_data(conn);
		break;

	case STATE_ASYNC_ONGOING:
		connection_complete_async_io(conn);
		break;

	default:
		printf("shouldn't get here %d\n", conn->state);
	}
}

void handle_output(struct connection *conn)
{
	/* Handle output information: may be a new valid requests or notification of
	 * completion of an asynchronous I/O operation or invalid requests.
	 */
	switch (conn->state) {
	case STATE_REQUEST_RECEIVED:
		parse_header(conn);
		connection_open_file(conn);

		switch (conn->fd) {
		case -1:
			connection_prepare_send_404(conn);
			break;

		default:
			conn->res_type = connection_get_resource_type(conn);
			switch (conn->res_type) {
			case RESOURCE_TYPE_NONE:
				connection_prepare_send_403(conn);
				break;

			default:
				connection_prepare_send_reply_header(conn);
			}
		}
		break;

	case STATE_SENDING_HEADER:
	case STATE_SENDING_404:
	case STATE_SENDING_403:
		connection_send_data(conn);
		break;

	case STATE_HEADER_SENT:
		if (conn->res_type == RESOURCE_TYPE_STATIC) {
			conn->state = STATE_SENDING_DATA;
			connection_send_static(conn);
		} else if (conn->res_type == RESOURCE_TYPE_DYNAMIC) {
			connection_start_async_io(conn);
		}
		break;

	case STATE_SENDING_DATA:
		if (conn->res_type == RESOURCE_TYPE_STATIC)
			connection_send_static(conn);
		else
			connection_send_dynamic(conn);
		break;

	case STATE_403_SENT:
	case STATE_404_SENT:
	case STATE_DATA_SENT:
		connection_remove(conn);
		break;

	default:
		ERR("Unexpected state\n");
		exit(1);
	}
}

void handle_client(uint32_t event, struct connection *conn)
{
	/* Handle new client. There can be input and output connections.
	 * Take care of what happened at the end of a connection.
	 */
	if (event & EPOLLIN)
		handle_input(conn);
	else if (event & EPOLLOUT)
		handle_output(conn);
}

int main(void)
{
	int rc;

	/* Initialize asynchronous operations. */
	rc = io_setup(10, &ctx);
	DIE(rc < 0, "io_setup");

	/* Initialize multiplexing. */
	epollfd = w_epoll_create();

	/* Create server socket. */
	listenfd = tcp_create_listener(AWS_LISTEN_PORT, 10);

	/* Add server socket to epoll object*/
	w_epoll_add_fd_in(epollfd, listenfd);

	/* Uncomment the following line for debugging. */
	// dlog(LOG_INFO, "Server waiting for connections on port %d\n", AWS_LISTEN_PORT);

	/* server main loop */
	while (1) {
		struct epoll_event rev;

		/* Wait for events. */
		w_epoll_wait_infinite(epollfd, &rev);

		/* Switch event types; consider
		 *   - new connection requests (on server socket)
		 *   - socket communication (on connection sockets)
		 */
		if (rev.data.fd == listenfd) {
			dlog(LOG_DEBUG, "New connection\n");
			handle_new_connection();
		} else {
			handle_client(rev.events, rev.data.ptr);
		}
	}

	return 0;
}
