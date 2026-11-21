# Memory Protection

With `CONFIG_MPU` the kernel runs tasks unprivileged and fences each one into its own memory. A task that breaks the rules is stopped and the rest of the system carries on. The option is off by default, and a build without it is the same as before.

## What it gives

| Rule | How it is enforced |
|------|--------------------|
| A task writes only to its own stack, the shared user data section and regions it was granted | MPU on Cortex-M, PMP on RISC-V, page tables on x86 |
| Other tasks' stacks and all kernel data are out of reach | same |
| Code is read-only and only code can run | MPU and PMP regions, on x86 a code segment limit and read-only pages |
| Tasks reach the kernel only through system calls | privilege level: Thread unprivileged on Cortex-M, U-mode on RISC-V, ring 3 on x86 |
| The kernel survives a task's mistake | the fault stops the task, prints a report and the scheduler picks another one |

## Targets

| Target | Mechanism | Verified in |
|--------|-----------|-------------|
| Cortex-M3, M4 (LM3S6965, STM32F4) | ARMv7-M MPU, `SVC` | QEMU (`lm3s6965evb`, `olimex-stm32-h405`) |
| RISC-V RV32 | PMP, `ecall` | QEMU `virt` |
| x86 (32-bit) | paging, TSS, `int 0x80` | QEMU |
| Cortex-M0+ (RP2040) | ARMv6-M MPU, `SVC` | not run: it builds, but there is no model to run it on |

STM32F103 (Cortex-M3 without an MPU) can build with `CONFIG_MPU` off only.

## Writing unprivileged code

```c
#include "bedrock/bedrock.h"

BR_USER_STACK(stack, 1024);
BR_USER_BSS static uint32_t shared;

static void worker(void *arg)
{
    int sem = br_user_sem_create(0, 1);
    br_user_uart_puts("hello\n");
    br_user_exit();
}

br_task_create_user(&tid, "worker", worker, NULL, 2, stack, sizeof(stack));
br_task_grant(tid, (void *)0x4000C000, 4096, BR_MEM_R | BR_MEM_W);
```

| Function or macro | Meaning |
|-------------------|---------|
| `br_task_create_user(...)` | Like `br_task_create`, for a task that runs unprivileged. `BR_ERR_INVALID` if the stack does not satisfy the rules below, `BR_ERR_NOMEM` if there is no free kernel stack (`CONFIG_MAX_USER_TASKS`). |
| `br_task_grant(tid, base, size, perms)` | Let the task use a memory range, for example a peripheral. Call it before the task runs. `perms` is `BR_MEM_R`, `BR_MEM_W` or both. At most `BR_TASK_REGIONS - 1` (3) grants. |
| `BR_USER_STACK(name, size)` | A stack that is aligned to its size, as the MPU needs. |
| `BR_USER_BSS` | Puts a variable in the user data section, which all unprivileged tasks share. The section is zeroed at start. Initializers are ignored, assign at run time. |

### Rules for a user stack

- Cortex-M and RISC-V: the size is a power of two and the base is a multiple of the size. The minimum is 512 bytes.
- x86: the size is a multiple of 4096 and the base is a multiple of 4096.
- The lowest eighth of a stack is a guard. The task cannot use it, and the kernel does not count it as the task's memory. On Cortex-M it keeps the context that the kernel saves for a task that has overflowed its stack from spilling into the next object.
- The user data section is 4096 bytes. The linker script reserves it and fails the build if it grows.

### Granted regions

| Target | Alignment and size |
|--------|--------------------|
| Cortex-M | power of two, at least 32 bytes (256 on Cortex-M0+), aligned to its size |
| RISC-V | power of two, at least 8 bytes, aligned to its size |
| x86 | whole pages inside the first 4 MiB |

## System calls

An unprivileged task calls the `br_user_*` functions from `include/bedrock/br_user.h`. They issue the system call instruction and the kernel checks every argument before it acts.

