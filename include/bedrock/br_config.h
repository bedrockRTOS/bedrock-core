/*
 * Project: bedrock[RTOS]
 * Version: 0.0.3
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: GPL-3.0-only WITH runtime exception
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3.
 * Applications that link against or run on bedrock[RTOS] are NOT required
 * to be GPL-licensed — only changes to bedrock[RTOS] itself must remain GPL.
 * See LICENSE.md for the full terms and runtime exception.
 */

#ifndef BR_CONFIG_H
#define BR_CONFIG_H

/*
 * When the Kconfig toolchain is in use, run `chorus configure` (or
 * `chorus defconfig`) first.  That generates include/generated/autoconf.h
 * from the active .config.  The #ifndef guards below act as fallback
 * defaults when building without the Kconfig toolchain.
 */
#ifdef __has_include
#  if __has_include("generated/autoconf.h")
#    include "generated/autoconf.h"
#  endif
#endif

#ifndef CONFIG_MAX_TASKS
#  define CONFIG_MAX_TASKS          16
#endif

#ifndef CONFIG_NUM_PRIORITIES
#  define CONFIG_NUM_PRIORITIES     8
#endif

#ifndef CONFIG_DEFAULT_STACK_SIZE
#  define CONFIG_DEFAULT_STACK_SIZE 1024
#endif

#ifndef CONFIG_TICKLESS
#  define CONFIG_TICKLESS           1
#endif

#ifndef CONFIG_RR_TIME_SLICE_US
#  define CONFIG_RR_TIME_SLICE_US   10000
#endif

#ifndef CONFIG_ASSERT
#  define CONFIG_ASSERT             1
#endif

/* Map Kconfig SYS_CLOCK_HZ to the name used by the HAL timer implementation */
#ifndef BR_HAL_SYS_CLOCK_HZ
#  ifdef CONFIG_SYS_CLOCK_HZ
#    define BR_HAL_SYS_CLOCK_HZ    CONFIG_SYS_CLOCK_HZ
#  else
#    define BR_HAL_SYS_CLOCK_HZ    16000000
#  endif
#endif

#endif /* BR_CONFIG_H */
