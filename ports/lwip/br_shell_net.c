/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "br_net.h"
#include "br_shell.h"
#include "br_console.h"

#define PING_COUNT_DEFAULT  4
#define PING_COUNT_MAX      100

static bool parse_u32(const char *s, uint32_t max, uint32_t *out)
{
    if (*s == '\0') {
        return false;
    }
    uint32_t v = 0;
    for (; *s != '\0'; s++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
        v = v * 10 + (uint32_t)(*s - '0');
        if (v > max) {
            return false;
        }
    }
    *out = v;
    return true;
}

static bool parse_ip4(const char *s, uint32_t *ip)
{
    uint32_t addr = 0;

    for (int part = 0; part < 4; part++) {
        char field[4];
        int n = 0;
        while (*s != '\0' && *s != '.') {
            if (n == 3) {
                return false;
            }
            field[n++] = *s++;
        }
        field[n] = '\0';

        uint32_t v;
        if (!parse_u32(field, 255, &v)) {
            return false;
        }
        addr = (addr << 8) | v;

        if (part < 3) {
            if (*s != '.') {
                return false;
            }
            s++;
        }
    }
    if (*s != '\0') {
        return false;
    }
    *ip = addr;
    return true;
}

static void print_ip(uint32_t ip)
{
    br_printf("%u.%u.%u.%u", (unsigned)(ip >> 24), (unsigned)((ip >> 16) & 0xFF),
              (unsigned)((ip >> 8) & 0xFF), (unsigned)(ip & 0xFF));
}

static int ifconfig_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    br_net_ifinfo_t info;
    for (uint32_t i = 0; br_net_ifinfo(i, &info) == BR_OK; i++) {
        br_printf("%s: %s\n", info.name, info.up ? "up" : "down");
        if (info.has_mac) {
            br_printf("  mac  %02x:%02x:%02x:%02x:%02x:%02x\n", info.mac[0], info.mac[1],
                      info.mac[2], info.mac[3], info.mac[4], info.mac[5]);
        }
        br_printf("  addr ");
        print_ip(info.ip);
        br_printf("  mask ");
        print_ip(info.mask);
        br_printf("  gw ");
        print_ip(info.gw);
        br_printf("\n");
    }
    return 0;
}

static int ping_handler(int argc, char **argv)
{
    uint32_t ip;
    uint32_t count = PING_COUNT_DEFAULT;

    if (argc < 2 || argc > 3 || !parse_ip4(argv[1], &ip) ||
        (argc == 3 && (!parse_u32(argv[2], PING_COUNT_MAX, &count) || count == 0))) {
        br_printf("usage: ping <a.b.c.d> [count]\n");
        return -1;
    }

    uint32_t received = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t rtt;
        br_err_t err = br_net_ping(ip, BR_SEC(1), &rtt);
        if (err == BR_OK) {
            received++;
            br_printf("reply from %s: time=%u us\n", argv[1], (unsigned)rtt);
        } else if (err == BR_ERR_TIMEOUT) {
            br_printf("request timed out\n");
        } else {
            br_printf("ping failed: %d\n", (int)err);
            return -1;
        }
        if (i + 1 < count) {
            br_sleep_ms(200);
        }
    }

    br_printf("%u sent, %u received\n", (unsigned)count, (unsigned)received);
    return received > 0 ? 0 : -1;
}

static int nslookup_handler(int argc, char **argv)
{
    if (argc != 2) {
        br_printf("usage: nslookup <name>\n");
        return -1;
    }

    uint32_t ip;
    br_err_t err = br_net_resolve(argv[1], &ip);
    if (err != BR_OK) {
        br_printf("cannot resolve %s: %d\n", argv[1], (int)err);
        return -1;
    }

    br_printf("%s is ", argv[1]);
    print_ip(ip);
    br_printf("\n");
    return 0;
}

static br_shell_cmd_t nslookup_cmd = {
    .name    = "nslookup",
    .help    = "nslookup <name>",
    .handler = nslookup_handler,
};

static br_shell_cmd_t ifconfig_cmd = {
    .name    = "ifconfig",
    .help    = "show network interfaces",
    .handler = ifconfig_handler,
};

static br_shell_cmd_t ping_cmd = {
    .name    = "ping",
    .help    = "ping <a.b.c.d> [count]",
    .handler = ping_handler,
};

void br_shell_register_net(void)
{
    br_shell_register(&ifconfig_cmd);
    br_shell_register(&ping_cmd);
    br_shell_register(&nslookup_cmd);
}