| Area | Calls |
|------|-------|
| Task | `br_user_self`, `br_user_yield`, `br_user_sleep_us`, `br_user_uptime_us`, `br_user_exit` |
| Semaphore | `br_user_sem_create`, `br_user_sem_take`, `br_user_sem_give` |
| Mutex | `br_user_mutex_create`, `br_user_mutex_lock`, `br_user_mutex_unlock` |
| Message queue | `br_user_mqueue_create`, `br_user_mqueue_send`, `br_user_mqueue_recv` |
| UART | `br_user_uart_putc`, `br_user_uart_puts`, `br_user_uart_getc` |

Objects are owned by the kernel. A task gets a handle (a small integer), never a pointer, so a task cannot corrupt an object by writing to it. The kernel has a static pool of `CONFIG_USER_OBJECTS` objects of each type, and message queues take their buffers from an arena of `CONFIG_USER_MQ_ARENA` bytes. Objects are not freed.

Pointers passed to the kernel are checked against the memory the task may use: a pointer outside it gives `BR_ERR_PERM`. A bad handle gives `BR_ERR_INVALID`. `br_user_uart_puts` reads at most 256 bytes.

### Device, file system and network calls

These are not built into the kernel. Each module registers its own range of call numbers, so a firmware that does not link the file system or the network pays nothing for them.

```c
br_device_enable_user_calls();   /* kernel/br_device.c */
br_fs_enable_user_calls();       /* lib/br_fs.c */
br_net_enable_user_calls();      /* ports/lwip/br_net_sock.c */
```

| Area | Calls |
|------|-------|
| Device | `br_user_device_open`, `_close`, `_read`, `_write`, `_ioctl` |
| File system | `br_user_fs_open`, `_read`, `_write`, `_seek`, `_close`, `_remove`, `_stat`, `_mkdir`, `_rmdir`, `_info`, `_readdir`, `_usage` |
| Network | `br_user_net_socket`, `_bind`, `_listen`, `_accept`, `_connect`, `_send`, `_recv`, `_sendto`, `_recvfrom`, `_close`, `_ping`, `_resolve`, `_ifinfo` |

Rules the kernel enforces:

- A device is open to unprivileged tasks only after `br_device_set_user(dev, true)`. Names are looked up by the kernel, never by pointer.
- `ioctl` goes through a whitelist: a command is allowed only if a driver listed it with `br_device_user_allow_ioctl(cmd, size)`, and the argument must have exactly that size. Commands whose argument holds pointers are not listed, so the kernel never follows a pointer inside a structure. The bundled GPIO, SPI, I2C, PWM and capture drivers list theirs.
- Descriptors belong to the task that made them. Another task using or closing them gets `BR_ERR_INVALID`.
- When a task is deleted, its devices, files and sockets are closed (`br_user_release_register` lets a module add its own cleanup).
- Every buffer and string is checked as for the other calls. Strings are copied into the kernel before use.
- Calls that need more than four values pass a small block of words in the task's memory. Out parameters (addresses, ports, sizes, info structures) are written by the kernel only after the pointer is checked.
- Addresses are `br_ip_t` (IPv4 or IPv6), passed by pointer, and the pointer must be aligned to 4 bytes. `br_user_net_socket` takes a family and a type. `br_user_net_accept` and `_recvfrom` return the port in a `uint32_t`.

A call to a module that was not enabled returns `BR_ERR_INVALID`.

### How a call works

A system call is not run in the exception handler, because it may block. The handler instead moves the task onto its own kernel stack, switches it to privileged mode and returns into `br_syscall_dispatch`. The kernel function then runs like it would for any privileged task, and can sleep. When it is done, a second system call returns to the user context with the result. A task that is interrupted in the middle of a call keeps its kernel stack, so preemption works.

`CONFIG_KSTACK_SIZE` (768 bytes) is the size of that stack, one per unprivileged task.

## Faults

When an unprivileged task touches memory it may not, the CPU raises a fault. The handler prints a report to the UART, marks the task `faulted`, suspends it and calls the scheduler:

```
*** TASK FAULT ***
Task:   faulty
Kind:   MemManage
Address: 0x20000000
PC:      0x000001ee
CFSR:    0x00000082
******************
```

`br_task_get_info()` reports `faulted` and `user`. A faulted task stays suspended and can be deleted. Resuming it runs the faulting instruction again.

