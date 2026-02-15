# Task Deletion and TCB Pool Management

## Overview

Bedrock RTOS now supports task deletion via `br_task_delete()`, which allows tasks to be removed from the system and their TCB (Task Control Block) slots to be returned to the pool for reuse.

## TCB Pool Architecture

### Previous Design (Pre-deletion)

Originally, TCB slots were allocated sequentially using a simple counter (`tcb_used`):
- Tasks were created in slots 0, 1, 2, ... up to `CONFIG_MAX_TASKS`
- Once allocated, slots were never freed
- No TCB reuse possible

### Current Design (With deletion support)

The TCB pool now uses state-based allocation:
- Each TCB has a state field (`BR_TASK_INACTIVE`, `BR_TASK_READY`, etc.)
- `br_task_create()` scans for the first `BR_TASK_INACTIVE` slot
- `br_task_delete()` marks the slot as `BR_TASK_INACTIVE`, returning it to the pool
- Slots can be reused indefinitely

## API: `br_task_delete()`

### Function Signature

```c
br_err_t br_task_delete(br_tid_t tid);
```

### Parameters

- `tid`: Task ID of the task to delete

### Return Values

- `BR_OK`: Task successfully deleted
- `BR_ERR_INVALID`: Invalid conditions:
  - Task ID out of range (`tid >= CONFIG_MAX_TASKS`)
  - Task is already inactive
  - Attempting to delete the currently running task

### Behavior

1. **Validation**: Checks if the task ID is valid and the task exists
2. **Self-deletion prevention**: Cannot delete the currently running task
3. **Queue removal**: Removes the task from ready queue if present
4. **Cleanup**: Clears all TCB fields
5. **Slot return**: Marks the slot as `BR_TASK_INACTIVE` for reuse

### Important Constraints

**Cannot delete the running task**: A task cannot delete itself. To implement task self-termination:

```c
/* From another task */
void supervisor_task(void *arg) {
    br_tid_t worker_tid = get_worker_tid();
    
    /* Signal worker to finish */
    signal_worker_to_stop();
    
    /* Wait for it to yield or block */
    br_sleep_ms(10);
    
    /* Now safe to delete */
    br_task_delete(worker_tid);
}
```

## Usage Examples

### Basic Task Deletion

```c
br_tid_t worker_tid;
static uint8_t worker_stack[512];

void worker_task(void *arg) {
    /* Do work */
    br_sleep_ms(100);
    /* Task completes, but stays in system until deleted */
}

void main_task(void *arg) {
    /* Create worker */
    br_task_create(&worker_tid, "worker", worker_task, NULL,
                   5, worker_stack, sizeof(worker_stack));
    
    /* Wait for worker to complete */
    br_sleep_ms(200);
    
    /* Delete worker task - slot can be reused */
    br_task_delete(worker_tid);
    
    /* Can create new task in the same slot */
    br_task_create(&worker_tid, "new_worker", another_task, NULL,
                   5, worker_stack, sizeof(worker_stack));
}
```

### Deleting Suspended Tasks

```c
/* Suspend a task */
br_task_suspend(worker_tid);

/* Delete while suspended */
br_task_delete(worker_tid);  /* OK - task is not running */
```

### Deleting Blocked Tasks

```c
/* Task is blocked on semaphore/mutex/sleep */
br_task_delete(blocked_tid);  /* OK - task is not running */
```

## Implementation Details

### Changes to TCB Allocation

**`br_task_create()` now searches for free slots:**

```c
/* Find first free TCB slot */
br_tcb_t *tcb = NULL;
for (int i = 0; i < CONFIG_MAX_TASKS; i++) {
    if (tcb_pool[i].state == BR_TASK_INACTIVE) {
        tcb = &tcb_pool[i];
        break;
    }
}
```

### Task Cleanup Process

When a task is deleted:

```c
/* Remove from ready queue if present */
if (tcb->state == BR_TASK_READY) {
    br_sched_unready(tcb);
}

/* Clear all fields */
tcb->state = BR_TASK_INACTIVE;
tcb->name = NULL;
tcb->entry = NULL;
tcb->arg = NULL;
tcb->stack_base = NULL;
tcb->stack_size = 0;
tcb->stack_canary = NULL;
tcb->sp = NULL;
tcb->next = NULL;
```

## Best Practices

### Memory Management

- **Stack lifetime**: The caller must ensure the stack memory remains valid while the task exists
- **Stack reuse**: After deletion, the same stack buffer can be reused for new tasks
- **No automatic cleanup**: Deleting a task does NOT free its stack - that's caller responsibility

### Safe Deletion Patterns

**Pattern 1: Supervisor-managed workers**
```c
void supervisor(void *arg) {
    while (1) {
        br_tid_t worker;
        create_worker(&worker);
        wait_for_worker_completion(worker);
        br_task_delete(worker);  /* Clean up when done */
    }
}
```

**Pattern 2: Task pool**
```c
#define POOL_SIZE 4
br_tid_t task_pool[POOL_SIZE];
uint8_t task_stacks[POOL_SIZE][TASK_STACK_SIZE];

void spawn_task(int slot, br_task_entry_t entry) {
    /* Delete old task if exists */
    if (task_pool[slot] != 0) {
        br_task_delete(task_pool[slot]);
    }
    
    /* Create new task in slot */
    br_task_create(&task_pool[slot], "pooled", entry, NULL,
                   5, task_stacks[slot], TASK_STACK_SIZE);
}
```

## Limitations

1. **No self-deletion**: Tasks cannot delete themselves
2. **No automatic stack cleanup**: Caller manages stack memory
3. **No dependency tracking**: Deleting a task holding a mutex/resource requires manual cleanup
4. **Idle task protection**: Consider adding protection against deleting critical system tasks

## Future Enhancements

Possible improvements:
- Add `br_task_exit()` for cooperative self-termination
- Resource cleanup hooks (automatically release held mutexes/semaphores)
- Task reference counting to prevent premature deletion
- Zombie state for tasks that have finished but not yet been deleted

## Related APIs

- `br_task_create()`: Create new tasks
- `br_task_suspend()`: Temporarily pause a task
- `br_task_resume()`: Resume a suspended task
- `br_task_yield()`: Voluntarily give up CPU

## References

- `include/bedrock/bedrock.h`: Public API declaration
- `kernel/br_task.c`: Implementation
- `include/bedrock/br_types.h`: TCB structure definition
