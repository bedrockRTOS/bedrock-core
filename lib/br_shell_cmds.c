/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_shell.h"
#include "br_console.h"
#include "br_pool.h"

static const char *state_name(br_task_state_t state)
{
    switch (state) {
    case BR_TASK_READY:     return "ready";
    case BR_TASK_RUNNING:   return "running";
    case BR_TASK_BLOCKED:   return "blocked";
    case BR_TASK_SUSPENDED: return "suspended";
    default:                return "inactive";
    }
}

static int tasks_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if CONFIG_TASK_STATS
    br_time_t now = br_uptime_us();
#endif

    br_printf("%-3s %-12s %-9s %4s %11s %5s\n",
              "ID", "NAME", "STATE", "PRIO", "STACK", "CPU");

    for (int tid = 0; tid < CONFIG_MAX_TASKS; tid++) {
        br_task_info_t info;
        if (br_task_get_info((br_tid_t)tid, &info) != BR_OK) {
            continue;
        }

        br_printf("%-3d %-12s %-9s %4u %5zu/%-5zu ",
                  tid,
                  info.name != NULL ? info.name : "-",
                  state_name(info.state),
                  (unsigned)info.priority,
                  info.stack_used, info.stack_size);

#if CONFIG_TASK_STATS
        unsigned percent = 0;
        if (now != 0) {
            percent = (unsigned)(info.cpu_time_us * 100 / now);
        }
        br_printf("%3u%%\n", percent);
#else
        br_printf("  -\n");
#endif
    }
    return 0;
}

static int mem_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    size_t count = br_pool_count();
    if (count == 0) {
        br_printf("no pools\n");
        return 0;
    }

    br_printf("%-4s %6s %6s %6s\n", "POOL", "BLOCK", "FREE", "TOTAL");
    for (size_t i = 0; i < count; i++) {
        br_pool_handle_t pool = br_pool_get(i);
        br_printf("%-4zu %6zu %6zu %6zu\n", i, br_pool_block_size(pool),
                  br_pool_available(pool), br_pool_total(pool));
    }
    return 0;
}

static int uptime_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    br_time_t us = br_uptime_us();
    unsigned long long total_s = us / 1000000U;

    br_printf("%llu.%06u s\n", total_s, (unsigned)(us % 1000000U));
    return 0;
}

static int version_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    br_printf("bedrock[RTOS] %d.%d.%d\n",
              BR_VERSION_MAJOR, BR_VERSION_MINOR, BR_VERSION_PATCH);
    return 0;
}

static int reboot_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    br_printf("rebooting\n");
    br_hal_reset();
}

static br_shell_cmd_t tasks_cmd = {
    .name    = "tasks",
    .help    = "list tasks with state, priority and stack usage",
    .handler = tasks_handler,
};

static br_shell_cmd_t mem_cmd = {
    .name    = "mem",
    .help    = "show memory pool usage",
    .handler = mem_handler,
};

static br_shell_cmd_t uptime_cmd = {
    .name    = "uptime",
    .help    = "show time since boot",
    .handler = uptime_handler,
};

static br_shell_cmd_t version_cmd = {
    .name    = "version",
    .help    = "show kernel version",
    .handler = version_handler,
};

static br_shell_cmd_t reboot_cmd = {
    .name    = "reboot",
    .help    = "reset the system",
    .handler = reboot_handler,
};

void br_shell_register_builtin(void)
{
    br_shell_register(&tasks_cmd);
    br_shell_register(&mem_cmd);
    br_shell_register(&uptime_cmd);
    br_shell_register(&version_cmd);
    br_shell_register(&reboot_cmd);
}