A fault in the kernel, or in a task that runs privileged, is fatal. The handler prints the registers and panics. This applies to all Cortex-M builds, with or without `CONFIG_MPU`, and to RISC-V and x86. The Cortex-M3/M4 report has the stacked registers, `CFSR`, `HFSR` and the fault address, the RISC-V report has `mcause`, `mepc` and `mtval`, the x86 report has the vector, the error code and `eip`.

## Per architecture

### Cortex-M

Regions 0 to 7: code (read-only, from the linker symbols `_mpu_code_base` and `_mpu_code_size`), the user data section, the task stack, three grants. `PRIVDEFENA` lets privileged code use the default map. The MPU is reprogrammed in `PendSV` for the incoming task, and `CONTROL.nPRIV` is set from the task. `SVC` 2 enters a system call, `SVC` 3 leaves it. The context switch hook uses the address of the task control block, which is the address of its saved stack pointer.

On Cortex-M0+ (ARMv6-M) there is only `HardFault`, so the report has no fault address.

### RISC-V

PMP entries: 0 and 1 code, 2 user data, 4 and 5 stack (top-of-range, without the guard), 6 to 8 grants. Machine mode ignores the unlocked entries. A trap from user mode swaps `sp` with `mscratch` and runs on the task's kernel stack. `ecall` from user mode enters a system call, `ecall` from machine mode with `a7` set to `0x7FFF` leaves it.

### x86

A flat segment model with a user code segment whose limit ends at the end of the code, which keeps data from running. Page tables map the first 4 MiB. Kernel pages are supervisor-only, code pages are user read-only, the user data section is user read-write. At each switch the stack and grant pages of the task that stops lose the user bit and those of the incoming task get it. The `esp0` field of the TSS points to the kernel stack of the incoming task. `int 0x80` enters a system call and `int 0x81` leaves it. The image must fit in the first 4 MiB.

## Configuration

| Option | Default | Meaning |
|--------|---------|---------|
| `CONFIG_MPU` | off | Enable the feature |
| `CONFIG_MAX_USER_TASKS` | 4 | Unprivileged tasks, one kernel stack each |
| `CONFIG_USER_OBJECTS` | 8 | Semaphores, mutexes and queues for each type |
| `CONFIG_USER_MQ_ARENA` | 512 | Bytes for message queue buffers |
| `CONFIG_KSTACK_SIZE` | 768 | Kernel stack of an unprivileged task |

The Cortex-M and RISC-V linker scripts of the LM3S6965, STM32F4, RP2040 and `virt` boards and the x86 script reserve a 4 KiB aligned section for the shared user data. That costs up to 8 KiB of RAM even when `CONFIG_MPU` is off.

## Tests

`test_mpu` runs on the Cortex-M3 (QEMU LM3S6965), the Cortex-M4 (QEMU STM32-H405), RISC-V and x86 builds with `CONFIG_MPU` on, and builds for RP2040. It checks the system calls between two unprivileged tasks, a write and a read of kernel data, a peripheral without a grant, a write to code, a write to another task's stack, a stack overflow, running code from data, granted regions, the limit of grants, and the rules for stacks. After all violations it checks that kernel data did not change. It then enables the device and file system calls and checks them with a mock device and a RAM disk: reads, writes, a whitelisted and a refused ioctl, a kernel pointer as a buffer and as a name, handles used by a task that does not own them, and cleanup when a task is deleted. The network calls are only compiled in this test, see Limits.

Renode stops this test on the Cortex-M4: its MPU model does not seem to honor the subregion that guards the stack, and the saved context of a task that overflowed its stack overwrites a neighbor. QEMU follows the architecture, so CI runs `test_mpu_f4` there.

## Limits

- The shell and the libraries other than the file system and the network are not reachable from user mode.
- The network calls are built with every network firmware but no test drives them from an unprivileged task yet.
- There is one hart or one core and no cache coherency issues. SMP is not supported.
- x86 has no NX bit in this port, so only the segment limit keeps data from running, and the first 4 MiB are all that is mapped.
- Cortex-M0+ is untested.
- An unprivileged task cannot create or delete tasks, and cannot free kernel objects.
- No W^X guarantee beyond what the hardware gives, and no ASLR.
