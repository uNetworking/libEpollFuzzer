# libEpollFuzzer - fuzzing for Linux servers

This mock implementation of the epoll/socket syscalls allows you to test intricate edge cases and find bugs in mission critical software - all within minutes.

<div align="center">
<img src="epollFuzzer.svg" height="200" />
</div>

## Can you find the bug?

The following code runs fine in most cases but has a critical security bug - can you find it?

```c++
int epfd = epoll_create1();

int lsfd = listen(asdasdasd);

int ready_fd = epoll_wait(epfd, lalalala);

for (all ready fds)

int length = recv(buf, 24234234);

//copy from 0 and length
```
