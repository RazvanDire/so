#include <errno.h>
#include <internal/syscall.h>
#include <time.h>
#include <unistd.h>

int nanosleep(const struct timespec *req, struct timespec *rem) {
	int ret = syscall(__NR_nanosleep, req, rem);

	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}

unsigned int sleep(unsigned int seconds) {
	struct timespec req;
	req.tv_sec = seconds;
	req.tv_nsec = 0;

	int ret = nanosleep(&req, NULL);

	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	return ret;
}
