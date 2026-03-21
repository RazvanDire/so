// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1
#define ERR			2

void free_argv(int argc, char **argv)
{
	for (int i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
}

void redirect(simple_command_t *s)
{
	int flags, fd;
	char *in, *out, *err;

	if (s->in) {
		in = get_word(s->in);

		fd = open(in, O_RDONLY);
		dup2(fd, READ);
		close(fd);

		free(in);
	}

	if (s->out)
		out = get_word(s->out);
	if (s->err)
		err = get_word(s->err);

	if (s->out && s->err && !strcmp(out, err)) {
		flags = O_WRONLY | O_CREAT;
		if (s->io_flags & (IO_OUT_APPEND | IO_ERR_APPEND))
			flags |= O_APPEND;
		else
			flags |= O_TRUNC;

		fd = open(out, flags, 0644);
		dup2(fd, WRITE);
		dup2(fd, ERR);
		close(fd);

		free(out);
		free(err);

		return;
	}

	if (s->out) {
		flags = O_WRONLY | O_CREAT;
		if (s->io_flags & IO_OUT_APPEND)
			flags |= O_APPEND;
		else
			flags |= O_TRUNC;

		fd = open(out, flags, 0644);
		dup2(fd, WRITE);
		close(fd);

		free(out);
	}

	if (s->err) {
		flags = O_WRONLY | O_CREAT;
		if (s->io_flags & IO_ERR_APPEND)
			flags |= O_APPEND;
		else
			flags |= O_TRUNC;

		fd = open(err, flags, 0644);
		dup2(fd, ERR);
		close(fd);

		free(err);
	}
}

/**
 * Internal change-directory command.
 */
static bool shell_cd(simple_command_t *s, word_t *dir)
{
	/* TODO: Execute cd. */
	int rc = 1, do_chdir = 1;
	char err_msg[1024];

	// case when there is more than one argument
	if (dir && dir->next_word) {
		do_chdir = 0;
		strcpy(err_msg, "cd: too many arguments\n");
	}

	int flags, fd_out, fd_err = ERR;
	char *out, *err;

	if (s->out) {
		out = get_word(s->out);

		fd_out = open(out, O_WRONLY | O_CREAT | O_APPEND, 0644);
		close(fd_out);

		free(out);
	}

	if (s->err) {
		err = get_word(s->err);

		flags = O_WRONLY | O_CREAT;
		if (s->io_flags & IO_ERR_APPEND)
			flags |= O_APPEND;
		else
			flags |= O_TRUNC;

		fd_err = open(err, flags, 0644);

		free(err);
	}

	char *path;

	if (!dir || !dir->string) {
		char *home = getenv("HOME");

		// treat case when HOME is not set
		if (home) {
			int home_len = strlen(home);
	
			path = malloc(home_len + 1);
			memcpy(path, home, home_len + 1);
		} else {
			do_chdir = 0;
			strcpy(err_msg, "cd: HOME not set\n");
		}
	} else if (do_chdir) {
		path = get_word(dir);

		if (!strcmp(path, "-")) {
			free(path);

			char *old_pwd = getenv("OLDPWD");

			if (!old_pwd) {
				do_chdir = 0;
				strcpy(err_msg, "cd: OLDPWD not set\n");
			} else {
				int old_pwd_len = strlen(old_pwd);

				path = malloc(old_pwd_len + 1);
				memcpy(path, old_pwd, old_pwd_len + 1);
			}
		}
	}

	if (do_chdir) {
		rc = chdir(path);

		if (rc) {
			switch (errno) {
				case EACCES:
					sprintf(err_msg, "cd: %s: Permission denied\n", path);
					break;

				case ENOENT:
					strcpy(err_msg, "cd: no such file or directory\n");
					break;

				case ENOTDIR:
					sprintf(err_msg, "cd: %s: Not a directory\n", path);
					break;

				default:
					strcpy(err_msg, "cd: unknown error\n");
			}
		}

		free(path);
	}

	if (!do_chdir || rc)
		write(fd_err, err_msg, strlen(err_msg));
	else {
		char *cwd = getcwd(NULL, 0);

		setenv("OLDPWD", getenv("PWD"), 1);
		setenv("PWD", cwd, 1);

		free(cwd);
	}

	if (s->err)
		close(fd_err);

	return rc;
}

static int shell_pwd(simple_command_t *s)
{
	char *pwd = getcwd(NULL, 0);

	int flags, fd_out = WRITE, fd_err, rc;
	char *out, *err;

	if (s->err) {
		err = get_word(s->err);

		fd_err = open(err, O_WRONLY | O_CREAT | O_APPEND, 0644);
		close(fd_err);

		free(err);
	}

	if (s->out) {
		out = get_word(s->out);

		flags = O_WRONLY | O_CREAT;
		if (s->io_flags & IO_OUT_APPEND)
			flags |= O_APPEND;
		else
			flags |= O_TRUNC;

		fd_out = open(out, flags, 0644);

		free(out);
	}

	if (pwd == NULL) {
		rc = -1;
	} else {
		write(fd_out, pwd, strlen(pwd));
		write(fd_out, "\n", 1);
		rc = 0;
	}

	if (s->out)
		close(fd_out);

	free(pwd);

	return rc;
}

/**
 * Internal exit/quit command.
 */
static int shell_exit(void)
{
	/* TODO: Execute exit/quit. */
	/* TODO: Replace with actual exit code. */
	return SHELL_EXIT;
}

/**
 * Parse a simple command (internal, environment variable assignment,
 * external command).
 */
static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	/* TODO: Sanity checks. */

	/* TODO: If builtin command, execute the command. */
	char *cmd = get_word(s->verb);

	if (!strcmp(cmd, "exit") || !strcmp(cmd, "quit")) {
		free(cmd);
		return shell_exit();
	} else if (!strcmp(cmd, "pwd")) {
		free(cmd);
		return shell_pwd(s);
	} else if (!strcmp(cmd, "cd")) {
		free(cmd);
		return shell_cd(s, s->params);
	} else if (!strcmp(cmd, "true")) {
		free(cmd);
		return 0;
	} else if (!strcmp(cmd, "false")) {
		free(cmd);
		return 1;
	}

	/* TODO: If variable assignment, execute the assignment and return
	 * the exit status.
	 */

	char *eq_sign = strchr(cmd, '=');

	if (eq_sign) {
		char *value = eq_sign + 1;
		unsigned long name_size = (unsigned long)(value - cmd);
		char *name = malloc(name_size * sizeof(char));

		memcpy(name, cmd, name_size - 1);
		name[name_size - 1] = '\0';
		setenv(name, value, 1);

		free(name);
		free(cmd);

		return 0;
	}

	/* TODO: If external command:
	 *   1. Fork new process
	 *     2c. Perform redirections in child
	 *     3c. Load executable in child
	 *   2. Wait for child
	 *   3. Return exit status
	 */

	int rc, argc, ret_status;
	char **argv = get_argv(s, &argc);
	pid_t pid;

	pid = fork();
	switch (pid) {
	case -1:
		fprintf(stderr, "Fork error");
		rc = -1;
		break;

	case 0:
		redirect(s);
		execvp(cmd, argv);
		break;

	default:
		waitpid(pid, &ret_status, 0);
		rc = WEXITSTATUS(ret_status);
	}

	free(cmd);
	free_argv(argc, argv);

	if (!pid) {
		printf("Execution failed for '%s'\n", cmd);

		exit(-1);
	}

	return rc; /* TODO: Replace with actual exit status. */
}

