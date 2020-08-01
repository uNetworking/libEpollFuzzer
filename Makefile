default:
	clang -fsanitize=address,fuzzer test.c --for-linker --wrap=epoll_wait --for-linker --wrap=epoll_create1 --for-linker --wrap=timerfd_settime --for-linker --wrap=close --for-linker --wrap=listen --for-linker --wrap=accept4 --for-linker --wrap=eventfd --for-linker --wrap=timerfd_create --for-linker --wrap=epoll_ctl --for-linker --wrap=shutdown -o test uSockets/uSockets.a
