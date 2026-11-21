# Optional Ports: littlefs and FatFs

`ports/` holds optional ports of third-party libraries. The kernel does not need them, nothing in `libbedrock_*.a` links them, and an application takes only what it uses. Each port is a directory with the unmodified upstream code in `upstream/` and the glue to bedrock[RTOS] next to it.

| Port | Upstream | License | Glue |
|------|----------|---------|------|
| `ports/littlefs/` | [littlefs](https://github.com/littlefs-project/littlefs) v2.11.3, a git submodule in `upstream/` | BSD-3-Clause, `upstream/LICENSE.md` | `br_lfs.c`, `br_lfs.h`, `br_lfs_config.h` |
| `ports/lwip/` | [lwIP](https://github.com/lwip-tcpip/lwip) 2.2.1, a git submodule in `upstream/` | BSD-3-Clause, `upstream/COPYING` | `br_net.c`, `br_net_sock.c`, `br_shell_net.c`, `sys_arch.c`, `lwipopts.h`, `arch/` |
| `ports/fatfs/` | [FatFs](https://elm-chan.org/fsw/ff/) R0.16 by ChaN, files copied unchanged into `upstream/` | FatFs license (BSD-style), `upstream/LICENSE.txt` | `br_ff.c`, `br_ff.h`, `ffconf.h` |

`ports/br_libc.c` supplies `memcmp`, `strchr`, `strcpy`, `strspn` and `strcspn` for these libraries on targets without a C library. Do not link it on the host.

Both ports sit on `br_blkdev_t` (see [filesystem](filesystem.md)), so they work with the RAM block device and with any flash driver that implements it. Neither allocates memory.

Fetch littlefs and lwIP with `git submodule update --init ports/littlefs/upstream ports/lwip/upstream`.

lwIP is described in the [networking guide](networking.md).

## littlefs

Add `ports/littlefs/upstream/lfs.c`, `lfs_util.c`, `ports/littlefs/br_lfs.c` and `ports/br_libc.c` to the application. Compile `lfs.c` and `lfs_util.c` with `-DLFS_DEFINES=br_lfs_config.h`, and use `-Iports/littlefs -Iports/littlefs/upstream` for everything that includes `br_lfs.h`. `br_lfs.h` defines `LFS_DEFINES` itself if the application did not.

`br_lfs_config.h` turns on `LFS_NO_MALLOC`, `LFS_THREADSAFE` and switches off asserts and log output. With `LFS_THREADSAFE` littlefs takes a bedrock mutex around every call, so several tasks can use one filesystem.

```c
br_err_t br_lfs_init(br_lfs_t *l, br_blkdev_t *dev);
int      br_lfs_file_open(br_lfs_t *l, br_lfs_file_t *f, const char *path, int flags);
```

`br_lfs_init` fills `l->cfg` for the block device. Then use the normal littlefs API on `l->lfs` and `l->cfg`:

```c
static br_lfs_t      fs;
static br_lfs_file_t file;

br_lfs_init(&fs, &dev);
lfs_format(&fs.lfs, &fs.cfg);
lfs_mount(&fs.lfs, &fs.cfg);

br_lfs_file_open(&fs, &file, "log", LFS_O_WRONLY | LFS_O_CREAT);
lfs_file_write(&fs.lfs, &file.file, data, len);
lfs_file_close(&fs.lfs, &file.file);
```

Open files with `br_lfs_file_open` and not `lfs_file_open`. Without malloc littlefs needs a buffer for every open file, and `br_lfs_file_t` carries it.

Buffers are sized at build time: `BR_LFS_CACHE_SIZE` (default 64) and `BR_LFS_LOOKAHEAD_SIZE` (default 16). `br_lfs_init` returns `BR_ERR_INVALID` if the cache size does not divide the block size or is not a multiple of the device `prog_size`. `block_cycles` is 100.

## FatFs

Add `ports/fatfs/upstream/ff.c`, `ports/fatfs/br_ff.c` and `ports/br_libc.c` to the application, with `-Iports/fatfs -Iports/fatfs/upstream`. The `ffconf.h` in `ports/fatfs/` is the stock R0.16 file with these changes:

| Option | Value | Why |
|--------|-------|-----|
| `FF_USE_MKFS` | 1 | `f_mkfs` is available |
| `FF_CODE_PAGE` | 437 | Single-byte code page, no table of 932 |
| `FF_FS_TINY` | 1 | A file object has no sector buffer, which saves 512 bytes each |
| `FF_FS_NORTC` | 1 | No `get_fattime` is required, files get a fixed time stamp |
| `FF_FS_REENTRANT` | 1 | Volume access is guarded by bedrock mutexes (`ff_mutex_*` in `br_ff.c`) |

Everything else is the default: one volume, one partition, 512-byte sectors, 8.3 file names (`FF_USE_LFN` is 0), no exFAT.

```c
br_err_t br_ff_attach(br_blkdev_t *dev);
void     br_ff_detach(void);
```

`br_ff_attach` selects the block device for volume 0. Its `block_size` must be 512. The rest is the standard FatFs API (`f_mount`, `f_mkfs`, `f_open`, ...). `disk_write` erases a block and programs it whole, so the device has to be one where a block is the erase unit and a 512-byte sector, such as a RAM disk. A real flash device with larger erase blocks needs a translation layer below `br_blkdev_t`.

`f_mkfs` needs at least 128 sectors, which is 64 KiB. For that reason `test_fatfs` runs on the host, RISC-V and RP2040 only, and not on LM3S6965 (64 KiB of RAM) and STM32F103 (20 KiB).

## Tests

`test_littlefs` runs on every target. It covers format, mount, files, directories, rename, remove, a remount and power loss at every second program of a rewrite. `test_fatfs` runs on the host, RISC-V and RP2040. It covers format, mount, files, directories, rename, remove, free space and a remount.

## Limits

- Neither port has been tried on real flash or an SD card. Both were run on the RAM block device only.
- The FatFs port has no `get_fattime`, so time stamps are fixed. Define `FF_FS_NORTC 0` and provide `get_fattime` to change that.
