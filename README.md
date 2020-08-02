# libEpollFuzzer - fuzzing for Linux servers

This mock implementation of the [epoll/socket](https://en.wikipedia.org/wiki/Epoll) syscalls allows you to test intricate edge cases and find bugs in mission critical software - all within minutes. It builds on LLVM's [libFuzzer](http://llvm.org/docs/LibFuzzer.html) and operates based on nothing but fuzz data, being entirely deterministic and reproducible.

## Can you find the bug?

The following code runs fine in most cases but has a critical security bug that can be hard to trigger - can you find it?

```c++
int epfd = epoll_create1();

int lsfd = listen(asdasdasd);

int ready_fd = epoll_wait(epfd, lalalala);

for (all ready fds)

int length = recv(buf, 24234234);

//copy from 0 and length
```

## Let's find it!

gif here of finding the bug

## A more complex case
Fuzzing the entire uSockets library takes no more than a few linker flags.

## How it works

<div align="center">
<img src="epollFuzzer.svg" height="200" />
</div>
