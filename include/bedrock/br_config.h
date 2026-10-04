/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_CONFIG_H
#define BR_CONFIG_H

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

#ifndef CONFIG_UART_RX_BUF_SIZE
#  define CONFIG_UART_RX_BUF_SIZE   64
#endif

#ifndef CONFIG_SHELL_LINE_MAX
#  define CONFIG_SHELL_LINE_MAX     64
#endif

#ifndef CONFIG_SHELL_MAX_ARGS
#  define CONFIG_SHELL_MAX_ARGS     8
#endif

#ifndef BR_HAL_SYS_CLOCK_HZ
#  ifdef CONFIG_SYS_CLOCK_HZ
#    define BR_HAL_SYS_CLOCK_HZ    CONFIG_SYS_CLOCK_HZ
#  else
#    define BR_HAL_SYS_CLOCK_HZ    16000000
#  endif
#endif

#endif /* BR_CONFIG_H */
