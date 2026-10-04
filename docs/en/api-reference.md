# API Reference

All public functions and types are declared in `include/bedrock/bedrock.h`.

## Types

### `br_time_t`

```c
typedef uint64_t br_time_t;
```

64-bit microsecond timestamp. Range: ~584,000 years.

**Constants:**

| Name | Value | Description |
|------|-------|-------------|
| `BR_TIME_INFINITE` | `UINT64_MAX` | Wait forever (no timeout) |

**Conversion macros:**

| Macro | Description |
|-------|-------------|
| `BR_USEC(us)` | Microseconds to `br_time_t` |
| `BR_MSEC(ms)` | Milliseconds to `br_time_t` |
| `BR_SEC(s)` | Seconds to `br_time_t` |

### `br_err_t`

```c
typedef enum {
    BR_OK            =  0,
    BR_ERR_INVALID   = -1,
    BR_ERR_NOMEM     = -2,
    BR_ERR_TIMEOUT   = -3,
    BR_ERR_BUSY      = -4,
    BR_ERR_ISR       = -5,
    BR_ERR_OVERFLOW  = -6
} br_err_t;
```

### `br_tid_t`

```c
typedef uint8_t br_tid_t;
```

Task identifier. Range: 0 to `CONFIG_MAX_TASKS - 1`.

### `br_task_entry_t`

```c
typedef void (*br_task_entry_t)(void *arg);
```

Task entry point signature. The function receives the `arg` pointer passed to `br_task_create()`.

## Version

Declared in `include/bedrock/br_version.h`, included by `bedrock.h`.

| Macro | Value |
|-------|-------|
| `BR_VERSION_MAJOR` | Major version |
| `BR_VERSION_MINOR` | Minor version |
| `BR_VERSION_PATCH` | Patch version |
| `BR_VERSION` | `(MAJOR << 16) \| (MINOR << 8) \| PATCH`, for comparisons in `#if` |

```c
#if BR_VERSION >= ((0 << 16) | (1 << 8) | 0)
#endif
```

## Kernel Lifecycle

### `br_kernel_init`

```c
void br_kernel_init(void);
```

Initialize the kernel. Must be called before any other bedrock API. Initializes the HAL, scheduler, and creates the idle task.

### `br_kernel_start`

```c
void br_kernel_start(void) __attribute__((noreturn));
```

Start the scheduler. Picks the highest-priority ready task and begins execution. This function never returns.

## Task Management

### `br_task_create`

```c
br_err_t br_task_create(br_tid_t *tid,
                        const char *name,
                        br_task_entry_t entry,
                        void *arg,
                        uint8_t priority,
                        void *stack,
                        size_t stack_size);
```

Create a new task.

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `tid` | Output: task ID (may be `NULL` if not needed) |
| `name` | Task name (stored as pointer, not copied) |
| `entry` | Task entry point function |
| `arg` | Argument passed to entry function |
| `priority` | Priority level (0 = highest, `CONFIG_NUM_PRIORITIES - 1` = lowest) |
| `stack` | Pointer to caller-provided stack buffer |
| `stack_size` | Size of stack buffer in bytes |

**Returns:** `BR_OK` on success, `BR_ERR_INVALID` if parameters are invalid, `BR_ERR_NOMEM` if TCB pool is full.

### `br_task_suspend`

```c
br_err_t br_task_suspend(br_tid_t tid);
```

Suspend a task. A suspended task is removed from the ready queue and will not be scheduled until resumed.

**Returns:** `BR_OK` on success, `BR_ERR_INVALID` if `tid` is out of range.

### `br_task_resume`

```c
br_err_t br_task_resume(br_tid_t tid);
```

Resume a previously suspended task.

**Returns:** `BR_OK` on success, `BR_ERR_INVALID` if `tid` is invalid or task is not suspended.

### `br_task_yield`

```c
void br_task_yield(void);
```

Voluntarily yield the CPU. The current task is moved to the back of its priority queue.

### `br_task_self`

```c
br_tid_t br_task_self(void);
```

Get the task ID of the currently running task.

## Time Services

### `br_sleep_us`

```c
void br_sleep_us(br_time_t us);
```

Block the current task for at least `us` microseconds. If `us` is 0, equivalent to `br_task_yield()`.

### `br_sleep_ms`

```c
static inline void br_sleep_ms(uint32_t ms);
```

Block the current task for at least `ms` milliseconds.

### `br_sleep_s`

```c
static inline void br_sleep_s(uint32_t s);
```

Block the current task for at least `s` seconds.

### `br_uptime_us`

```c
br_time_t br_uptime_us(void);
```

Get the system uptime in microseconds since boot.

## Semaphore

### `br_sem_init`

```c
br_err_t br_sem_init(br_sem_t *sem, int32_t initial, int32_t max);
```

Initialize a counting semaphore.

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `sem` | Pointer to semaphore object |
| `initial` | Initial count (must be >= 0 and <= `max`) |
| `max` | Maximum count (must be >= 1) |

**Returns:** `BR_OK` on success, `BR_ERR_INVALID` on bad parameters.

### `br_sem_take`

```c
br_err_t br_sem_take(br_sem_t *sem, br_time_t timeout);
```

