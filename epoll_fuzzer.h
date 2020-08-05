#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/timerfd.h>
#include <sys/epoll.h>

//#define printf

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

/* This one should only return the FD or -1, not do any init */
int allocate_fd(/*int type, struct file *f*/) {
	// this can be massively optimized by having a list of free blocks or the like
	for (int fd = 0; fd < MAX_FDS; fd++) {
		if (!fd_to_file[fd]) {
			//fd_to_file[fd] = f;
			//f->type = type;
			num_fds++;
			return fd;
		}
	}
	return -1;
}

/* This one should set the actual file for this FD */
void init_fd(int fd, int type, struct file *f) {
	fd_to_file[fd] = f;
	fd_to_file[fd]->type = type;
	fd_to_file[fd]->next = NULL;
	fd_to_file[fd]->prev = NULL;
}

struct file *map_fd(int fd) {
	return fd_to_file[fd];
}

/* This one should remove the FD from any pollset by calling epoll_ctl remove */
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
	/* Todo: check that we do not allocate more than one epoll FD */
	int fd = allocate_fd();

	if (fd != -1) {
		struct epoll_file *ef = (struct epoll_file *)malloc(sizeof(struct epoll_file));

		/* Init the epoll_file */
		ef->poll_set_head = NULL;
		ef->poll_set_tail = NULL;

		init_fd(fd, FD_TYPE_EPOLL, (struct file *)ef);
	}

	return fd;
}

// this function cannot be called inside an iteration! it changes the list
/* This function is O(1) and does not consume any fuzz data */
int __wrap_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	printf("epoll_ctl: 0\n");

	struct epoll_file *ef = (struct epoll_file *)map_fd(epfd);

	// return if bad epfd
	if (!ef) {
		return -1;
	}

	struct file *f = (struct file *)map_fd(fd);

	// return if bad fd
	if (!f) {
		return -1;
	}

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

		if (f->prev) {
			f->prev->next = f->next;
		} else {
			ef->poll_set_head = f->next;
		}

		if (f->next) {
			f->next->prev = f->prev;
		} else {
			// tail ska vara vår.prev
			ef->poll_set_tail = f->prev;
		}

	}

	return 0;
}

/* This function is O(n) and consumes fuzz data and might trigger teardown callback */
int __wrap_epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout) {
	//printf("epoll_wait: %d\n", 0);

	printf("Calling epoll_wait\n");

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
					events[ready_events] = f->epev;

					// todo: the event should be masked by the byte, not everything it wants shold be given all the time!
					events[ready_events++].events = ready_event;
				} else {
					// we are full, break
					break;
				}
			}

		}

		return ready_events;

	} else {

		printf("Calling teardown\n");
		teardown();

		// after shutting down the listen socket we clear the whole list (the bug in epoll_ctl remove)
		// so the below loop doesn't work - we never close anything more than the listen socket!

		/* You don't really need to emit teardown, you could simply emit error on every poll */

		int ready_events = 0;

		printf("Emitting error on every remaining FD\n");
		for (struct file *f = ef->poll_set_head; f; f = f->next) {

			if (f->type == FD_TYPE_SOCKET) {

				if (ready_events < maxevents) {
					events[ready_events] = f->epev;

					// todo: the event should be masked by the byte, not everything it wants shold be given all the time!
					events[ready_events++].events = EPOLLERR | EPOLLHUP;
				} else {
					// we are full, break
					break;
				}

			}
		}

		printf("Ready events: %d\n", ready_events);

		return ready_events;


		/* For all FDs still in epoll pollset, emit error! */

		printf("Emitting error on every remaining FD\n");
		for (struct file *f = ef->poll_set_head; f; f = f->next) {

		}
	}

	return 0;
}

/* The socket syscalls */

struct socket_file {
	struct file base;
};

#include <string.h>

