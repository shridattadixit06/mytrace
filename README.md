# mytrace

`mytrace` is a small educational Linux system-call tracer written in C using the `ptrace()` API.

The project is being developed to understand how tools such as `strace` work internally — particularly how a tracer can observe system calls, inspect registers, read memory from another process, and handle signals.

> **Status:** Early development — currently supports a small set of system calls and basic signal tracing.

---

## Features

Currently, `mytrace` can:

* Create a tracee process using `fork()`
* Attach the child to the tracer using `PTRACE_TRACEME`
* Stop the tracee before execution using `SIGSTOP`
* Trace system-call entry and exit using `PTRACE_SYSCALL`
* Inspect x86-64 registers using `PTRACE_GETREGS`
* Detect system-call stops using `PTRACE_O_TRACESYSGOOD`
* Decode a small set of system calls
* Display system-call arguments
* Read string arguments from tracee memory
* Decode common `openat()` flags
* Read data returned by `read()`
* Detect and display signals received by the tracee
* Handle normal process exit and signal termination

---

## Currently Supported System Calls

| Syscall  | Number | Arguments                    |
| -------- | -----: | ---------------------------- |
| `read`   |    `0` | fd, buffer, count            |
| `write`  |    `1` | fd, buffer, count            |
| `openat` |  `257` | dirfd, pathname, flags, mode |

The syscall information is stored in a table so that additional system calls can be added later.

---

## Example Output

A typical trace may look like:

```text
Parent: I am the tracer
Parent PID: 84769

Child: I am the tracee
Child PID: 84770

[SIGNAL STOP] SIGSTOP (19)

[SYSCALL ENTRY] openat
arg count = 4
arg1 = -100
arg2 = "/etc/ld.so.cache"
arg3=O_RDONLY
arg4 = 0

[SYSCALL EXIT] openat -> 3

[SYSCALL ENTRY] read
arg count = 3
arg1 = 3
arg2 = 0x7f...
arg3 = 832

[SYSCALL EXIT] read -> 832
Read data: ...

[SYSCALL ENTRY] write
arg count = 3
arg1 = 1
arg2 = 0x7f...
arg3 = 1024

[SYSCALL EXIT] write -> 1024
```

The exact output depends on the system calls performed by the tracee.

---

## How It Works

At a high level, `mytrace` follows this process:

```text
                ┌──────────────┐
                │    tracer    │
                │   (parent)   │
                └──────┬───────┘
                       │
                     fork()
                       │
              ┌────────┴────────┐
              │                 │
              ▼                 ▼
        ┌───────────┐     ┌───────────┐
        │  Parent   │     │   Child   │
        │  Tracer   │     │  Tracee   │
        └───────────┘     └─────┬─────┘
                                │
                         PTRACE_TRACEME
                                │
                            SIGSTOP
                                │
                         ┌──────▼──────┐
                         │    exec     │
                         │     ls      │
                         └──────┬──────┘
                                │
                         system calls
                                │
                                ▼
                         ┌─────────────┐
                         │    tracer   │
                         │ observes    │
                         │ entry/exit  │
                         └─────────────┘
```

The tracer repeatedly waits for the tracee to stop:

```text
waitpid()
    ↓
inspect stop reason
    ↓
PTRACE_GETREGS
    ↓
identify syscall / signal
    ↓
print information
    ↓
PTRACE_SYSCALL
    ↓
wait again
```

---

## System Call Tracing

On x86-64 Linux, system-call arguments are passed through registers.

`mytrace` currently reads them from:

```text
rdi → argument 1
rsi → argument 2
rdx → argument 3
r10 → argument 4
r8  → argument 5
r9  → argument 6
```

The system-call number is obtained from:

```c
regs.orig_rax
```

and the return value is obtained from:

```c
regs.rax
```

This allows the tracer to distinguish between system-call entry and exit.

---

## System Call Entry / Exit

`mytrace` maintains an `in_syscall` state:

