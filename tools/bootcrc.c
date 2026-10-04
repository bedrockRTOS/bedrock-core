/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BOOT2_SIZE     256
#define BOOT2_PAYLOAD  252

static uint32_t crc32_msb(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i] << 24;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80000000UL) ? (crc << 1) ^ 0x04C11DB7UL : crc << 1;
        }
    }
    return crc;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <boot2.bin> <boot2.S>\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "rb");
    if (in == NULL) {
        perror(argv[1]);
        return 1;
    }

    uint8_t buf[BOOT2_SIZE];
    memset(buf, 0, sizeof(buf));
    size_t len = fread(buf, 1, BOOT2_SIZE, in);
    int extra = fgetc(in);
    fclose(in);

    if (len > BOOT2_PAYLOAD || extra != EOF) {
        fprintf(stderr, "%s: boot2 is larger than %d bytes\n", argv[1], BOOT2_PAYLOAD);
        return 1;
    }

    uint32_t crc = crc32_msb(buf, BOOT2_PAYLOAD);
    buf[252] = (uint8_t)crc;
    buf[253] = (uint8_t)(crc >> 8);
    buf[254] = (uint8_t)(crc >> 16);
    buf[255] = (uint8_t)(crc >> 24);

    FILE *out = fopen(argv[2], "w");
    if (out == NULL) {
        perror(argv[2]);
        return 1;
    }

    fprintf(out, ".section .boot2, \"ax\"\n");
    for (int i = 0; i < BOOT2_SIZE; i++) {
        fprintf(out, "%s0x%02x", (i % 16 == 0) ? ".byte " : ", ", buf[i]);
        if (i % 16 == 15) {
            fprintf(out, "\n");
        }
    }
    fclose(out);
    return 0;
}
