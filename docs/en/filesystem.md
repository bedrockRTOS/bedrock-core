# Filesystem

bedrock[RTOS] has a small flash-oriented filesystem. It has no dependencies and no dynamic allocation. Sources: `lib/br_fs.c`, `lib/br_fs.h`.

## Block device

A filesystem sits on a `br_blkdev_t` (`include/bedrock/br_blkdev.h`), a flat array of equal blocks with flash semantics:

```c
typedef struct br_blkdev {
    const br_blkdev_ops_t *ops;
    void                  *priv;
    uint32_t               block_size;
    uint32_t               block_count;
    uint32_t               prog_size;
} br_blkdev_t;
```

| Operation | Contract |
|-----------|----------|
| `read(dev, block, off, buf, len)` | Read any range inside a block. |
| `write(dev, block, off, buf, len)` | Program. `off` and `len` are multiples of `prog_size`. Programming can only clear bits, so a range must be erased before it is written, and a `prog_size` unit must be written once per erase. |
| `erase(dev, block)` | Set every bit of the block. |
| `sync(dev)` | Make earlier writes durable. |

The `br_blkdev_read`, `br_blkdev_write`, `br_blkdev_erase` and `br_blkdev_sync` wrappers check the range and alignment and return `BR_ERR_INVALID` on a bad argument. A flash driver fills in the ops table.

`lib/br_ramdisk.c` is a block device in RAM that behaves like NOR flash: it starts erased, `write` only clears bits and `erase` sets them. `br_ramdisk_t::writes_until_fail` makes the device return `BR_ERR_IO` after that many programs, to test power loss. `-1` disables it.

```c
br_err_t br_ramdisk_init(br_ramdisk_t *rd, void *mem, uint32_t block_size,
                         uint32_t block_count);
```

## API

Declared in `lib/br_fs.h`. One filesystem is mounted at a time. All calls are safe from several tasks and are not allowed from an ISR.

```c
br_err_t br_fs_format(br_blkdev_t *dev);
br_err_t br_fs_mount(br_blkdev_t *dev);
br_err_t br_fs_unmount(void);

int      br_fs_open(const char *name, uint8_t flags);
int      br_fs_read(int fd, void *buf, size_t len);
br_err_t br_fs_seek(int fd, size_t offset);
int      br_fs_write(int fd, const void *buf, size_t len);
br_err_t br_fs_close(int fd);

br_err_t br_fs_remove(const char *name);
br_err_t br_fs_stat(const char *name, size_t *size);
br_err_t br_fs_list(uint32_t index, br_fs_info_t *info);
br_err_t br_fs_usage(br_fs_usage_t *usage);

br_err_t br_fs_mkdir(const char *path);
br_err_t br_fs_rmdir(const char *path);
br_err_t br_fs_info(const char *path, br_fs_info_t *info);
br_err_t br_fs_readdir(const char *dir, uint32_t index, br_fs_info_t *info);
```

| Function | Behavior |
|----------|----------|
| `br_fs_format` | Erase every block and write the superblock. `BR_ERR_BUSY` while a filesystem is mounted. |
| `br_fs_mount` | Check the superblock (`BR_ERR_IO` if there is none), find the newest version of each file and build the free map. `BR_ERR_BUSY` if already mounted, `BR_ERR_INVALID` if the device does not fit the limits below. |
| `br_fs_unmount` | `BR_ERR_BUSY` while a file is open. |
| `br_fs_open` | `flags` is `BR_FS_READ`, `BR_FS_WRITE`, or `BR_FS_WRITE \| BR_FS_APPEND`. Returns a descriptor or a negative `br_err_t`. Creating a file needs its parent directory, `BR_ERR_NOENT` otherwise. |
| `br_fs_read` | Returns the bytes read, `0` at the end of the file. |
| `br_fs_seek` | Move a read descriptor to an absolute offset from `0` to the file size. A write descriptor cannot seek, `BR_ERR_INVALID`. Going back to the start of a file and then forward walks the block chain, so it takes time proportional to the offset. |
| `br_fs_write` | Sequential. Returns the bytes accepted. A short count or an error means the file is damaged and `br_fs_close` will reject it. |
| `br_fs_close` | For a write descriptor this commits the file. |
| `br_fs_remove` | `BR_ERR_NOENT` if the file does not exist, `BR_ERR_BUSY` if it is open. |
| `br_fs_stat` | File size. |
| `br_fs_list` | The `index`-th entry of the whole filesystem, files and directories with their full paths, `BR_ERR_NOENT` past the last one. In block order, not in name order. |
| `br_fs_mkdir` | Create a directory. `BR_ERR_NOENT` if the parent does not exist, `BR_ERR_BUSY` if a file or a directory has the name. |
| `br_fs_rmdir` | Remove an empty directory. `BR_ERR_BUSY` if it has entries or is open. |
| `br_fs_info` | Kind and size of a file or a directory. `info->name` is the last component of the path. |
| `br_fs_readdir` | The `index`-th direct child of `dir`, with `""` for the root. `info->name` is the leaf name and `info->dir` tells directories apart. `BR_ERR_NOENT` past the last one or for a missing directory. |
| `br_fs_usage` | Block size, total blocks and free blocks. |

