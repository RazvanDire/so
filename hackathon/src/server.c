// SPDX-License-Identifier: BSD-3-Clause

#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>

#include "ipc.h"
#include "server.h"
#include "utils.h"

#ifndef OUTPUT_TEMPLATE
#define OUTPUT_TEMPLATE "../checker/output/out-XXXXXX"
#endif

static int lib_prehooks(struct lib *lib)
{
	return 0;
}

static int lib_load(struct lib *lib)
{
	lib->handle = dlopen(lib->libname, RTLD_LAZY);

	return 0;
}

static int lib_execute(struct lib *lib, int *fds)
{
	lib->outputfile = malloc(1 + strlen(OUTPUT_TEMPLATE));
	DIE(lib->outputfile == NULL, "malloc");

	strcpy(lib->outputfile, OUTPUT_TEMPLATE);
	int fd_out = mkstemp(lib->outputfile);
	dup2(fd_out, STDOUT_FILENO);

	char sig[3 * BUFSIZE];

	if (strlen(lib->funcname) == 0) {
		sprintf(sig, "Error: %s run received signal SIGSEGV.\n", lib->libname);
	} else if (strlen(lib->filename) == 0) {
		sprintf(sig, "Error: %s %s received signal SIGSEGV.\n", lib->libname, lib->funcname);
	} else {
		sprintf(sig, "Error: %s %s %s received signal SIGSEGV.\n", lib->libname, lib->funcname, lib->filename);
	}

	close(fds[0]);
	write(fds[1], lib->outputfile, strlen(lib->outputfile) + 1);
	write(fds[1], sig, 3 * BUFSIZE);
	close(fds[1]);

	if (strlen(lib->funcname) == 0) {
		lib->run = dlsym(lib->handle, "run");

		if (lib->run == NULL)
			printf("Error: %s run could not be executed.\n", lib->libname);
		else
			lib->run();
	} else if (strlen(lib->filename) == 0) {
		lib->run = dlsym(lib->handle, lib->funcname);
		
		if (lib->run == NULL)
			printf("Error: %s %s could not be executed.\n", lib->libname, lib->funcname);
		else
			lib->run();
	} else {
		lib->p_run = dlsym(lib->handle, lib->funcname);

		if (lib->p_run == NULL)
			printf("Error: %s %s %s could not be executed.\n", lib->libname, lib->funcname, lib->filename);
		else
			lib->p_run(lib->filename);
	}

	fflush(stdout);
	close(fd_out);

	return 0;
}

static int lib_close(struct lib *lib)
{
	/* TODO: Implement lib_close(). */
	return 0;
}

static int lib_posthooks(struct lib *lib)
{
	/* TODO: Implement lib_posthooks(). */
	return 0;
}

static int lib_run(struct lib *lib, int *fds)
{
	int err;

	err = lib_prehooks(lib);
	if (err)
		return err;

	err = lib_load(lib);
	if (err)
		return err;

	err = lib_execute(lib, fds);
	if (err)
		return err;

	err = lib_close(lib);
	if (err)
		return err;

	return lib_posthooks(lib);
}

static int parse_command(const char *buf, char *name, char *func, char *params)
{
	int ret;

	ret = sscanf(buf, "%s %s %s", name, func, params);
	if (ret < 0)
		return -1;

	return ret;
}

int create_listener(void) {
	int listenfd, rc;
	struct sockaddr_un addr;

	remove(SOCKET_NAME);

	listenfd = create_socket();

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCKET_NAME);
	rc = bind(listenfd, &addr, sizeof(addr));
	DIE(rc < 0, "bind");

	rc = listen(listenfd, MAX_CLIENTS);
	DIE(rc < 0, "listen");

	return listenfd;
}

int main(void)
{
	/* TODO: Implement server connection. */
	int ret, listenfd, connectfd;
	struct lib lib;
	struct sockaddr_un addr;
	char name[BUFSIZE], func[BUFSIZE], params[BUFSIZE];
	char buf[BUFSIZE];
	pid_t pid;
	size_t addr_size = sizeof(addr);

	listenfd = create_listener();
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCKET_NAME);

	name[0] = '\0';
	func[0] = '\0';
	params[0] = '\0';
	memset(buf, 0, BUFSIZE);

	while (1) {
		/* TODO - get message from client */
		/* TODO - parse message with parse_command and populate lib */
		/* TODO - handle request from client */
		connectfd = accept(listenfd, &addr, &addr_size);
		DIE(connectfd < 0, "accept");

		pid = fork();

		switch (pid) {
			case -1:
				perror("Couldn't fork process");
				break;

			case 0:
				recv_socket(connectfd, buf, BUFSIZE);

				ret = parse_command(buf, name, func, params);
				DIE(ret < 0, "parse");

				lib.libname = malloc(1 + strlen(name));
				DIE(lib.libname == NULL, "malloc");
				memmove(lib.libname, name, 1 + strlen(name));

				lib.funcname = malloc(1 + strlen(func));
				DIE(lib.funcname == NULL, "malloc");
				memmove(lib.funcname, func, 1 + strlen(func));

				lib.filename = malloc(1 + strlen(params));
				DIE(lib.filename == NULL, "malloc");
				memmove(lib.filename, params, 1 + strlen(params));

				int fds[2];

				DIE(pipe(fds) == -1, "pipe");

				pid = fork();

				switch (pid) {
					case -1:
						perror("Couldn't fork");
						break;

					case 0:
						ret = lib_run(&lib, fds);

						exit(ret);
						break;

					default:
						int ret_status, fd_out;
						char sig[3 * BUFSIZE];

						close(fds[1]);
						lib.outputfile = malloc(strlen(OUTPUT_TEMPLATE) + 1);
						DIE(lib.outputfile == NULL, "malloc");

						read(fds[0], lib.outputfile, strlen(OUTPUT_TEMPLATE) + 1);
						read(fds[0], sig, 3 * BUFSIZE);
						close(fds[0]);

						waitpid(pid, &ret_status, 0);

						send_socket(connectfd, lib.outputfile, strlen(lib.outputfile));
						close(connectfd);

						if (WIFSIGNALED(ret_status) && WTERMSIG(ret_status) == SIGSEGV) {
							fd_out = open(lib.outputfile, O_WRONLY);
							write(fd_out, sig, strlen(sig));
							close(fd_out);
						}

						exit(WEXITSTATUS(ret_status));
						break;
				}
				break;

			default:
				close(connectfd);
		}
	}

	close(listenfd);

	return 0;
}
