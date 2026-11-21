/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>

#define BYTE_ORDER                  LITTLE_ENDIAN

#define X8_F                        "02x"
#define U16_F                       "u"
#define S16_F                       "d"
#define X16_F                       "x"
#define U32_F                       "u"
#define S32_F                       "d"
#define X32_F                       "x"
#define SZT_F                       "u"

#define PACK_STRUCT_FIELD(x)        x
#define PACK_STRUCT_STRUCT          __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_PLATFORM_DIAG(x)
#define LWIP_PLATFORM_ASSERT(x)

#endif
