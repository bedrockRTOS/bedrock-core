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

static void print_ip(const br_ip_t *ip)
{
    char text[48];
    if (br_ip_format(ip, text, sizeof(text)) < 0) {
        br_printf("-");
    } else {
        br_printf("%s", text);
    }
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
        print_ip(&info.ip);
        br_printf("  mask ");
        print_ip(&info.mask);
        br_printf("  gw ");
        print_ip(&info.gw);
        br_printf("\n");
        for (uint8_t k = 0; k < info.ip6_count; k++) {
            br_printf("  addr6 ");
            print_ip(&info.ip6[k]);
            br_printf("\n");
        }
    }
    return 0;
}

static int ping_handler(int argc, char **argv)
{
    br_ip_t  ip;
    uint32_t count = PING_COUNT_DEFAULT;

    if (argc < 2 || argc > 3 || !br_ip_parse(argv[1], &ip) ||
        (argc == 3 && (!parse_u32(argv[2], PING_COUNT_MAX, &count) || count == 0))) {
        br_printf("usage: ping <address> [count]\n");
        return -1;
    }

    uint32_t received = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t rtt;
        br_err_t err = br_net_ping(&ip, BR_SEC(1), &rtt);
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
    uint32_t version = 0;

    if (argc < 2 || argc > 3 || (argc == 3 && (!parse_u32(argv[2], 6, &version) ||
                                               (version != 4 && version != 6)))) {
        br_printf("usage: nslookup <name> [4|6]\n");
        return -1;
    }

    br_ip_t ip;
    br_err_t err = br_net_resolve(argv[1], (int)version, &ip);
    if (err != BR_OK) {
        br_printf("cannot resolve %s: %d\n", argv[1], (int)err);
        return -1;
    }

    br_printf("%s is ", argv[1]);
    print_ip(&ip);
    br_printf("\n");
    return 0;
}

static br_shell_cmd_t nslookup_cmd = {
    .name    = "nslookup",
    .help    = "nslookup <name> [4|6]",
    .handler = nslookup_handler,
};

static br_shell_cmd_t ifconfig_cmd = {
    .name    = "ifconfig",
    .help    = "show network interfaces",
    .handler = ifconfig_handler,
};

static br_shell_cmd_t ping_cmd = {
    .name    = "ping",
    .help    = "ping <address> [count]",
    .handler = ping_handler,
};

void br_shell_register_net(void)
{
    br_shell_register(&ifconfig_cmd);
    br_shell_register(&ping_cmd);
    br_shell_register(&nslookup_cmd);
}
