#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/timerfd.h>
#include <sys/epoll.h>

#define printf

/* The test case */
void test();
void teardown();

struct file {
	/* Every file has a type; socket, event, timer, epoll */
	int type;

	/* We assume there can only be one event-loop at any given point in time,
	 * so every file holds its own epoll_event */
	struct epoll_event epev;

	/* A file may be added to an epfd by linking it in a list */
	struct file *prev, *next;
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
	printf("Setting consumable length: %d\n", new_length);
	consumable_data = (unsigned char *) new_data;
	consumable_data_length = new_length;
}

void consume_data() {

}

/* Keeping track of FDs */

int allocate_fd(int type, struct file *f) {
	// this can be massively optimized by having a list of free blocks or the like
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

	/* A doubly linked list for polls awaiting events */
	struct file *poll_set_head, *poll_set_tail;
};

/* This function is O(n) and does not consume any fuzz data, but will fail if run out of FDs */
int __wrap_epoll_create1(int flags) {
	struct epoll_file *ef = (struct epoll_file *)malloc(sizeof(struct epoll_file));

	/* Init the epoll_file */
	//memset(ef->poll_set, 0, 100 * sizeof(struct epoll_event));
	ef->poll_set_head = NULL;
	ef->poll_set_tail = NULL;

	int fd = allocate_fd(FD_TYPE_EPOLL, (struct file *)ef);

	//printf("epoll_create1: %d\n", fd);
	return fd;
}

/* This function is O(1) and does not consume any fuzz data */
int __wrap_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	printf("epoll_ctl: 0\n");

	struct epoll_file *ef = (struct epoll_file *)map_fd(epfd);

	// return if bad epfd

	struct file *f = (struct file *)map_fd(fd);

	// return if bad fd

	/* We add new polls in the head */
	if (op == EPOLL_CTL_ADD) {
		printf("ADDING FD!\n");
		// if there is a head already
		if (ef->poll_set_head) {
			ef->poll_set_head->prev = f;

			// then it will be our next
			f->next = ef->poll_set_head;
		} else {
			// if there was no head then we became the tail also
			ef->poll_set_tail = f;
		}

		// we are now the head in any case
		ef->poll_set_head = f;

		f->epev = *event;

	} else if (op == EPOLL_CTL_MOD) {
		/* Modifying is simply changing the file itself */
		f->epev = *event;
	} else if (op == EPOLL_CTL_DEL) {

		// let's just clear the whole list for now
		ef->poll_set_head = NULL;
		ef->poll_set_tail = NULL;

	}

	return 0;
}

/* This function is O(n) and consumes fuzz data and might trigger teardown callback */
int __wrap_epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout) {
	//printf("epoll_wait: %d\n", 0);

	struct epoll_file *ef = (struct epoll_file *)map_fd(epfd);

	// return if bad ef

	if (consumable_data_length) {




		//printf("ef->poll_set_head: %p\n", ef->poll_set_head);


		int ready_events = 0;

		for (struct file *f = ef->poll_set_head; f; f = f->next) {


			/* Consume one fuzz byte, AND it with the event */
			if (!consumable_data_length) {
				// break if we have no data
				break;
			}

			// here we have the main condition that drives everything
			int ready_event = consumable_data[0] & f->epev.events;

			// consume the byte
			consumable_data_length--;
			consumable_data++;

			if (ready_event) {
				if (ready_events < maxevents) {
					events[ready_events++] = f->epev;
				} else {
					// we are full, break
					break;
				}
			}

		}

		return ready_events;

	} else {
		teardown();
	}

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

int __wrap_read() {
	printf("Wrapped read\n");

	// read is called by the eventfd andexpects 8 bytes, but we should make this more generic
	return 8;
}

int __wrap_send() {
	printf("Wrapped send\n");
	return 0;
}

int __wrap_bind() {
	printf("Wrapped bind\n");
	return 0;
}

int __wrap_setsockopt() {
	printf("Wrapped setsockopt\n");
	return 0;
}

int __wrap_fcntl() {
	printf("Wrapped fcntl\n");
	return 0;
}

int __wrap_getaddrinfo() {
	printf("Wrapped getaddrinfo\n");
	return 0;
}

int __wrap_freeaddrinfo() {
	printf("Wrapped freeaddrinfo\n");
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

	/* Init the file */
	tf->base.prev = 0;
	tf->base.next = 0;

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

	/* Init the file */
	ef->base.prev = 0;
	ef->base.next = 0;

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