Common errors: `BR_ERR_INVALID` for a bad name, flags, descriptor or a filesystem that is not mounted, `BR_ERR_NOENT` for a missing file, `BR_ERR_NOMEM` when the descriptor table or the free blocks run out, `BR_ERR_BUSY` for a conflicting open, `BR_ERR_IO` for a device error or damaged data.

A path is 1 to `CONFIG_FS_NAME_MAX - 1` printable characters without spaces, with `/` between components. It does not start or end with `/`, and has no empty component. The limit is on the whole path, and a directory path is one shorter, because its marker file carries a trailing `/`.

### Rules for open files

- Opening for write replaces the whole file when it is closed. With `BR_FS_APPEND` the old content is copied into the new version first, so an append needs room for the whole file and takes time proportional to its size, and it is atomic like any write.
- Only one file can be open for write at a time.
- A file that is open cannot be opened for write, removed, or read while it is being written.
- A write descriptor does nothing visible until `br_fs_close` returns `BR_OK`. If the write fails, or the system loses power before the commit, the old version of the file stays.

## Configuration

| Option | Default | Meaning |
|--------|---------|---------|
| `CONFIG_FS_NAME_MAX` | 24 | Name field size with the terminating zero, a multiple of 4 |
| `CONFIG_FS_BLOCK_MAX` | 512 | Largest block size. This is the size of the static write buffer |
| `CONFIG_FS_MAX_BLOCKS` | 256 | Largest block count. One free-map bit per block |
| `CONFIG_FS_MAX_OPEN` | 4 | Descriptor table size |

A device with a larger block size or block count cannot be mounted. `prog_size` can be at most 64 bytes.

## On-disk format

Block 0 is the superblock: magic `BRFS`, version, block size, block count. Every other block is free or belongs to one file.

A file is a chain of blocks linked forward through `next`. The first block is the head.

```
data block   magic 'BRFD' | next | len | data...
head block   magic 'BRFH' | next | len | generation | name[CONFIG_FS_NAME_MAX]
             | size slot | commit slot | dead slot | data...
```

All numbers are 32-bit, in the byte order of the CPU. `next` is `0xFFFFFFFF` in the last block. `len` is the number of data bytes in that block. The three slots in the head are each `prog_size` bytes (at least 4), erased when the head is written, and programmed once later.

### Writing a file

1. Take a free block, erase it and keep the head in the RAM buffer.
2. When the buffer is full, take the next free block, program the buffer with `next` pointing to it, and continue in the new block.
3. At close, program the last block, then the `size` slot, then the `commit` slot (`CMIT`) of the head, then the `dead` slot of the previous version of the file.

A file counts only if its head has the commit value and an erased `dead` slot. A power loss before the commit slot leaves a head that no one refers to, and its blocks are free again at the next mount. A power loss between the commit and the `dead` slot leaves two committed versions, and mount keeps the one with the higher generation and marks the other dead. At every point the file is either the old or the new version. `test_fs` cuts power at every program of a rewrite and checks that.

### Directories

A directory is a file of size 0 whose name is the path with a trailing `/`, for example `logs/`. A file inside is an ordinary file named `logs/a.txt`. Creating a file checks that the marker of its parent exists, removing a directory checks that no file starts with its path. Nothing else about the format changed, so a device written by an earlier version mounts and has no directories.

### Free space

Mount and every change rebuild an in-RAM map of used blocks by walking the chain of every live file. Blocks are allocated round-robin and erased when allocated, which spreads the wear without a counter.

### Limits

- A lookup scans the head of every block, so `open` takes time proportional to the block count.
- There is no data checksum. A bit flip in the middle of a file is not detected.
- Rewriting or appending to a file needs free space for the new version while the old one still exists.
- A lookup in a directory scans every block, so a directory is not faster than a flat namespace.
- Directories cannot be renamed, and there is no `rename` for files either.
- Bad blocks are not tracked.

## Shell commands

`br_shell_register_fs()` (`lib/br_shell_fs.c`) registers:

| Command | Behavior |
|---------|----------|
| `ls [dir]` | List a directory (the root by default), then the number of free blocks |
| `mkdir <dir>` | Create a directory |
| `rm <path>` | Remove a file or an empty directory |
| `cat <file>` | Print a file |
| `write <file> [text...]` | Write the arguments, joined by single spaces, to a file |

Mount a filesystem before using them. `ls` without one fails.
