# libEpollFuzzer - fuzzing for async apps


This mock implementation of the epoll syscalls are made to deterministically consume fuzz data from LLVM's libFuzzer. Outcome is thus entirely driven by data, so found bugs can be replayed in verbatim as many times you need.

<div align="center">
<img src="epollFuzzer.svg" height="200" />
</div>

## In practise

Here's an example test case where the original uSockets source code is being fuzzed deterministically.

test.cpp:
```c++
/* All test cases rely on epoll_fuzzer.h (this library) */
#include "epoll_fuzzer.h"

/* Since we want to test uSockets, we need to include its APIs */
#include "uSockets/src/libusockets.h"

void wakeup_cb(struct us_loop_t *loop) {

}

void pre_cb(struct us_loop_t *loop) {

}

void post_cb(struct us_loop_t *loop) {

}

/* We use this timer to keep the event-loop open */
struct us_timer_t *timer;

/* We define a test that deterministically sets up and tears down an uSockets event-loop */
void test() {
  /* We create a new event-loop */
	struct us_loop_t *loop = us_create_loop(0, wakeup_cb, pre_cb, post_cb, 0);

  /* Create a timer that is not fall-through (see uSockets docs) */
	timer = us_create_timer(loop, 0, 0);

  /* Run the event-loop until teardown is called */
	us_loop_run(loop);

  /* When we falled through from above call, clean up the loop */
	us_loop_free(loop);
}

/* Thus function should shutdown the event-loop and let the test fall through */
void teardown() {
  /* It is time to gracefully shut down, so close our timer and let the test case fall through */
	us_timer_close(timer);
}
```

The test case is compiled and linked as per Makefile.
