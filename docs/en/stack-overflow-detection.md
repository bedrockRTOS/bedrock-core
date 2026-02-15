# Stack Overflow Detection

## Overview

Bedrock RTOS implements stack overflow detection using a canary-based mechanism. A magic value (canary) is placed at the bottom of each task's stack and checked during context switches.

## Implementation Details

### Canary Value

The canary is defined as:
```c
#define BR_STACK_CANARY   0xDEADBEEF
```

### How It Works

1. **Initialization**: When a task is created (`br_task_create`), a canary value is written to the bottom (lowest address) of the task's stack.

2. **Storage**: Each TCB (`br_tcb_t`) contains a pointer to the canary location:
   ```c
   uint32_t *stack_canary;   /* Pointer to canary at stack bottom */
   ```

3. **Detection**: Before every context switch (`br_sched_reschedule`), the scheduler checks if the canary value has been corrupted using `br_hal_check_stack_overflow()`.

4. **Response**: If corruption is detected, the system triggers a kernel panic and halts execution to prevent further damage.

### Error Code

A new error code has been added:
```c
BR_ERR_STACK_OVF = -7  /* Stack overflow detected */
```

## HAL Interface

### `br_hal_check_stack_overflow(br_tcb_t *tcb)`

Checks the stack canary for the given task. If the canary value has been corrupted (indicating stack overflow), the function triggers a kernel panic.

**Parameters:**
- `tcb`: Pointer to the Task Control Block to check

**Behavior:**
- Compares `*tcb->stack_canary` with `BR_STACK_CANARY`
- If mismatch detected, calls `br_kernel_panic()` and halts the system

## Limitations

- **Detection Timing**: Stack overflow is only detected during context switches. If a task overflows its stack and never yields control, the overflow won't be detected until the next switch attempt.

- **Single Canary**: Only one canary value at the stack bottom. This detects complete overflow but may not catch partial corruption.

- **No Recovery**: When overflow is detected, the system halts. This is by design to prevent undefined behavior.

## Future Enhancements

Potential improvements:
- Multiple canary values throughout the stack
- Periodic stack checking independent of context switches
- Stack usage watermarking for debugging
- Configurable overflow response (panic vs. task termination)

## Testing

To test stack overflow detection, create a task with a small stack and trigger recursive calls or large local arrays:

```c
static uint8_t test_stack[128];  /* Very small stack */

void overflow_task(void *arg) {
    volatile uint8_t large_buffer[256];  /* Larger than stack! */
    /* This will overflow and be detected on next context switch */
}
```

## References

- `include/bedrock/br_types.h`: Canary constant and TCB structure
- `kernel/br_task.c`: Canary initialization
- `kernel/br_sched.c`: Canary checking during context switch
- `arch/arm-cortex-m/br_hal_context.c`: Platform-specific implementation
