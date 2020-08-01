#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/timerfd.h>
#include <sys/epoll.h>

/* The test case */
void test();
void teardown();

struct file {
	int type;
};

/* Map from some collection of integers to a shared extensible struct of data */
const int MAX_FDS = 1000;
struct file *fd_to_file[MAX_FDS];

const int FD_TYPE_EPOLL = 0;
const int FD_TYPE_TIMER = 1;
const int FD_TYPE_EVENT = 2;
const int FD_TYPE_SOCKET = 3;

int num_fds = 0;

/* Keeping track of cunsumable data */
unsigned char *consumable_data;
int consumable_data_length;

void set_consumable_data(const unsigned char *new_data, int new_length) {
	consumable_data = (unsigned char *) new_data;
	consumable_data_length = new_length;
}

void consume_data() {

}

/* Keeping track of FDs */

int allocate_fd(int type, struct file *f) {
	for (int fd = 0; fd < MAX_FDS; fd++) {
		if (!fd_to_file[fd]) {
			fd_to_file[fd] = f;
			f->type = type;
			num_fds++;
			return fd;
		}
	}
	return -1;
}

struct file *map_fd(int fd) {
	return fd_to_file[fd];
}

int free_fd(int fd) {
	if (fd_to_file[fd]) {
		fd_to_file[fd] = 0;
		num_fds--;
		return 0;
	}
	return -1;
}

/* The epoll syscalls */

struct epoll_file {
	struct file base;

	// we need a list of currently polling-for fds
	// and what they poll for
};

int __wrap_epoll_create1(int flags) {
	struct epoll_file *ef = (struct epoll_file *)malloc(sizeof(struct epoll_file));

	int fd = allocate_fd(FD_TYPE_EPOLL, (struct file *)ef);

	//printf("epoll_create1: %d\n", fd);
	return fd;
}

int __wrap_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	printf("epoll_ctl: 0\n");
	return 0;
}

int __wrap_epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout) {
	printf("epoll_wait: %d\n", 0);

	if (consumable_data_length) {


		consumable_data_length=0;



		// return some triggered fds such as the eventfd one!


		// randomly? pick from fds in epoll_ctl-added list

		// hur många ska pickas?
		// vilka ska pickas?

		// detta är i princip den enda drivande randomnessen som finns! allt annat är relativt givet



	} else {

		teardown();
	}

	// grab 1 byte from consumable data

	// if empty, trigger the teardown callback expecting fallthrough



	return 0;
}

/* The socket syscalls */

struct socket_file {
	struct file base;
};

int __wrap_recv() {
	printf("Wrapped recv\n");
	return 0;
}

int __wrap_send() {
	printf("Wrapped send\n");
	return 0;
}

int __wrap_accept4() {
	printf("Wrapped accept4\n");
	return 0;
}

int __wrap_listen() {
	printf("Wrapped listen\n");
	return 0;
}

int __wrap_socket() {
	printf("Wrapped socket\n");
	return 0;
}

int __wrap_shutdown() {
	printf("Wrapped shutdown\n");
	return 0;
}

/* The timerfd syscalls */

struct timer_file {
	struct file base;
};

int __wrap_timerfd_create(int clockid, int flags) {
	struct timer_file *tf = (struct timer_file *)malloc(sizeof(struct timer_file));

	int fd = allocate_fd(FD_TYPE_TIMER, (struct file *)tf);

	//printf("timerfd_create: %d\n", fd);

	return fd;
}

int __wrap_timerfd_settime(int fd, int flags,
                    const struct itimerspec *new_value,
                    struct itimerspec *old_value) {
	//printf("timerfd_settime: %d\n", fd);
	return 0;
}

/* The eventfd syscalls */

struct event_file {
	struct file base;
};

int __wrap_eventfd() {
	//printf("Wrapped eventfd\n");

	struct event_file *ef = (struct event_file *)malloc(sizeof(struct event_file));

	int fd = allocate_fd(FD_TYPE_EVENT, (struct file *)ef);

	printf("eventfd: %d\n", fd);

	return fd;
}

// timerfd_settime

/* File descriptors exist in a shared dimension, and has to know its type */
int __wrap_close(int fd) {
	struct file *f = map_fd(fd);

	if (!f) {
		return -1;
	}

	if (f->type == FD_TYPE_EPOLL) {
		printf("Closing epoll FD\n");

		free(f);

		return free_fd(fd);

	} else if (f->type == FD_TYPE_TIMER) {
		printf("Closing timer\n");

		free(f);

		return free_fd(fd);
	} else if (f->type == FD_TYPE_EVENT) {
		printf("Closing event\n");

		free(f);

		return free_fd(fd);
	} else if (f->type == FD_TYPE_SOCKET) {
		printf("Closing socket\n");

		free(f);

		return free_fd(fd);
	}

	return -1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	set_consumable_data(data, size);

	test();

	if (num_fds) {
		printf("ERROR! Cannot leave open FDs after test!\n");
	}

	return 0;
}
