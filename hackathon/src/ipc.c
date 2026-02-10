// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ipc.h"
#include "utils.h"


int create_socket(void)
{
	/* TODO: Implement create_socket(). */
	int fd;

	fd = socket(PF_UNIX, SOCK_STREAM, 0);
	DIE(fd < 0, "socket");

	return fd;
}

int connect_socket(int fd)
{
	/* TODO: Implement connect_socket(). */
	int rc;
	struct sockaddr_un addr;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCKET_NAME);
	rc = connect(fd, &addr, sizeof(addr));

	return rc;
}

ssize_t send_socket(int fd, const char *buf, size_t len)
{
	/* TODO: Implement send_socket(). */
	size_t bytes_sent, total_bytes = 0;

	while (total_bytes <= len) {
		bytes_sent = send(fd, buf + total_bytes, len - total_bytes, 0);
		total_bytes += bytes_sent;

		if (!bytes_sent)
			break;
	}

	return total_bytes;
}

ssize_t recv_socket(int fd, char *buf, size_t len)
{
	/* TODO: Implement recv_socket(). */
	size_t total_bytes = 0;

	total_bytes = recv(fd, buf, len, 0);

	return total_bytes;
}

void close_socket(int fd)
{
	/* TODO: Implement close_socket(). */
	close(fd);
}
