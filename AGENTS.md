# AI Agent Guidelines for bedrock[RTOS]

This document provides instructions for AI agents (such as Claude Code, GitHub Copilot, Cursor AI) working on the bedrock[RTOS] codebase. It complements `CONTRIBUTING.md` with AI-specific context and constraints.

## Table of Contents

- [Project Overview](#project-overview)
- [Architecture Constraints](#architecture-constraints)
- [Code Modification Guidelines](#code-modification-guidelines)
- [Testing and Verification](#testing-and-verification)
- [Common Patterns](#common-patterns)
- [What NOT to Do](#what-not-to-do)

---

## Project Overview

bedrock[RTOS] is a **nanokernel** RTOS with strict architectural principles:

1. **Zero dynamic allocation** — No `malloc`, `calloc`, `realloc`, or `free` anywhere in `/kernel`, `/arch`, or `/lib`.
2. **Tickless design** — No periodic timer interrupts. One-shot alarms only.
3. **Clean separation** — Hardware abstraction is absolute. The kernel never touches hardware directly.

### Key Technologies

- **Language:** Pure C (C11/C17), no C++
- **Build system:** [Chorus](https://github.com/z3nnix/chorus)
- **Configuration:** Kconfig
- **Target architectures:** ARM Cortex-M (primary), RISC-V (planned)

---

## Architecture Constraints

### Hardware Abstraction Layer (HAL)

**All hardware access goes through `include/bedrock/br_hal.h`.**

If you need to:
- Read/write registers → Add a HAL function, implement it in `/arch/<arch>/`
- Access a timer → Use existing `br_hal_timer_*()` functions
- Enable/disable interrupts → Use `br_hal_irq_disable()` / `br_hal_irq_restore()`

**Never** add platform-specific code (`#ifdef __ARM__`, MMIO addresses, assembly) to `/kernel`.

### Memory Management

**Static allocation only.**

- Tasks come from a fixed-size `tcb_pool[CONFIG_MAX_TASKS]` array.
- IPC objects (semaphores, mutexes, queues) are allocated by the user in their own memory.
- If you need dynamic-like behavior, use a static pool allocator (see `lib/br_pool.c`).

**Never** call `malloc` in kernel code.

### Time Representation

Time is always `br_time_t` (64-bit microseconds):

```c
typedef uint64_t br_time_t;

#define BR_USEC(us)  ((br_time_t)(us))
#define BR_MSEC(ms)  ((br_time_t)(ms) * 1000U)
#define BR_SEC(s)    ((br_time_t)(s)  * 1000000U)
```

**Never** use `int`, `uint32_t`, or "ticks" for time. Always use `br_time_t` with explicit units.

---

## Code Modification Guidelines

### Adding a New Feature

1. **Check the roadmap** (`ROADMAP.md`) — is this feature planned? Does it fit the phase?
2. **Start with the public API** — define the function signature in `include/bedrock/bedrock.h`.
3. **Implement in the correct layer:**
   - Scheduler logic → `kernel/br_sched.c`
   - Task management → `kernel/br_task.c`
   - IPC primitives → `kernel/br_ipc.c`
   - Time/sleep → `kernel/br_time.c`
   - Hardware-specific → `arch/<arch>/br_hal_*.c`
4. **Add configuration if needed:**
   - Add `#ifndef CONFIG_FEATURE` to `include/bedrock/br_config.h`
   - Add `config FEATURE` to `Kconfig`
5. **Update documentation:**
   - User-facing changes → Add to `CHANGELOG.md` under `[Unreleased]`
   - API changes → Update `docs/en/api-reference.md`

### Modifying Existing Code

When modifying scheduler, IPC, or time logic:

1. **Read the entire file first** — understand the existing patterns.
2. **Preserve interrupt safety** — critical sections must be wrapped in:
   ```c
   uint32_t key = br_hal_irq_disable();
   // ... critical section ...
   br_hal_irq_restore(key);
   ```
3. **Maintain state consistency** — if a task state changes, ensure:
   - The task is removed from old queues (ready/wait/sleep)
   - The task is added to new queues
   - The `state` field is updated
4. **Respect the calling context:**
   - Functions callable from ISR must check `br_hal_in_isr()`
   - Mutexes **cannot** be used from ISR (return `BR_ERR_ISR`)

### Adding Configuration

**Every new configurable parameter must appear in both files:**

1. `include/bedrock/br_config.h`:
   ```c
   #ifndef CONFIG_NEW_FEATURE
   #define CONFIG_NEW_FEATURE 1  /* Default value */
   #endif
   ```

2. `Kconfig`:
   ```kconfig
   config NEW_FEATURE
       bool "Enable new feature"
       default y
       help
         Detailed description of what this feature does.
   ```

**Always provide:**
- A sensible default
- A help text explaining the feature
- Range constraints for integer values (`range 1 256`)

---

## Testing and Verification

### Current Testing Strategy (as of 0.0.3)

- **Manual testing:** Build and run on QEMU (`chorus && chorus run`)
- **Static analysis:** Review diffs for obvious errors
- **Behavioral testing:** Observe UART output for expected task scheduling

### When to Test

Before submitting a change:

1. **Build:** `chorus`
2. **Run on QEMU:** `chorus run` (if the target exists)
3. **Check output:** Verify tasks are scheduled correctly, no crashes
4. **Review code:** Ensure no `malloc`, no naked hardware access in `/kernel`

### Future Testing (0.1.0+)

Unit tests will be added with a host-native mock HAL. If your change is significant, prepare it for testability:
- Avoid global state where possible
- Keep functions small and focused
- Separate policy (what to do) from mechanism (how to do it)

---

## Common Patterns

### Implementing a New IPC Primitive

Example: Adding a barrier.

1. **Define the structure** in `include/bedrock/br_types.h`:
   ```c
   typedef struct {
       uint8_t    count;       /* Number of tasks to wait for */
       uint8_t    waiting;     /* Current number of waiting tasks */
       br_tcb_t  *wait_queue;
   } br_barrier_t;
   ```

2. **Declare the API** in `include/bedrock/bedrock.h`:
   ```c
   void br_barrier_init(br_barrier_t *barrier, uint8_t count);
   br_err_t br_barrier_wait(br_barrier_t *barrier, br_time_t timeout);
   ```

3. **Implement in** `kernel/br_ipc.c`:
   ```c
   void br_barrier_init(br_barrier_t *barrier, uint8_t count) {
       barrier->count = count;
       barrier->waiting = 0;
       barrier->wait_queue = NULL;
   }

   br_err_t br_barrier_wait(br_barrier_t *barrier, br_time_t timeout) {
       if (br_hal_in_isr()) {
           return BR_ERR_ISR;
       }

       uint32_t key = br_hal_irq_disable();

       barrier->waiting++;
       if (barrier->waiting < barrier->count) {
           // Block task...
       } else {
           // Wake all tasks...
           barrier->waiting = 0;
       }

       br_hal_irq_restore(key);
       return BR_OK;
   }
   ```

4. **Add documentation** to `docs/en/api-reference.md`.

### Adding a HAL Function

Example: Adding `br_hal_gpio_set()`.

1. **Declare in** `include/bedrock/br_hal.h`:
   ```c
   void br_hal_gpio_set(uint8_t port, uint8_t pin, bool value);
   ```

2. **Implement for Cortex-M** in `arch/arm-cortex-m/br_hal_gpio.c`:
   ```c
   void br_hal_gpio_set(uint8_t port, uint8_t pin, bool value) {
       // Write to MMIO registers here
   }
   ```

3. **Implement for RISC-V** in `arch/riscv32/br_hal_gpio.c` (when added):
   ```c
   void br_hal_gpio_set(uint8_t port, uint8_t pin, bool value) {
       // Write to RISC-V GPIO registers
   }
   ```

**Rule:** If you add a HAL function, it must be implemented for **all** supported architectures.

### Implementing Round-Robin Scheduling (Example)

This was implemented in commit `feat: implement round-robin time-slice scheduling`:

1. **Configuration:**
   - Added `CONFIG_RR_TIME_SLICE_US` to `br_config.h` and `Kconfig`

2. **Scheduler changes:**
   - Initialize `tcb->rr_remaining = CONFIG_RR_TIME_SLICE_US` when task becomes `RUNNING`
   - Added `br_sched_tick(br_time_t elapsed_us)` to decrement time slice
   - Preempt task when `rr_remaining` reaches zero and other tasks are ready

3. **Timer integration:**
   - Call `br_sched_tick()` from `SysTick_Handler()` in `arch/arm-cortex-m/br_hal_timer.c`

4. **Documentation:**
   - Updated `CHANGELOG.md`
   - Marked roadmap item as complete

**Key insight:** The time slice is stored in `br_tcb_t`, decremented by the timer, and checked by the scheduler — clean separation of concerns.

---

## What NOT to Do

### ❌ Do NOT add dynamic allocation

**Bad:**
```c
br_tcb_t *tcb = malloc(sizeof(br_tcb_t));
```

**Good:**
```c
static br_tcb_t tcb_pool[CONFIG_MAX_TASKS];
br_tcb_t *tcb = &tcb_pool[tcb_used++];
```

### ❌ Do NOT access hardware from kernel code

**Bad:**
```c
// In kernel/br_sched.c
#define SYSTICK_CTRL (*(volatile uint32_t *)0xE000E010)
SYSTICK_CTRL |= 0x01;
```

**Good:**
```c
// In kernel/br_sched.c
br_hal_timer_init();

// In arch/arm-cortex-m/br_hal_timer.c
void br_hal_timer_init(void) {
    SYST_CSR = 0x07;
}
```

### ❌ Do NOT use `#ifdef` for architecture detection in `/kernel`

**Bad:**
```c
#ifdef __ARM__
    __asm("svc 0");
#elif defined(__riscv)
    __asm("ecall");
#endif
```

**Good:**
```c
// In include/bedrock/br_hal.h
void br_hal_syscall(void);

// In arch/arm-cortex-m/br_hal_syscall.c
void br_hal_syscall(void) {
    __asm("svc 0");
}

// In arch/riscv32/br_hal_syscall.c
void br_hal_syscall(void) {
    __asm("ecall");
}
```

### ❌ Do NOT use "ticks" or ambiguous time units

**Bad:**
```c
void br_sleep(uint32_t ticks);  // What is a tick? 1ms? 10ms? Unknown.
```

**Good:**
```c
void br_sleep_us(br_time_t us);  // Unambiguous: microseconds
```

### ❌ Do NOT break the API without BREAKING CHANGE

If you change a public function signature:

**Bad:**
```
feat(task): change br_task_create to use config struct
```

**Good:**
```
feat(task)!: change br_task_create to use config struct

BREAKING CHANGE: br_task_create now takes a br_task_config_t pointer
instead of individual parameters.
```

### ❌ Do NOT create documentation unless requested

Agents should **not** proactively generate:
- Tutorial files
- Example documentation
- README rewrites

Only create documentation when:
- The user explicitly asks for it
- It is part of a feature specification (e.g., "add RR scheduling and document it")

### ❌ Do NOT add features outside the roadmap

Respect the phased roadmap in `ROADMAP.md`. If a feature is not listed:
1. Suggest adding it to the roadmap first
2. Ask the user if it should be prioritized
3. Only implement after confirmation

---

## Commit Message Examples

### Good Commits

```
feat(sched): implement round-robin time-slice scheduling

- Add CONFIG_RR_TIME_SLICE_US (default 10ms)
- Initialize rr_remaining when task becomes running
- Decrement time slice in SysTick handler
- Preempt tasks when quantum expires if others are ready
```

```
fix(mutex): restore original priority on nested unlock

When a task acquired multiple mutexes, unlocking the outer
mutex did not restore the task's original priority. This
fix ensures the priority stack is unwound correctly.
```

```
refactor(hal): extract common IRQ disable/restore pattern

Created br_critical_section_t helper to reduce boilerplate
in IPC code.
```

```
docs(api): add br_barrier_wait API reference

Closes #42
```

### Bad Commits

```
update stuff
```
```
fixed the bug
```
```
feat: add shell + update docs + fix timer
```

---

## Summary

When working on bedrock[RTOS] as an AI agent:

1. **Understand the constraints:** No malloc, no naked hardware access, no platform `#ifdef` in `/kernel`.
2. **Follow the HAL:** All hardware goes through `br_hal.h`.
3. **Use `br_time_t`:** Always explicit microsecond units.
4. **Respect the roadmap:** Implement features in the planned order.
5. **Test on QEMU:** Build and run before submitting.
6. **Write clear commits:** Follow Conventional Commits format.
7. **Update `CHANGELOG.md`:** For every user-facing change.

When in doubt:
- Read the existing code first
- Ask the user if the approach makes sense
- Prefer simple, explicit code over clever abstractions
- Remember: this is a **nanokernel** — every byte counts.

---

**For questions or clarifications, refer to:**
- `CONTRIBUTING.md` — for general contributor guidelines
- `ROADMAP.md` — for planned features and priorities
- `docs/en/architecture.md` — for design philosophy and architecture details
