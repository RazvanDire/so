#ifndef __TIME_H__
#define __TIME_H__	1

typedef unsigned long time_t;

struct timespec{
	time_t tv_sec;
	long tv_nsec;
};

int nanosleep(const struct timespec *req, struct timespec *rem);

#endif
