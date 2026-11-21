/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_fs.h"
#include "br_string.h"

#define FS_MAGIC_SUPER  0x53465242UL
#define FS_MAGIC_HEAD   0x48465242UL
#define FS_MAGIC_DATA   0x44465242UL
#define FS_VERSION      1UL
#define FS_NONE         0xFFFFFFFFUL
#define FS_COMMITTED    0x434D4954UL
#define FS_DEAD         0UL
#define FS_SLOT_MAX     64U

_Static_assert(CONFIG_FS_NAME_MAX % 4 == 0, "CONFIG_FS_NAME_MAX must be a multiple of 4");

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t block_count;
} super_t;

typedef struct {
    uint32_t magic;
    uint32_t next;
    uint32_t len;
} data_hdr_t;

typedef struct {
    uint32_t magic;
    uint32_t next;
    uint32_t len;
    uint32_t gen;
    char     name[CONFIG_FS_NAME_MAX];
} head_fixed_t;

typedef struct {
    uint32_t next;
    uint32_t len;
    uint32_t gen;
    uint32_t size;
    uint32_t commit;
    uint32_t dead;
    char     name[CONFIG_FS_NAME_MAX];
} head_info_t;

typedef struct {
    bool     open;
    bool     writing;
    bool     failed;
    uint32_t head;
    uint32_t old_head;
    uint32_t cur;
    uint32_t blk;
    uint32_t next;
    uint32_t blk_data_off;
    uint32_t blk_len;
    uint32_t blk_off;
    uint32_t h_len;
    uint32_t h_next;
    uint32_t wpos;
    size_t   size;
    size_t   pos;
    char     name[CONFIG_FS_NAME_MAX];
} file_t;

static struct {
    br_blkdev_t *dev;
    bool         mounted;
    br_mutex_t   lock;
    uint32_t     next_gen;
    uint32_t     cursor;
    uint32_t     slot;
    uint32_t     data_off;
    uint32_t     size_off;
    uint32_t     commit_off;
    uint32_t     dead_off;
    uint32_t     head_data_off;
    uint8_t      used[(CONFIG_FS_MAX_BLOCKS + 7) / 8];
    uint8_t      wbuf[CONFIG_FS_BLOCK_MAX] __attribute__((aligned(4)));
    file_t       files[CONFIG_FS_MAX_OPEN];
} fs;

static int read_bytes(file_t *f, void *buf, size_t len);
static int write_bytes(file_t *f, const void *buf, size_t len);

static uint32_t round_up(uint32_t v, uint32_t to)
{
    return (v + to - 1) / to * to;
}

