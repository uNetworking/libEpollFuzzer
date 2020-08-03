/* We need to include the APIs to fuzz within */
#include "epoll_fuzzer.h"

/* Our test fuzz uSockets so we need its APIs */
#include "uSockets/src/libusockets.h"

void wakeup_cb(struct us_loop_t *loop) {
	printf("EVENTFD TRIGGERED!\n");
	exit(0);
}

void pre_cb(struct us_loop_t *loop) {
	//printf("Inside pre_cb\n");
}

void post_cb(struct us_loop_t *loop) {
	//printf("Inside post_cb\n");
}

struct us_timer_t *timer;

/* We define a test that deterministically sets up and tears down an uSockets event-loop */
void test() {
	printf("Entering test\n");

	struct us_loop_t *loop = us_create_loop(0, wakeup_cb, pre_cb, post_cb, 0);

	timer = us_create_timer(loop, 0, 0);

	/*struct us_socket_context_options_t context_options = {};
	struct us_socket_context_t *context = us_create_socket_context(0, loop, 0, context_options);

	// listen either succeeds or fails
	struct us_listen_socket_t *listen_socket = us_socket_context_listen(0, context, 0, 3001, 0, 0);

	if (listen_socket) {
		us_listen_socket_close(0, listen_socket);
	} else {
		printf("Failed to listen!\n");
	}*/

	us_loop_run(loop);

	//us_socket_context_free(0, context);
	us_loop_free(loop);

	printf("Leaving test\n");
}

/* Thus function should shutdown the event-loop and let the test fall through */
void teardown() {
	us_timer_close(timer);
}