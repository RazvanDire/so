// SPDX-License-Identifier: BSD-3-Clause

#include <errno.h>
#include <fcntl.h>
#include <internal/syscall.h>
#include <stdarg.h>

int open(const char *filename, int flags, ...) {
	/* TODO: Implement open system call. */

	va_list args;
	va_start(args, flags);
	int mode = va_arg(args, int);
	va_end(args);

	int ret = syscall(__NR_open, filename, flags, mode);

	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}
