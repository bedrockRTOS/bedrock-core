<!--
  Project: bedrock[RTOS]
  Version: 0.0.1
  Author:  AnmiTaliDev <anmitalidev@nuros.org>
  License: CC BY-SA 4.0
-->

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