Decrement the semaphore. If count is 0, block until available or timeout expires. Pass `0` for non-blocking try, `BR_TIME_INFINITE` to wait forever.

**Returns:** `BR_OK` on success, `BR_ERR_TIMEOUT` if would block and `timeout` is 0.

### `br_sem_give`

```c
br_err_t br_sem_give(br_sem_t *sem);
```

Increment the semaphore. If tasks are waiting, the highest-priority waiter is unblocked instead.

**Returns:** `BR_OK` on success, `BR_ERR_OVERFLOW` if count would exceed max.

## Mutex

Mutexes support priority inheritance — if a high-priority task blocks on a mutex held by a low-priority task, the owner's priority is temporarily raised.

### `br_mutex_init`

```c
br_err_t br_mutex_init(br_mutex_t *mtx);
```

Initialize a mutex. Must not be called from ISR.

### `br_mutex_lock`

```c
br_err_t br_mutex_lock(br_mutex_t *mtx, br_time_t timeout);
```

Lock the mutex. If already locked, block until available or timeout expires.

**Returns:** `BR_OK` on success, `BR_ERR_TIMEOUT` if non-blocking and locked, `BR_ERR_ISR` if called from ISR.

### `br_mutex_unlock`

```c
br_err_t br_mutex_unlock(br_mutex_t *mtx);
```

Unlock the mutex. Must be called by the owning task.

**Returns:** `BR_OK` on success, `BR_ERR_INVALID` if caller is not the owner, `BR_ERR_ISR` if called from ISR.

## Message Queue

Fixed-size ring buffer for inter-task communication. Buffer is caller-provided.

### `br_mqueue_init`

```c
br_err_t br_mqueue_init(br_mqueue_t *mq, void *buffer,
                        size_t msg_size, size_t max_msgs);
```

Initialize a message queue.

**Parameters:**

| Parameter | Description |
|-----------|-------------|
| `mq` | Pointer to message queue object |
| `buffer` | Pointer to caller-provided storage (`msg_size * max_msgs` bytes) |
| `msg_size` | Size of a single message in bytes |
| `max_msgs` | Maximum number of messages |

### `br_mqueue_send`

```c
br_err_t br_mqueue_send(br_mqueue_t *mq, const void *msg, br_time_t timeout);
```

Send a message. If the queue is full, block until space is available or timeout expires.

### `br_mqueue_recv`

```c
br_err_t br_mqueue_recv(br_mqueue_t *mq, void *msg, br_time_t timeout);
```

Receive a message. If the queue is empty, block until a message arrives or timeout expires.

## UART

Experimental. Declared in `include/bedrock/bedrock.h`.

### `br_uart_putc` / `br_uart_puts`

```c
void br_uart_putc(char c);
void br_uart_puts(const char *s);
```

Write a character or a string to the board console UART. Polled, blocks until the transmitter accepts the data. `br_uart_puts` sends `\r` before every `\n`.

### `br_uart_getc`

```c
br_err_t br_uart_getc(char *c, br_time_t timeout);
```

Read one received character. The UART receive interrupt puts incoming bytes into a ring buffer of `CONFIG_UART_RX_BUF_SIZE` bytes. If the buffer is empty, block until a byte arrives or `timeout` expires. Bytes that arrive while the buffer is full are dropped.

| Return | Meaning |
|--------|---------|
| `BR_OK` | `*c` holds the character |
| `BR_ERR_TIMEOUT` | No data within `timeout` |
| `BR_ERR_INVALID` | `c` is `NULL` |
| `BR_ERR_ISR` | Called from an ISR with a non-zero timeout |

## Shell

Declared in `lib/br_shell.h`. No dynamic allocation: commands are structures owned by the application.

```c
typedef int (*br_shell_handler_t)(int argc, char **argv);

typedef struct br_shell_cmd {
    const char          *name;
    const char          *help;
    br_shell_handler_t   handler;
    struct br_shell_cmd *next;
} br_shell_cmd_t;
```

### `br_shell_register`

```c
void br_shell_register(br_shell_cmd_t *cmd);
```

Add a command to the end of the command list. `cmd` must stay valid for the lifetime of the shell. Register each structure once. The built-in `help` command prints every command with its help text.

### `br_shell_readline`

```c
br_err_t br_shell_readline(char *buf, size_t size, br_time_t timeout);
```

Read a line from the UART with echo. Backspace and DEL erase the last character, CR, LF or CRLF end the line, other control characters are ignored. Characters past `size - 1` are dropped. Returns `BR_OK` when the line ends, or the `br_uart_getc` error (`BR_ERR_TIMEOUT`) if no character arrives within `timeout`. `buf` always ends with `\0`.

### `br_shell_execute`

```c
int br_shell_execute(char *line);
```

Split `line` in place on spaces and tabs (no quoting) and call the matching handler. Returns the handler result, `0` for an empty line, `-1` for an unknown command or more than `CONFIG_SHELL_MAX_ARGS` words.

### `br_shell_run`

```c
void br_shell_run(void) __attribute__((noreturn));
```

Print `> `, read a line of up to `CONFIG_SHELL_LINE_MAX - 1` characters and execute it, forever. Call it from a dedicated task.
