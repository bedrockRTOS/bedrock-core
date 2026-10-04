# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/). Versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.1.0] - 2026-10-04

### Added
- `BR_STABLE`, `BR_EXPERIMENTAL` and `BR_INTERNAL` annotations in the public API. `BR_WARN_EXPERIMENTAL` and `BR_WARN_INTERNAL` turn the last two into compiler warnings.
- `include/bedrock/br_version.h` with `BR_VERSION_MAJOR`, `BR_VERSION_MINOR`, `BR_VERSION_PATCH` and `BR_VERSION`.
- Host HAL for x86-64 Linux in `arch/host-x86-64`. The kernel builds and runs as a Linux process with `chorus host`.
- Unit tests for tasks, scheduler, semaphore, mutex, message queue, sleep list and memory pool, for Cortex-M under QEMU and for the host.
- Kconfig support through kconfig-tools: `chorus build-tools`, `chorus defconfig`, `chorus menuconfig`.
- GitHub Actions workflow: Cortex-M build and host test run.
- Double-free check in `br_pool_free()`.

### Changed
- **Breaking:** `BEDROCK_VERSION_MAJOR`, `BEDROCK_VERSION_MINOR`, `BEDROCK_VERSION_PATCH` and `BEDROCK_VERSION` are renamed to `BR_VERSION_MAJOR`, `BR_VERSION_MINOR`, `BR_VERSION_PATCH` and `BR_VERSION`.
- kconfig-tools moved to a separate repository and is built with Zig.
- Source file headers are reduced to the SPDX identifier.

### Fixed
- Priority inheritance did not move a boosted mutex owner to its new ready queue, so the boost had no effect on scheduling.
- Tasks in a timed wait on a semaphore, mutex or queue shared one `next` field between the wait queue and the sleep list, which corrupted one of the lists.
- Timeouts on semaphores, mutexes and queues could fire late because the hardware alarm was not reprogrammed after a timed wait was added.
- `br_pool_free()` called twice on the same block made two later allocations return the same block.
- On the host HAL, a task switched in from the SIGALRM handler could see the ISR flag set and get `BR_ERR_ISR` from IPC calls.

[Unreleased]: https://github.com/bedrockRTOS/bedrock-core/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/bedrockRTOS/bedrock-core/compare/v0.0.3...v0.1.0
