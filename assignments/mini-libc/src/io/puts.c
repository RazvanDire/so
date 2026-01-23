#include <internal/io.h>
#include <internal/syscall.h>
#include <string.h>

int puts(const char *str) {
	char newline[2] = "\n";

	int ret = write(1, str, strlen(str));
	if (ret < 0) {
		return ret;
	}

	ret = write(1, newline, 1);
	if (ret < 0) {
		return ret;
	}

	return strlen(str) + 1;
}