/**
 * Process two commands in parallel, by creating two children.
 */
static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Execute cmd1 and cmd2 simultaneously. */
	bool rc;
	pid_t pid1 = fork(), pid2;
	int fds[2];

	if (pipe(fds) == -1)
		return false;

	switch (pid1) {
	case -1:
		return false;

	case 0:
		close(fds[READ]);
		rc = parse_command(cmd1, level + 1, father);
		write(fds[WRITE], &rc, sizeof(int));
		close(fds[WRITE]);
		exit(rc);

	default:
		pid2 = fork();
	}

	switch (pid2) {
	case -1:
		return false;

	case 0:
		close(fds[READ]);
		rc = parse_command(cmd2, level + 1, father);
		write(fds[WRITE], &rc, sizeof(int));
		close(fds[WRITE]);
		exit(rc);

	default:
		read(fds[READ], &rc, sizeof(int));
	}

	close(fds[0]);
	close(fds[1]);

	return rc; /* TODO: Replace with actual exit status. */
}

/**
 * Run commands by creating an anonymous pipe (cmd1 | cmd2).
 */
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	/* TODO: Redirect the output of cmd1 to the input of cmd2. */
	int fds[2], ret_status;

	if (pipe(fds) == -1)
		return false;

	pid_t pid1 = fork(), pid2;
	bool rc;

	switch (pid1) {
	case -1:
		return false;

	case 0:
		close(fds[READ]);
		dup2(fds[WRITE], WRITE);
		rc = parse_command(cmd1, level + 1, father);
		close(fds[WRITE]);

		exit(rc);

	default:
		close(fds[WRITE]);
		pid2 = fork();
	}

	switch (pid2) {
	case -1:
		return false;

	case 0:
		waitpid(pid1, &ret_status, 0);

		close(fds[WRITE]);
		dup2(fds[READ], READ);
		rc = parse_command(cmd2, level + 1, father);
		close(fds[READ]);

		exit(rc);

	default:
		waitpid(pid2, &ret_status, 0);

		rc = WEXITSTATUS(ret_status);
		close(fds[READ]);
	}

	return rc; /* TODO: Replace with actual exit status. */
}

/**
 * Parse and execute a command.
 */
int parse_command(command_t *c, int level, command_t *father)
{
	/* TODO: sanity checks */

	if (c->op == OP_NONE) {
		/* TODO: Execute a simple command. */
		/* TODO: Replace with actual exit code of command. */
		return parse_simple(c->scmd, level, father);
	}

	int rc;

	switch (c->op) {
	case OP_SEQUENTIAL:
		/* TODO: Execute the commands one after the other. */
		parse_command(c->cmd1, level + 1, c);
		rc = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PARALLEL:
		/* TODO: Execute the commands simultaneously. */
		rc = run_in_parallel(c->cmd1, c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_NZERO:
		/* TODO: Execute the second command only if the first one
		 * returns non zero.
		 */
		rc = parse_command(c->cmd1, level + 1, c);

		if (rc)
			rc = parse_command(c->cmd2, level + 1, c);

		break;

	case OP_CONDITIONAL_ZERO:
		/* TODO: Execute the second command only if the first one
		 * returns zero.
		 */
		rc = parse_command(c->cmd1, level + 1, c);

		if (!rc)
			rc = parse_command(c->cmd2, level + 1, c);

		break;

	case OP_PIPE:
		/* TODO: Redirect the output of the first command to the
		 * input of the second.
		 */
		rc = run_on_pipe(c->cmd1, c->cmd2, level + 1, c);
		break;

	default:
		return SHELL_EXIT;
	}

	return rc; /* TODO: Replace with actual exit code of command. */
}
