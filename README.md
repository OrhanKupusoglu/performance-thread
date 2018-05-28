# System Performance

A common need is to check the system performance before starting a heavy-duty task. In Linux, the pseudo-files under the **/proc** directory is filled by kernel with performance parameters. Theoretically all answers to questions about current system load can be answered by reading these parameters and by processing  these data with some user-defined thresholds.

## The /proc File-System
The **/proc** file system is accessed by the kernel using **VFS (Virtual File System)**. Although most of the files in **/proc** are empty, they actually contain in-memory data.

The manual page for **proc** states that:

> The proc filesystem is a pseudo-filesystem which provides an interface to kernel data structures.

See [man proc](http://man7.org/linux/man-pages/man5/proc.5.html) for further info.

The library collects data from the four **/proc** pseudo-files given below.

### /proc/loadavg
The first three fields in this file are load average figures giving the number of jobs in the run queue (state R) or waiting for disk I/O (state D) averaged over 1, 5, and 15 minutes

### /proc/stat
The amount of time, measured in units of USER_HZ (1/100ths of a second on most architectures, use sysconf(_SC_CLK_TCK) to obtain the right value), that the system ("cpu" line) or the specific CPU ("cpuN" line) spent in various states:

```
us, user    : time running un-niced user processes
sy, system  : time running kernel processes
ni, nice    : time running niced user processes
id, idle    : time spent in the kernel idle handler
wa, IO-wait : time waiting for I/O completion
hi : time spent servicing hardware interrupts
si : time spent servicing software interrupts
st : time stolen from this vm by the hypervisor
```

See the **%Cpu(s)** line of the **top** output.

See [man top](http://man7.org/linux/man-pages/man1/top.1.html) for further info.

### /proc/meminfo
This file reports statistics about memory usage on the system.
See the two lines starting with **KiB Mem** line of the **top** output.

```
$ top

top - 23:49:54 up 18 min,  2 users,  load average: 0,49, 0,39, 0,33
Tasks: 274 total,   1 running, 273 sleeping,   0 stopped,   0 zombie
%Cpu(s):  0,9 us,  0,9 sy,  0,0 ni, 98,1 id,  0,0 wa,  0,0 hi,  0,0 si,  0,0 st
KiB Mem : 24581148 total, 20272608 free,  2317568 used,  1990972 buff/cache
KiB Swap: 25059324 total, 25059324 free,        0 used. 21433828 avail Mem

```

### /proc/net/dev
The dev pseudo-file contains network device status information.  This gives the number of received and sent packets, the number of errors and collisions and other basic statistics.

## pthreads &mdash; POSIX Threads

The library is meant to be used in a **pthread** so that the calling thread can receive data about the system load in a timely manner.

See [man pthreads](http://man7.org/linux/man-pages/man7/pthreads.7.html) for further info.

The **debug** output displays data from the four **/proc** pseudo-files described above. This data is printed to stdout.

A pthread can be created in two states:

* **Joinable**

The calling thread waits for until the thread finishes.

See [man pthread_join](http://man7.org/linux/man-pages/man3/pthread_join.3.html) for further info.

* **Detached**

The thread runs in parallel with the calling thread.

See [man pthread_create](http://man7.org/linux/man-pages/man3/pthread_create.3.html) for further info.


## The Sample Application

The sample application has a simple [configuration file](./application/bin/prf_system.cfg) which consists of Bash style **# comments** and **name=value** pairs.

```
$ cat prf_system.cfg
# name=value
#
is_debug=true
is_joinable=true
thread_name=prfthread
interval_s=3
interval_ms=0
cpu_name=cpu
cpu_load_type=5
cpu_threshold=0.70
interface_name=wlp2s0

```
The sample application can create a pthread either as **joinable** or **detached**. In the detached mode the current application can use the **cpu_threshold** parameter, which is the system average load of the last **cpu_load_type** minutes,  to receive data back from the pthread.

With the **cpu_name** parameter a certain CPU, like **cpu6** can be specified. The **cpu_load_type** parameter can be either **1, 5, or 15**, referring to load averages over 1, 5, and 15 minutes. The **cpu_threshold** parameter sets the threshold to decide whether the system is overloaded or not.

Disabling **debug** removes clutter and only leaves the **cpu_threshold** value.

A **SIGINT** signal, **CTRL + C**, terminates the application.

## CMake

This project consists of a library to read **proc** data and a sample application to use this library. Both are built by [CMake](https://cmake.org/). For convenience a build script is provided:

```
$ ./build.sh help
USAGE: enter a command, no command defaults to 'build'
    build         -- call 'lib cmake; lib make; app cmake; app make;'
    clean         -- call 'lib clean; app clean;'
    prune         -- call 'lib prune; app prune;'
    purge         -- call 'clean; prune;'
    run           -- run the test executable
    help          -- print this help
    lib cmake     -- call 'cmake'
    lib make      -- call 'make; make install'
    lib clean     -- call 'make clean'
    lib prune     -- prune all artifacts
    app cmake     -- call 'cmake'
    app make      -- call 'make'
    app clean     -- call 'make clean'
    app prune     -- prune all artifacts

$ ./build.sh

$ ./build.sh run

-- joined: 0.14 | is overloaded? false

$ ./build.sh run

== detached: 0.17 | is overloaded? false

```