static bool name_equal(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void name_copy(char *dst, const char *src)
{
    size_t i = 0;
    for (; i + 1 < CONFIG_FS_NAME_MAX && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    for (; i < CONFIG_FS_NAME_MAX; i++) {
        dst[i] = '\0';
    }
}

static bool chars_valid(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    size_t n = 0;
    for (; name[n] != '\0'; n++) {
        if (n + 1 >= CONFIG_FS_NAME_MAX || name[n] <= ' ' || name[n] > '~') {
            return false;
        }
    }
    return true;
}

static size_t name_length(const char *name)
{
    size_t n = 0;
    while (name[n] != '\0') {
        n++;
    }
    return n;
}

static bool name_valid(const char *name)
{
    if (!chars_valid(name)) {
        return false;
    }

    size_t n = name_length(name);
    if (name[0] == '/' || name[n - 1] == '/') {
        return false;
    }
    for (size_t i = 0; i + 1 < n; i++) {
        if (name[i] == '/' && name[i + 1] == '/') {
            return false;
        }
    }
    return true;
}

static bool make_marker(char *out, const char *path)
{
    size_t n = name_length(path);

    if (n + 2 > CONFIG_FS_NAME_MAX) {
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        out[i] = path[i];
    }
    out[n] = '/';
    out[n + 1] = '\0';
    return true;
}

static bool starts_with(const char *name, const char *prefix)
{
    while (*prefix != '\0') {
        if (*name++ != *prefix++) {
            return false;
        }
    }
    return true;
}

static const char *leaf_of(const char *path)
{
    const char *leaf = path;
    for (const char *c = path; *c != '\0'; c++) {
        if (*c == '/' && c[1] != '\0') {
            leaf = c + 1;
        }
    }
    return leaf;
}

static bool used_get(uint32_t b)
{
    return (fs.used[b / 8] & (1U << (b % 8))) != 0;
}

static void used_set(uint32_t b)
{
    fs.used[b / 8] |= (uint8_t)(1U << (b % 8));
}

static br_err_t read_word(uint32_t block, uint32_t off, uint32_t *v)
{
    return br_blkdev_read(fs.dev, block, off, v, sizeof(*v));
}

static br_err_t write_slot(uint32_t block, uint32_t off, uint32_t value)
{
    uint8_t slot[FS_SLOT_MAX];

    memset(slot, 0xFF, fs.slot);
    memcpy(slot, &value, sizeof(value));
    return br_blkdev_write(fs.dev, block, off, slot, fs.slot);
}

static br_err_t load_head(uint32_t block, head_info_t *info)
{
    head_fixed_t h;

    br_err_t err = br_blkdev_read(fs.dev, block, 0, &h, sizeof(h));
    if (err != BR_OK) {
        return err;
    }
    if (h.magic != FS_MAGIC_HEAD) {
        return BR_ERR_NOENT;
    }

    info->next = h.next;
    info->len  = h.len;
    info->gen  = h.gen;
    memcpy(info->name, h.name, sizeof(info->name));
    info->name[CONFIG_FS_NAME_MAX - 1] = '\0';

    err = read_word(block, fs.size_off, &info->size);
    if (err == BR_OK) {
        err = read_word(block, fs.commit_off, &info->commit);
    }
    if (err == BR_OK) {
        err = read_word(block, fs.dead_off, &info->dead);
    }
    return err;
}

static bool head_live(const head_info_t *info)
{
    return info->commit == FS_COMMITTED && info->dead == FS_NONE;
}

static br_err_t find_live(const char *name, uint32_t *block, head_info_t *out)
{
    bool found = false;

    for (uint32_t b = 1; b < fs.dev->block_count; b++) {
        head_info_t info;
        br_err_t err = load_head(b, &info);
        if (err == BR_ERR_NOENT) {
            continue;
        }
        if (err != BR_OK) {
            return err;
        }
        if (!head_live(&info) || !name_equal(info.name, name)) {
            continue;
        }
        if (!found || info.gen > out->gen) {
            *block = b;
            *out   = info;
            found  = true;
        }
    }
    return found ? BR_OK : BR_ERR_NOENT;
}

static void mark_chain(uint32_t head)
{
    uint32_t b = head;

    for (uint32_t steps = 0; steps < fs.dev->block_count; steps++) {
        used_set(b);

        data_hdr_t h;
        if (br_blkdev_read(fs.dev, b, 0, &h, sizeof(h)) != BR_OK ||
            h.next == FS_NONE || h.next >= fs.dev->block_count || h.next == 0) {
            break;
        }
        b = h.next;
    }
}

static br_err_t rebuild_used(void)
{
    memset(fs.used, 0, sizeof(fs.used));
    used_set(0);

    for (uint32_t b = 1; b < fs.dev->block_count; b++) {
        head_info_t info;
        br_err_t err = load_head(b, &info);
        if (err == BR_ERR_NOENT) {
            continue;
        }
        if (err != BR_OK) {
            return err;
        }
        if (head_live(&info)) {
            mark_chain(b);
        }
    }
    return BR_OK;
}

static br_err_t alloc_block(uint32_t *out)
{
    uint32_t count = fs.dev->block_count;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t b = (fs.cursor + i) % count;
        if (b == 0 || used_get(b)) {
            continue;
        }

        br_err_t err = br_blkdev_erase(fs.dev, b);
        if (err != BR_OK) {
            return err;
        }
        used_set(b);
        fs.cursor = b + 1;
        *out = b;
        return BR_OK;
    }
    return BR_ERR_NOMEM;
}

static void buffer_reset(void)
{
    memset(fs.wbuf, 0xFF, fs.dev->block_size);
}

static br_err_t flush_block(file_t *f, uint32_t next)
{
    bool is_head = f->cur == f->head;
    uint32_t data_start = is_head ? fs.head_data_off : fs.data_off;

    if (is_head) {
        head_fixed_t h;
        h.magic = FS_MAGIC_HEAD;
        h.next  = next;
        h.len   = f->wpos - data_start;
        h.gen   = fs.next_gen - 1;
        name_copy(h.name, f->name);
        memcpy(fs.wbuf, &h, sizeof(h));
    } else {
        data_hdr_t h;
        h.magic = FS_MAGIC_DATA;
        h.next  = next;
        h.len   = f->wpos - data_start;
        memcpy(fs.wbuf, &h, sizeof(h));
    }

    return br_blkdev_write(fs.dev, f->cur, 0, fs.wbuf,
                           round_up(f->wpos, fs.dev->prog_size));
}

static int fd_alloc(void)
{
    for (int i = 0; i < CONFIG_FS_MAX_OPEN; i++) {
        if (!fs.files[i].open) {
            return i;
        }
    }
    return BR_ERR_NOMEM;
}

static file_t *fd_get(int fd)
{
    if (fd < 0 || fd >= CONFIG_FS_MAX_OPEN || !fs.files[fd].open) {
        return NULL;
    }
    return &fs.files[fd];
}

static bool name_open(const char *name, bool *writer_present)
{
    bool any = false;

    for (int i = 0; i < CONFIG_FS_MAX_OPEN; i++) {
        if (fs.files[i].open) {
            if (fs.files[i].writing) {
                *writer_present = true;
            }
            if (name_equal(fs.files[i].name, name)) {
                any = true;
            }
        }
    }
    return any;
}

static br_err_t geometry_ok(const br_blkdev_t *dev)
{
    if (dev == NULL || dev->ops == NULL || dev->prog_size == 0 ||
        dev->prog_size > FS_SLOT_MAX || dev->block_count < 2 ||
        dev->block_size % dev->prog_size != 0 ||
        dev->block_size > CONFIG_FS_BLOCK_MAX ||
        dev->block_count > CONFIG_FS_MAX_BLOCKS) {
        return BR_ERR_INVALID;
    }

    uint32_t slot = dev->prog_size < 4 ? 4 : dev->prog_size;
    uint32_t head = round_up(sizeof(head_fixed_t), slot) + 3 * slot;
    if (dev->block_size < head + slot || slot > FS_SLOT_MAX) {
        return BR_ERR_INVALID;
    }
    return BR_OK;
}

static void geometry_init(br_blkdev_t *dev)
{
    fs.dev  = dev;
    fs.slot = dev->prog_size < 4 ? 4 : dev->prog_size;
    fs.size_off      = round_up(sizeof(head_fixed_t), fs.slot);
    fs.commit_off    = fs.size_off + fs.slot;
    fs.dead_off      = fs.commit_off + fs.slot;
    fs.head_data_off = fs.dead_off + fs.slot;
    fs.data_off      = round_up(sizeof(data_hdr_t), fs.slot);
}

static br_err_t lock(void)
{
    if (br_hal_in_isr() || !fs.mounted) {
        return BR_ERR_INVALID;
    }
    return br_mutex_lock(&fs.lock, BR_TIME_INFINITE);
}

br_err_t br_fs_format(br_blkdev_t *dev)
{
    if (fs.mounted) {
        return BR_ERR_BUSY;
    }
    br_err_t err = geometry_ok(dev);
    if (err != BR_OK) {
        return err;
    }

    for (uint32_t b = 0; b < dev->block_count; b++) {
        err = br_blkdev_erase(dev, b);
        if (err != BR_OK) {
            return err;
        }
    }

    uint8_t buf[sizeof(super_t) + FS_SLOT_MAX];
    uint32_t len = round_up(sizeof(super_t), dev->prog_size);
    super_t s = {
        .magic = FS_MAGIC_SUPER, .version = FS_VERSION,
        .block_size = dev->block_size, .block_count = dev->block_count,
    };

    memset(buf, 0xFF, len);
    memcpy(buf, &s, sizeof(s));
    err = br_blkdev_write(dev, 0, 0, buf, len);
    if (err != BR_OK) {
        return err;
    }
    return br_blkdev_sync(dev);
}

br_err_t br_fs_mount(br_blkdev_t *dev)
{
    if (fs.mounted) {
        return BR_ERR_BUSY;
    }
    br_err_t err = geometry_ok(dev);
    if (err != BR_OK) {
        return err;
    }

    super_t s;
    err = br_blkdev_read(dev, 0, 0, &s, sizeof(s));
    if (err != BR_OK) {
        return err;
    }
    if (s.magic != FS_MAGIC_SUPER || s.version != FS_VERSION ||
        s.block_size != dev->block_size || s.block_count != dev->block_count) {
        return BR_ERR_IO;
    }

    geometry_init(dev);
    br_mutex_init(&fs.lock);
    memset(fs.files, 0, sizeof(fs.files));
    fs.next_gen = 1;
    fs.cursor   = 1;
    fs.mounted  = true;

    for (uint32_t b = 1; b < dev->block_count; b++) {
        head_info_t info;
        err = load_head(b, &info);
        if (err == BR_ERR_NOENT) {
            continue;
        }
        if (err != BR_OK) {
            fs.mounted = false;
            return err;
        }
        if (info.gen >= fs.next_gen && info.gen != FS_NONE) {
            fs.next_gen = info.gen + 1;
        }
    }

    for (uint32_t b = 1; b < dev->block_count; b++) {
        head_info_t info;
        head_info_t best;
        uint32_t    best_block;

        if (load_head(b, &info) != BR_OK || !head_live(&info)) {
            continue;
        }
        if (find_live(info.name, &best_block, &best) == BR_OK && best_block != b) {
            err = write_slot(b, fs.dead_off, FS_DEAD);
            if (err != BR_OK) {
                fs.mounted = false;
                return err;
            }
        }
    }

    err = rebuild_used();
    if (err != BR_OK) {
        fs.mounted = false;
    }
    return err;
}

br_err_t br_fs_unmount(void)
{
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    for (int i = 0; i < CONFIG_FS_MAX_OPEN; i++) {
        if (fs.files[i].open) {
            br_mutex_unlock(&fs.lock);
            return BR_ERR_BUSY;
        }
    }

    err = br_blkdev_sync(fs.dev);
    fs.mounted = false;
    br_mutex_unlock(&fs.lock);
    return err;
}

static br_err_t parent_ok(const char *path)
{
    size_t n = name_length(path);
    size_t k = n;

    while (k > 0 && path[k - 1] != '/') {
        k--;
    }
    if (k == 0) {
        return BR_OK;
    }

    char marker[CONFIG_FS_NAME_MAX];
    for (size_t i = 0; i < k; i++) {
        marker[i] = path[i];
    }
    marker[k] = '\0';

    uint32_t    block;
    head_info_t info;
    return find_live(marker, &block, &info);
}

static int open_read(const char *name)
{
    uint32_t    block;
    head_info_t info;

    br_err_t err = find_live(name, &block, &info);
    if (err != BR_OK) {
        return err;
    }

    int fd = fd_alloc();
    if (fd < 0) {
        return fd;
    }

    file_t *f = &fs.files[fd];
    memset(f, 0, sizeof(*f));
    f->open         = true;
    f->head         = block;
    f->blk          = block;
    f->next         = info.next;
    f->blk_data_off = fs.head_data_off;
    f->blk_len      = info.len;
    f->h_len        = info.len;
    f->h_next       = info.next;
    f->size         = info.size;
    name_copy(f->name, name);
    return fd;
}

static int open_write(const char *name, bool append)
{
    int fd = fd_alloc();
    if (fd < 0) {
        return fd;
    }

    uint32_t    old = FS_NONE;
    head_info_t info;
    uint32_t    found;
    if (find_live(name, &found, &info) == BR_OK) {
        old = found;
    }

    uint32_t head;
    br_err_t err = alloc_block(&head);
    if (err != BR_OK) {
        return err;
    }

    buffer_reset();

    file_t *f = &fs.files[fd];
    memset(f, 0, sizeof(*f));
    f->open     = true;
    f->writing  = true;
    f->head     = head;
    f->cur      = head;
    f->old_head = old;
    f->wpos     = fs.head_data_off;
    name_copy(f->name, name);
    fs.next_gen++;

    if (append && old != FS_NONE) {
        file_t r;
        memset(&r, 0, sizeof(r));
        r.head         = old;
        r.blk          = old;
        r.next         = info.next;
        r.blk_data_off = fs.head_data_off;
        r.blk_len      = info.len;
        r.size         = info.size;

        uint8_t chunk[64];
        int n;
        while ((n = read_bytes(&r, chunk, sizeof(chunk))) > 0) {
            if (write_bytes(f, chunk, (size_t)n) != n) {
                n = BR_ERR_IO;
                break;
            }
        }
        if (n < 0) {
            f->open = false;
            rebuild_used();
            return n;
        }
    }
    return fd;
}

int br_fs_open(const char *name, uint8_t flags)
{
    if (!name_valid(name) ||
        (flags != BR_FS_READ && flags != BR_FS_WRITE && flags != (BR_FS_WRITE | BR_FS_APPEND))) {
        return BR_ERR_INVALID;
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    bool writer = false;
    bool same   = name_open(name, &writer);
    int  fd;

    if (flags == BR_FS_READ) {
        fd = (same && writer) ? BR_ERR_BUSY : open_read(name);
    } else if (same || writer) {
        fd = BR_ERR_BUSY;
    } else {
        char marker[CONFIG_FS_NAME_MAX];
        uint32_t    block;
        head_info_t info;

        err = parent_ok(name);
        if (err == BR_OK && make_marker(marker, name) && find_live(marker, &block, &info) == BR_OK) {
            err = BR_ERR_BUSY;
        }
        fd = err != BR_OK ? (int)err : open_write(name, (flags & BR_FS_APPEND) != 0);
    }

    br_mutex_unlock(&fs.lock);
    return fd;
}

static br_err_t advance_chunk(file_t *f)
{
    data_hdr_t h;

    if (f->next == FS_NONE || f->next == 0 || f->next >= fs.dev->block_count ||
        br_blkdev_read(fs.dev, f->next, 0, &h, sizeof(h)) != BR_OK ||
        h.magic != FS_MAGIC_DATA || h.len > fs.dev->block_size - fs.data_off) {
        return BR_ERR_IO;
    }

    f->blk          = f->next;
    f->blk_data_off = fs.data_off;
    f->blk_len      = h.len;
    f->blk_off      = 0;
    f->next         = h.next;
    return BR_OK;
}

static int read_bytes(file_t *f, void *buf, size_t len)
{
    if (len > f->size - f->pos) {
        len = f->size - f->pos;
    }

    uint8_t *out    = buf;
    size_t   done   = 0;
    int      result = 0;

    while (done < len) {
        if (f->blk_off == f->blk_len) {
            br_err_t err = advance_chunk(f);
            if (err != BR_OK) {
                result = err;
                break;
            }
            continue;
        }

        size_t chunk = len - done;
        if (chunk > f->blk_len - f->blk_off) {
            chunk = f->blk_len - f->blk_off;
        }

        br_err_t err = br_blkdev_read(fs.dev, f->blk, f->blk_data_off + f->blk_off,
                                      out + done, chunk);
        if (err != BR_OK) {
            result = err;
            break;
        }
        f->blk_off += (uint32_t)chunk;
        done       += chunk;
    }

    f->pos += done;
    return (done == 0 && result != 0) ? result : (int)done;
}

int br_fs_read(int fd, void *buf, size_t len)
{
    if (buf == NULL) {
        return BR_ERR_INVALID;
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    file_t *f = fd_get(fd);
    int result = (f == NULL || f->writing) ? BR_ERR_INVALID : read_bytes(f, buf, len);

    br_mutex_unlock(&fs.lock);
    return result;
}

br_err_t br_fs_seek(int fd, size_t offset)
{
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    file_t *f = fd_get(fd);
    if (f == NULL || f->writing || offset > f->size) {
        br_mutex_unlock(&fs.lock);
        return BR_ERR_INVALID;
    }

    f->blk          = f->head;
    f->blk_data_off = fs.head_data_off;
    f->blk_len      = f->h_len;
    f->blk_off      = 0;
    f->next         = f->h_next;
    f->pos          = 0;

    size_t remaining = offset;
    while (remaining > 0) {
        if (f->blk_off == f->blk_len) {
            err = advance_chunk(f);
            if (err != BR_OK) {
                break;
            }
            continue;
        }

        size_t step = remaining;
        if (step > f->blk_len - f->blk_off) {
            step = f->blk_len - f->blk_off;
        }
        f->blk_off += (uint32_t)step;
        f->pos     += step;
        remaining  -= step;
    }

    br_mutex_unlock(&fs.lock);
    return err;
}

static int write_bytes(file_t *f, const void *buf, size_t len)
{
    const uint8_t *in = buf;
    size_t done = 0;
    int result = 0;

    while (done < len) {
        uint32_t room = fs.dev->block_size - f->wpos;

        if (room == 0) {
            uint32_t next;
            br_err_t err = alloc_block(&next);
            if (err == BR_OK) {
                err = flush_block(f, next);
            }
            if (err != BR_OK) {
                result = err;
                f->failed = true;
                break;
            }
            f->cur  = next;
            f->wpos = fs.data_off;
            buffer_reset();
            continue;
        }

        size_t chunk = len - done;
        if (chunk > room) {
            chunk = room;
        }
        memcpy(fs.wbuf + f->wpos, in + done, chunk);
        f->wpos += (uint32_t)chunk;
        done    += chunk;
    }

    f->size += done;
    return (done == 0 && result != 0) ? result : (int)done;
}

int br_fs_write(int fd, const void *buf, size_t len)
{
    if (buf == NULL) {
        return BR_ERR_INVALID;
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    file_t *f = fd_get(fd);
    int result = (f == NULL || !f->writing) ? BR_ERR_INVALID : write_bytes(f, buf, len);

    br_mutex_unlock(&fs.lock);
    return result;
}

static br_err_t commit_file(file_t *f)
{
    br_err_t err = flush_block(f, FS_NONE);
    if (err == BR_OK) {
        err = br_blkdev_sync(fs.dev);
    }
    if (err == BR_OK) {
        err = write_slot(f->head, fs.size_off, (uint32_t)f->size);
    }
    if (err == BR_OK) {
        err = write_slot(f->head, fs.commit_off, FS_COMMITTED);
    }
    if (err == BR_OK && f->old_head != FS_NONE) {
        err = write_slot(f->old_head, fs.dead_off, FS_DEAD);
    }
    if (err == BR_OK) {
        err = br_blkdev_sync(fs.dev);
    }
    return err;
}

static br_err_t close_fd(int fd)
{
    file_t *f = fd_get(fd);
    if (f == NULL) {
        return BR_ERR_INVALID;
    }

    br_err_t err = BR_OK;
    if (f->writing) {
        err = f->failed ? BR_ERR_IO : commit_file(f);
        br_err_t scan = rebuild_used();
        if (err == BR_OK) {
            err = scan;
        }
    }

    f->open = false;
    return err;
}

br_err_t br_fs_close(int fd)
{
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    err = close_fd(fd);
    br_mutex_unlock(&fs.lock);
    return err;
}

br_err_t br_fs_remove(const char *name)
{
    if (!name_valid(name)) {
        return BR_ERR_INVALID;
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    bool writer = false;
    if (name_open(name, &writer)) {
        br_mutex_unlock(&fs.lock);
        return BR_ERR_BUSY;
    }

    uint32_t    block;
    head_info_t info;
    err = find_live(name, &block, &info);
    if (err == BR_OK) {
        err = write_slot(block, fs.dead_off, FS_DEAD);
    }
    if (err == BR_OK) {
        err = rebuild_used();
    }

    br_mutex_unlock(&fs.lock);
    return err;
}

br_err_t br_fs_stat(const char *name, size_t *size)
{
    if (!name_valid(name) || size == NULL) {
        return BR_ERR_INVALID;
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    uint32_t    block;
    head_info_t info;
    err = find_live(name, &block, &info);
    if (err == BR_OK) {
        *size = info.size;
    }

    br_mutex_unlock(&fs.lock);
    return err;
}

br_err_t br_fs_list(uint32_t index, br_fs_info_t *out)
{
    if (out == NULL) {
        return BR_ERR_INVALID;
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    err = BR_ERR_NOENT;
    uint32_t seen = 0;

    for (uint32_t b = 1; b < fs.dev->block_count; b++) {
        head_info_t info;
        br_err_t r = load_head(b, &info);
        if (r == BR_ERR_NOENT) {
            continue;
        }
        if (r != BR_OK) {
            err = r;
            break;
        }
        if (!head_live(&info)) {
            continue;
        }
        if (seen++ == index) {
            memcpy(out->name, info.name, sizeof(out->name));
            out->size = info.size;
            out->dir  = info.name[name_length(info.name) - 1] == '/';
            err = BR_OK;
            break;
        }
    }

    br_mutex_unlock(&fs.lock);
    return err;
}

br_err_t br_fs_usage(br_fs_usage_t *usage)
{
    if (usage == NULL) {
        return BR_ERR_INVALID;
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    uint32_t free_blocks = 0;
    for (uint32_t b = 1; b < fs.dev->block_count; b++) {
        if (!used_get(b)) {
            free_blocks++;
        }
    }
    usage->block_size   = fs.dev->block_size;
    usage->total_blocks = fs.dev->block_count;
    usage->free_blocks  = free_blocks;

    br_mutex_unlock(&fs.lock);
    return BR_OK;
}

br_err_t br_fs_mkdir(const char *path)
{
    char marker[CONFIG_FS_NAME_MAX];

    if (!name_valid(path) || !make_marker(marker, path)) {
        return BR_ERR_INVALID;
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    uint32_t    block;
    head_info_t info;
    bool        writer = false;

    if (name_open(marker, &writer) || writer) {
        err = BR_ERR_BUSY;
    } else if (find_live(path, &block, &info) == BR_OK ||
               find_live(marker, &block, &info) == BR_OK) {
        err = BR_ERR_BUSY;
    } else {
        err = parent_ok(path);
        if (err == BR_OK) {
            int fd = open_write(marker, false);
            err = fd < 0 ? (br_err_t)fd : close_fd(fd);
        }
    }

    br_mutex_unlock(&fs.lock);
    return err;
}

static bool has_children(const char *marker)
{
    for (uint32_t b = 1; b < fs.dev->block_count; b++) {
        head_info_t info;
        if (load_head(b, &info) != BR_OK || !head_live(&info)) {
            continue;
        }
        if (starts_with(info.name, marker) && !name_equal(info.name, marker)) {
            return true;
        }
    }
    return false;
}

br_err_t br_fs_rmdir(const char *path)
{
    char marker[CONFIG_FS_NAME_MAX];

    if (!name_valid(path) || !make_marker(marker, path)) {
        return BR_ERR_INVALID;
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    uint32_t    block;
    head_info_t info;
    bool        writer = false;

    err = find_live(marker, &block, &info);
    if (err == BR_OK && (has_children(marker) || name_open(marker, &writer) || writer)) {
        err = BR_ERR_BUSY;
    }
    if (err == BR_OK) {
        err = write_slot(block, fs.dead_off, FS_DEAD);
    }
    if (err == BR_OK) {
        err = rebuild_used();
    }

    br_mutex_unlock(&fs.lock);
    return err;
}

br_err_t br_fs_info(const char *path, br_fs_info_t *out)
{
    char marker[CONFIG_FS_NAME_MAX];

    if (!name_valid(path) || out == NULL || !make_marker(marker, path)) {
        return BR_ERR_INVALID;
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    uint32_t    block;
    head_info_t info;

    err = find_live(path, &block, &info);
    if (err == BR_OK) {
        out->size = info.size;
        out->dir  = false;
    } else if (find_live(marker, &block, &info) == BR_OK) {
        err = BR_OK;
        out->size = 0;
        out->dir  = true;
    }
    if (err == BR_OK) {
        name_copy(out->name, leaf_of(path));
    }

    br_mutex_unlock(&fs.lock);
    return err;
}

br_err_t br_fs_readdir(const char *dir, uint32_t index, br_fs_info_t *out)
{
    char prefix[CONFIG_FS_NAME_MAX];
    bool root = dir == NULL || dir[0] == '\0';

    if (out == NULL || (!root && (!name_valid(dir) || !make_marker(prefix, dir)))) {
        return BR_ERR_INVALID;
    }
    if (root) {
        prefix[0] = '\0';
    }
    br_err_t err = lock();
    if (err != BR_OK) {
        return err;
    }

    uint32_t    block;
    head_info_t info;

    if (!root && find_live(prefix, &block, &info) != BR_OK) {
        br_mutex_unlock(&fs.lock);
        return BR_ERR_NOENT;
    }

    err = BR_ERR_NOENT;
    uint32_t seen = 0;
    size_t   plen = name_length(prefix);

    for (uint32_t b = 1; b < fs.dev->block_count; b++) {
        if (load_head(b, &info) != BR_OK || !head_live(&info) ||
            !starts_with(info.name, prefix) || info.name[plen] == '\0') {
            continue;
        }

        const char *rest = info.name + plen;
        size_t      n    = name_length(rest);
        size_t      slashes = 0;
        for (size_t i = 0; i < n; i++) {
            slashes += rest[i] == '/';
        }

        bool is_dir  = slashes == 1 && rest[n - 1] == '/';
        bool is_file = slashes == 0;
        if (!is_dir && !is_file) {
            continue;
        }

        if (seen++ == index) {
            name_copy(out->name, rest);
            if (is_dir) {
                out->name[n - 1] = '\0';
            }
            out->size = is_dir ? 0 : info.size;
            out->dir  = is_dir;
            err = BR_OK;
            break;
        }
    }

    br_mutex_unlock(&fs.lock);
    return err;
}

#if CONFIG_MPU

#include "bedrock/br_syscall.h"

#define USER_PATH_MAX  (CONFIG_FS_NAME_MAX * 2)

static struct {
    br_tid_t owner;
    bool     used;
} fd_owner[CONFIG_FS_MAX_OPEN];

static bool fd_ok(const br_tcb_t *t, uint32_t fd)
{
    return fd < CONFIG_FS_MAX_OPEN && fd_owner[fd].used && fd_owner[fd].owner == t->id;
}

static bool out_ok(const br_tcb_t *t, uint32_t ptr, size_t size)
{
    return ptr % 4 == 0 && br_user_access_ok(t, (const void *)ptr, size, BR_MEM_W);
}

static int32_t fs_path(const br_tcb_t *t, uint32_t ptr, char *out)
{
    int n = br_user_copy_string(t, (const void *)ptr, out, USER_PATH_MAX);
    return n < 0 ? n : BR_OK;
}

static uint32_t fs_handler(br_tcb_t *t, const uint32_t *a)
{
    char    path[USER_PATH_MAX];
    int32_t r = BR_ERR_INVALID;

    switch (a[0]) {
    case BR_SYS_FS_OPEN:
        r = fs_path(t, a[1], path);
        if (r == BR_OK) {
            r = br_fs_open(path, (uint8_t)a[2]);
            if (r >= 0 && r < CONFIG_FS_MAX_OPEN) {
                fd_owner[r].owner = t->id;
                fd_owner[r].used  = true;
            }
        }
        break;

    case BR_SYS_FS_READ:
    case BR_SYS_FS_WRITE: {
        bool rd = a[0] == BR_SYS_FS_READ;
        if (!fd_ok(t, a[1])) {
            r = BR_ERR_INVALID;
        } else if (!br_user_access_ok(t, (const void *)a[2], a[3], rd ? BR_MEM_W : BR_MEM_R)) {
            r = BR_ERR_PERM;
        } else {
            r = rd ? br_fs_read((int)a[1], (void *)a[2], a[3])
                   : br_fs_write((int)a[1], (const void *)a[2], a[3]);
        }
        break;
    }

    case BR_SYS_FS_SEEK:
        r = fd_ok(t, a[1]) ? br_fs_seek((int)a[1], a[2]) : BR_ERR_INVALID;
        break;

    case BR_SYS_FS_CLOSE:
        if (fd_ok(t, a[1])) {
            r = br_fs_close((int)a[1]);
            fd_owner[a[1]].used = false;
        }
        break;

    case BR_SYS_FS_REMOVE:
    case BR_SYS_FS_MKDIR:
    case BR_SYS_FS_RMDIR:
        r = fs_path(t, a[1], path);
        if (r == BR_OK) {
            r = a[0] == BR_SYS_FS_REMOVE ? br_fs_remove(path)
              : a[0] == BR_SYS_FS_MKDIR  ? br_fs_mkdir(path) : br_fs_rmdir(path);
        }
        break;

    case BR_SYS_FS_STAT: {
        size_t size = 0;
        r = fs_path(t, a[1], path);
        if (r == BR_OK && !out_ok(t, a[2], sizeof(uint32_t))) {
            r = BR_ERR_PERM;
        } else if (r == BR_OK) {
            r = br_fs_stat(path, &size);
            if (r == BR_OK) {
                *(uint32_t *)a[2] = (uint32_t)size;
            }
        }
        break;
    }

    case BR_SYS_FS_INFO: {
        br_fs_info_t info;
        r = fs_path(t, a[1], path);
        if (r == BR_OK && !out_ok(t, a[2], sizeof(info))) {
            r = BR_ERR_PERM;
        } else if (r == BR_OK) {
            r = br_fs_info(path, &info);
            if (r == BR_OK) {
                *(br_fs_info_t *)a[2] = info;
            }
        }
        break;
    }

    case BR_SYS_FS_READDIR: {
        br_fs_info_t info;
        r = fs_path(t, a[1], path);
        if (r == BR_OK && !out_ok(t, a[3], sizeof(info))) {
            r = BR_ERR_PERM;
        } else if (r == BR_OK) {
            r = br_fs_readdir(path, a[2], &info);
            if (r == BR_OK) {
                *(br_fs_info_t *)a[3] = info;
            }
        }
        break;
    }

    case BR_SYS_FS_USAGE: {
        br_fs_usage_t u;
        if (!out_ok(t, a[1], sizeof(u))) {
            r = BR_ERR_PERM;
        } else {
            r = br_fs_usage(&u);
            if (r == BR_OK) {
                *(br_fs_usage_t *)a[1] = u;
            }
        }
        break;
    }

    default:
        break;
    }

    return (uint32_t)r;
}

static void fs_release(br_tid_t tid)
{
    for (int i = 0; i < CONFIG_FS_MAX_OPEN; i++) {
        if (fd_owner[i].used && fd_owner[i].owner == tid) {
            br_fs_close(i);
            fd_owner[i].used = false;
        }
    }
}

br_err_t br_fs_enable_user_calls(void)
{
    br_err_t err = br_syscall_register(BR_SYS_FS_OPEN, 12, fs_handler);
    if (err == BR_OK) {
        err = br_user_release_register(fs_release);
    }
    return err;
}

#endif
