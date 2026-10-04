/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UF2_MAGIC_START0  0x0A324655UL
#define UF2_MAGIC_START1  0x9E5D5157UL
#define UF2_MAGIC_END     0x0AB16F30UL
#define UF2_FLAG_FAMILY   0x00002000UL
#define UF2_PAYLOAD       256

typedef struct {
    uint32_t magic_start0;
    uint32_t magic_start1;
    uint32_t flags;
    uint32_t target_addr;
    uint32_t payload_size;
    uint32_t block_no;
    uint32_t num_blocks;
    uint32_t family_id;
    uint8_t  data[476];
    uint32_t magic_end;
} uf2_block_t;

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: %s <in.bin> <out.uf2> <base-addr> <family-id>\n", argv[0]);
        return 1;
    }

    uint32_t base = (uint32_t)strtoul(argv[3], NULL, 0);
    uint32_t family = (uint32_t)strtoul(argv[4], NULL, 0);

    FILE *in = fopen(argv[1], "rb");
    if (in == NULL) {
        perror(argv[1]);
        return 1;
    }
    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);

    FILE *out = fopen(argv[2], "wb");
    if (out == NULL) {
        perror(argv[2]);
        fclose(in);
        return 1;
    }

    uint32_t num_blocks = (uint32_t)((size + UF2_PAYLOAD - 1) / UF2_PAYLOAD);

    for (uint32_t i = 0; i < num_blocks; i++) {
        uf2_block_t block;
        memset(&block, 0, sizeof(block));
        block.magic_start0 = UF2_MAGIC_START0;
        block.magic_start1 = UF2_MAGIC_START1;
        block.flags        = UF2_FLAG_FAMILY;
        block.target_addr  = base + i * UF2_PAYLOAD;
        block.payload_size = UF2_PAYLOAD;
        block.block_no     = i;
        block.num_blocks   = num_blocks;
        block.family_id    = family;
        block.magic_end    = UF2_MAGIC_END;

        if (fread(block.data, 1, UF2_PAYLOAD, in) == 0) {
            break;
        }
        if (fwrite(&block, sizeof(block), 1, out) != 1) {
            perror(argv[2]);
            fclose(in);
            fclose(out);
            return 1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}