```text
in_syscall = 0
        ↓
  syscall entry
        ↓
in_syscall = 1
        ↓
  syscall exit
        ↓
in_syscall = 0
```

This is necessary because `PTRACE_SYSCALL` causes the tracee to stop around both the entry and exit of a system call.

---

## Reading Tracee Memory

Some system-call arguments are pointers.

For example:

```c
openat(..., "/etc/file", ...)
```

The pathname is stored in the tracee's address space rather than directly inside the register.

`mytrace` uses:

```c
process_vm_readv()
```

to copy memory from the tracee into the tracer.

This is used for:

* String arguments such as `openat()` paths
* Data returned by `read()`

---

## Signal Tracing

The tracer also detects signal stops.

For example:

```text
[SIGNAL STOP] SIGSTOP (19)
```

Signals currently recognized include:

```text
SIGINT
SIGTERM
SIGQUIT
SIGSEGV
SIGSTOP
SIGCONT
SIGTSTP
SIGTRAP
```

Unknown signals are reported as:

```text
UNKNOWN
```

The tracer also distinguishes syscall stops from ordinary signal stops using:

```c
SIGTRAP | 0x80
```

when `PTRACE_O_TRACESYSGOOD` is enabled.

---

## Project Structure

The project is currently intentionally small:

```text
mytrace/
│
├── main.c
├── mytrace
└── README.md
```

More components may be introduced as the tracer grows.

---

## Building

Compile with:

```bash
gcc -Wall -Wextra -o mytrace main.c
```

Run:

```bash
./mytrace
```

`mytrace` currently executes `ls` as its tracee.

---

## Requirements

* Linux
* x86-64 system
* GCC
* Linux `ptrace()` support
* `process_vm_readv()` support

The current register handling is specifically written for the x86-64 Linux syscall ABI.

---

## Important Concepts

This project is primarily an exploration of Linux internals.

The main concepts involved are:

* Processes
* `fork()`
* `exec()`
* `waitpid()`
* Signals
* `ptrace()`
* System-call entry/exit
* x86-64 calling conventions
* CPU registers
* Process memory
* `process_vm_readv()`
* File descriptors
* Linux syscall ABI

---

## Why Build mytrace?

`strace` is an extremely powerful tool, but using it directly hides much of the mechanism underneath.

Building a simplified tracer from scratch provides a practical way to understand:

```text
User program
     ↓
libc / syscall interface
     ↓
CPU registers
     ↓
syscall instruction
     ↓
Linux kernel
     ↓
system call
```

and how another process can observe this interaction through `ptrace()`.

---

## Current Limitations

This is an educational tracer and is **not intended to replace `strace`**.

Current limitations include:

* Only a small number of syscalls are decoded
* The tracee command is currently hard-coded to `ls`
* Limited argument decoding
* Limited flag decoding
* No complete syscall database
* No command-line interface for choosing the tracee
* No filtering by syscall
* No timestamps
* No syscall statistics
* No multi-process tracing
* No advanced memory/string safety handling
* Architecture-specific register handling

These limitations are intentional for the current stage of development.

---

## Future Improvements

Planned improvements include:

* [ ] Add more system calls
* [ ] Add command-line arguments
* [ ] Trace arbitrary programs
* [ ] Improve string/pointer handling
* [ ] Decode more syscall flags
* [ ] Add syscall filtering
* [ ] Add timestamps
* [ ] Add syscall statistics
* [ ] Trace child processes
* [ ] Improve signal handling
* [ ] Add memory inspection utilities
* [ ] Improve output formatting
* [ ] Eventually build a more `strace`-like interface

The project will evolve incrementally as more Linux internals are explored.

---

## Learning Goal

`mytrace` is not just a utility.

It is a systems-programming project intended to progressively explore:

```text
C
 ↓
Linux processes
 ↓
Signals
 ↓
System calls
 ↓
ptrace()
 ↓
Process memory
 ↓
CPU registers
 ↓
Linux kernel internals
 ↓
Operating-system research
```

---

## License

This project is currently intended as an educational/open-source project.

License information can be added when the project is formally released.
