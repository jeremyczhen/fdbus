Fast Distributed Bus (FDBus)
============================

FDBus is a middleware development framework focusing on the following goals:
- Inter-Process Communication (IPC)
- System abstraction (Windows, Linux, QNX)
- Components based on which middleware is built (job, worker, timer, watch...)

It is something like DBus or SOME/IP, but with its own features:
- Distributed: no central hub like dbus daemon
- High performance: endpoints talk to each other directly
- Logic name for service: service is addressable through logic name
- Address allocation: service address is allocated and maintained
- Networking: communication inside host and cross hosts
- IDL and code generation: protocol buffer is used
- Total slution: it is not only an IPC machanism, but also a middleware development framework

Supported system
----------------
- Linux
- Windows
- QNX

Requirements
------------
- cmake - 3.1.3 or above
- protocol buffer
- compiler supporting C++11