int __wrap_read(int fd, void *buf, size_t count) {
	printf("Wrapped read\n");


	struct file *f = map_fd(fd);

	if (!f) {
		return -1;
	}

	if (f->type == FD_TYPE_SOCKET) {
		printf("Reading from a socket!\n");

		if (!consumable_data_length) {
			return 0;
		} else {
			int data_available = (unsigned char) consumable_data[0];
			consumable_data_length--;
			consumable_data++;

			if (consumable_data_length < data_available) {
				data_available = consumable_data_length;
			}

			if (count < data_available) {
				data_available = count;
			}

			memcpy(buf, consumable_data, data_available);

			consumable_data_length -= data_available;
			consumable_data += data_available;

			return data_available;
		}
	}

	if (f->type == FD_TYPE_EVENT) {
		printf("Reading from an eventfd!\n");

		return 8;
	}

	if (f->type == FD_TYPE_TIMER) {
		printf("Reading from a timer!\n");

		return 8;
	}

	return -1;
}

int __wrap_recv(int sockfd, void *buf, size_t len, int flags) {
	return __wrap_read(sockfd, buf, len);
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

  #include <sys/types.h>
       #include <sys/socket.h>
       #include <netdb.h>

/* Addrinfo */
int __wrap_getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints,
                       struct addrinfo **res) {
	printf("Wrapped getaddrinfo\n");


	// we need to return an addrinfo with family AF_INET6

	static struct addrinfo ai;

	//ai.ai_next = NULL;

	ai.ai_family = AF_INET;//hints->ai_family;
	ai.ai_flags = hints->ai_flags;
	ai.ai_socktype = hints->ai_socktype;
	ai.ai_protocol = hints->ai_protocol;

	ai.ai_addrlen = 4; // fel
	ai.ai_next = NULL;
	ai.ai_canonname = "";

	ai.ai_addr = NULL; // ska peka på en sockaddr!

	*res = &ai;
	return 0;
}

int __wrap_freeaddrinfo() {
	printf("Wrapped freeaddrinfo\n");
	return 0;
}

int __wrap_accept4() {
	/* We must end with -1 since we are called in a loop */

	if (consumable_data_length < 0) {
		printf("ACCEPT4 FEL PÅ CONSUMABLE LENGTH!\n");
		exit(33);
	}

	/* Read one byte, if it is null then we have a new socket */
	if (consumable_data_length) {

		int byte = consumable_data[0];

		/* Consume the fuzz byte */
		consumable_data_length--;
		consumable_data++;

		/* Okay, so do we have a new connection? */
		if (!byte) {

			int fd = allocate_fd();

			if (fd != -1) {

				/* Allocate the file */
				struct socket_file *sf = (struct socket_file *) malloc(sizeof(struct socket_file));

				/* Init the file */

				/* Here we need to create a socket FD and return */
				init_fd(fd, FD_TYPE_SOCKET, (struct file *)sf);

			}

			printf("accept4 returning fd: %d\n", fd);
			return fd;
		} else {
			return -1;
		}
	}

	/* If we have no consumable data we cannot return a socket */
	return -1;
}

int __wrap_listen() {
	printf("Wrapped listen\n");
	return 0;
}

/* This one is similar to accept4 and has to return a valid FD of type socket */
int __wrap_socket() {
	int fd = allocate_fd();

	if (fd != -1) {
		struct socket_file *sf = (struct socket_file *)malloc(sizeof(struct socket_file));

		/* Init the file */

		init_fd(fd, FD_TYPE_SOCKET, (struct file *)sf);
	}

	return fd;
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

	int fd = allocate_fd();

	if (fd != -1) {
		struct timer_file *tf = (struct timer_file *)malloc(sizeof(struct timer_file));

		/* Init the file */


		init_fd(fd, FD_TYPE_TIMER, (struct file *)tf);

	}

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

	int fd = allocate_fd();

	if (fd != -1) {
		struct event_file *ef = (struct event_file *)malloc(sizeof(struct event_file));

		/* Init the file */

		init_fd(fd, FD_TYPE_EVENT, (struct file *)ef);

		//printf("eventfd: %d\n", fd);
	}

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

		// we should call epoll_ctl remove here

		free(f);

		int ret = free_fd(fd);


		printf("Ret: %d\n", ret);

		//free(-1);
		return ret;
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
