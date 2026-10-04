# Contributing

## Setup

1. Fork the repository and clone your fork.
2. Install the dependencies from [README.md](README.md#dependencies).
3. Build: `chorus`.
4. Build and run the host tests: `chorus host`, then the `test_*_host` binaries.
5. Run on QEMU: `chorus run`.

## Workflow

1. Create a branch from `master`:
   - `feat/<name>`: new feature
   - `fix/<name>`: bug fix
   - `docs/<name>`: documentation
   - `refactor/<name>`: refactoring
   - `test/<name>`: tests
2. Make the change. One commit is one logical change.
3. For user-facing changes, add an entry to `CHANGELOG.md` (see below).
4. Push the branch and open a pull request against `master`.

### Pull requests

- One pull request is one task. Do not mix unrelated changes.
- CI must pass. CI builds for Cortex-M and runs the host tests.
- Smaller pull requests get reviewed faster.
- The description says what changed and why.

## Commit messages

Commits follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>[(scope)]: <description>

[body]

[footer]
```

| Type | Use for |
|------|---------|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `refactor` | Code change with no new behavior and no fix |
| `test` | Tests |
| `perf` | Performance |
| `style` | Formatting only |
| `build` | Build system, dependencies |
| `ci` | CI configuration |
| `chore` | Everything else |

The scope is the affected area: `sched`, `ipc`, `task`, `time`, `hal`, `pool`, `chorus` and so on.

```
feat(sched): add round-robin time slicing
fix(mutex): restore original priority on nested unlock
test(pool): add exhaustion and double-free cases
```

Breaking changes to the public API need `!` after the type and a `BREAKING CHANGE:` footer:

```
feat(task)!: take a config struct in br_task_create

BREAKING CHANGE: br_task_create now takes a br_task_config_t pointer
instead of separate arguments.
```

## Changelog

`CHANGELOG.md` uses the [Keep a Changelog](https://keepachangelog.com/) format. New entries go under `[Unreleased]`, in one of the sections: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`.

```markdown
## [Unreleased]

### Added
- `br_task_delete()` to free TCB slots.

### Fixed
- Mutex priority inheritance did not restore the original priority on nested unlock.
```

## Code

### Language

C11, built with `-std=c11 -Wall -Wextra -Werror`. No C++.

### Naming

| Element | Rule | Example |
|---------|------|---------|
| Public functions | `br_` prefix, snake_case | `br_task_create()` |
| Public types | `br_` prefix, snake_case, `_t` suffix | `br_tcb_t` |
| Public macros | `BR_` prefix, UPPER_SNAKE_CASE | `BR_TIME_INFINITE` |
| Config macros | `CONFIG_` prefix, UPPER_SNAKE_CASE | `CONFIG_MAX_TASKS` |
| Static functions | snake_case, no prefix | `pick_next()` |
| Local variables | snake_case | `stack_top` |

### Formatting

- 4 spaces, no tabs.
- Lines up to 100 characters.
- The opening brace of a function goes on its own line. For `if`, `for`, `while` and `switch` it stays on the same line.
- One blank line between functions.
- No trailing whitespace.

```c
br_err_t br_sem_take(br_sem_t *sem, br_time_t timeout)
{
    if (sem == NULL) {
        return BR_ERR_INVALID;
    }

    uint32_t key = br_hal_irq_disable();
    br_hal_irq_restore(key);
    return BR_OK;
}
```

### Rules

- No `malloc`, `calloc`, `realloc` or `free` in `/kernel`, `/arch` or `/lib`. Objects live in static pools or in user memory.
- No hardware access and no architecture `#ifdef` in `/kernel`. Hardware is reached only through `include/bedrock/br_hal.h`.
- A new HAL function must be implemented for every architecture in `/arch`, including `arch/host-x86-64`.
- Time is `br_time_t` in microseconds. No ticks.
- Critical sections go between `br_hal_irq_disable()` and `br_hal_irq_restore()`.
- Functions that can be called from an ISR check `br_hal_in_isr()`. Mutexes return `BR_ERR_ISR` in an ISR.
- A new config option goes into both `include/bedrock/br_config.h` and `Kconfig`, with a default value and help text.
- Every `switch` has a `default`.
- Include only what you use.

## Tests

Tests are in `examples/test_*.c`. Each test prints `PASS` or `FAIL` per case and `=== All Tests Complete ===` at the end. A new test needs targets in `chorus.build` for both Cortex-M and host, and an entry in the test list in `.github/workflows/ci.yml`.

## License headers

Each source file in `/kernel`, `/arch`, `/boards`, `/include`, `/lib` and `/examples` starts with:

```c
/*
 * SPDX-License-Identifier: GPL-3.0-only
 */
```

This code is under GPL-3.0 with the bedrock[RTOS] runtime exception, see [LICENSE.md](LICENSE.md). `/docs` is under CC BY-SA 4.0.

## Links

- Issues: <https://github.com/bedrockRTOS/bedrock-core/issues>
- [Roadmap](ROADMAP.md)
- [Documentation](docs/en/)
