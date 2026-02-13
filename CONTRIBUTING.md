# Contributing to bedrock[RTOS]

Thank you for your interest in contributing to bedrock[RTOS]. This document describes the rules, conventions, and workflow that every contributor must follow.

## Table of Contents

- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Contribution Workflow](#contribution-workflow)
- [Commit Message Format](#commit-message-format)
- [Updating the Changelog](#updating-the-changelog)
- [Coding Style](#coding-style)
- [License Headers](#license-headers)
- [Links](#links)

---

## Project Structure

| Directory | Purpose | License |
|-----------|---------|---------|
| `/kernel` | Scheduler, tasks, time, IPC | BSD 3-Clause |
| `/arch` | HAL implementations per architecture | BSD 3-Clause |
| `/boards` | Board-specific configs and linker scripts | BSD 3-Clause |
| `/include/bedrock` | Public API headers | BSD 3-Clause |
| `/lib` | Static pool allocator, utilities | LGPL 2.1 |
| `/examples` | Application templates and demos | GPL 3.0 |
| `/docs` | Documentation | CC BY-SA 4.0 |
| `/3rd` | Third-party tools and libraries | Per-project |

---

## Getting Started

1. Fork the repository on GitHub.
2. Clone your fork locally.
3. Install the prerequisites:
   - `arm-none-eabi-gcc` toolchain
   - `qemu-system-arm` (for Cortex-M3 emulation)
   - [Chorus](https://github.com/z3nnix/chorus) build system (included in `3rd/tools/chorus`)
4. Build the project: `chorus`
5. Run on QEMU: `chorus run`

---

## Contribution Workflow

1. **Fork** the repository.
2. **Create a branch** from `master` with a descriptive name:
   - `feat/short-description` — for new features
   - `fix/short-description` — for bug fixes
   - `docs/short-description` — for documentation changes
   - `refactor/short-description` — for refactoring
   - `test/short-description` — for test additions
3. **Make your changes.** Keep each commit focused on a single logical change.
4. **Update `CHANGELOG.md`** if the change is user-facing (see [below](#updating-the-changelog)).
5. **Push** your branch to your fork.
6. **Open a Pull Request** against `master`.

### Pull Request Rules

- **One PR = one logical task.** Do not combine unrelated changes.
- **CI must pass.** A PR with failing CI will not be reviewed.
- **Keep it small.** Smaller PRs are reviewed faster and merged sooner.
- **Describe what and why.** The PR description should explain the motivation, not just list changed files.

---

## Commit Message Format

This project follows [Conventional Commits](https://www.conventionalcommits.org/).

### Format

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

### Types

| Type | When to use |
|------|-------------|
| `feat` | A new feature |
| `fix` | A bug fix |
| `docs` | Documentation only changes |
| `refactor` | Code change that neither fixes a bug nor adds a feature |
| `test` | Adding or correcting tests |
| `chore` | Build system, CI, dependencies, scripts |
| `perf` | Performance improvement |
| `style` | Formatting, whitespace, semicolons (not CSS) |
| `ci` | CI pipeline changes |

### Scope (Optional)

The scope indicates the area of the codebase affected:

```
feat(sched): add round-robin time-slice support
fix(ipc): handle semaphore overflow in ISR context
docs(porting): update RISC-V porting guide
chore(chorus): add QEMU run target
```

### Good Examples

```
feat(sched): add round-robin time-slice support
fix(mutex): restore original priority on nested unlock
docs: add project roadmap (0.0.1 → 1.0.0)
refactor(hal): extract common IRQ disable/restore pattern
test(pool): add exhaustion and double-free test cases
perf(sched): use bitmap for O(1) priority lookup
chore: update chorus to v0.3.0
```

### Bad Examples

```
# Too vague
fix: fixed stuff
update code

# Not conventional format
Fixed the bug in scheduler
FEAT - added new task API

# Multiple unrelated changes in one commit
feat: add shell + fix timer + update docs
```

### Breaking Changes

For changes that break the public API, use `!` after the type or add a `BREAKING CHANGE:` footer:

```
feat(task)!: change br_task_create signature to accept config struct

BREAKING CHANGE: br_task_create now takes a br_task_config_t pointer
instead of individual parameters.
```

---

## Updating the Changelog

Every user-facing change **must** be recorded in `CHANGELOG.md` under the `[Unreleased]` section. We follow the [Keep a Changelog](https://keepachangelog.com/) format.

### Categories

- **Added** — new features
- **Changed** — changes to existing functionality
- **Deprecated** — features that will be removed in a future version
- **Removed** — features that have been removed
- **Fixed** — bug fixes
- **Security** — vulnerability fixes

### Example

```markdown
## [Unreleased]

### Added
- Round-robin scheduling for equal-priority tasks.
- `br_task_delete()` to reclaim TCB slots.

### Fixed
- Mutex priority inheritance not restoring original priority on nested unlock.
```

---

## Coding Style

### Language

- **C11/C17 only.** No C++ in any kernel, HAL, or library code.
- Compile with `-std=c11` (or `-std=c17`).

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Public functions | `br_` prefix, snake_case | `br_task_create()` |
| Public types | `br_` prefix, snake_case, `_t` suffix | `br_tcb_t` |
| Public macros/constants | `BR_` prefix, UPPER_SNAKE_CASE | `BR_TIME_INFINITE` |
| Config macros | `CONFIG_` prefix, UPPER_SNAKE_CASE | `CONFIG_MAX_TASKS` |
| Static functions | snake_case, no prefix | `pick_next()` |
| Local variables | snake_case | `stack_top` |

### Formatting

- **Indentation:** 4 spaces. No tabs.
- **Max line length:** 100 characters.
- **Braces:** K&R style — opening brace on the same line.
- **Single blank line** between function definitions.
- **No trailing whitespace.**

```c
br_err_t br_sem_take(br_sem_t *sem, br_time_t timeout) {
    if (sem == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();
    /* ... */
    br_hal_irq_restore(key);
    return BR_OK;
}
```

### Rules

- **No dynamic allocation in `/kernel`.** Never use `malloc`, `calloc`, `realloc`, or `free` in kernel code.
- **No platform-specific code in `/kernel`.** All hardware access goes through the HAL interface in `include/bedrock/br_hal.h`.
- **Minimize includes.** Only include what you use.
- **Use `const` and `static` where appropriate.**
- **Every `switch` must have a `default` case.**

---

## License Headers

Every new source file **must** begin with the correct license header matching its directory. Do not include the full license text — reference the SPDX identifier and a brief notice.

### BSD 3-Clause (for `/kernel`, `/arch`, `/boards`, `/include`)

```c
/*
 * Project: bedrock[RTOS]
 * Author:  Your Name <your@email.com>
 * License: BSD 3-Clause
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.
 */
```

### LGPL 2.1 (for `/lib`)

```c
/*
 * Project: bedrock[RTOS]
 * Author:  Your Name <your@email.com>
 * License: LGPL-2.1
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 */
```

### GPL 3.0 (for `/examples`)

```c
/*
 * Project: bedrock[RTOS]
 * Author:  Your Name <your@email.com>
 * License: GPL-3.0
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
```

### Documentation (for `/docs`)

```markdown
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->
```

---

## Links

- **Repository:** [github.com/bedrockRTOS/bedrock-core](https://github.com/bedrockRTOS/bedrock-core)
- **Issues:** [github.com/bedrockRTOS/bedrock-core/issues](https://github.com/bedrockRTOS/bedrock-core/issues)
- **Roadmap:** [ROADMAP.md](ROADMAP.md)
