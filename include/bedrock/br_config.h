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
 * See LICENSE-GPL-3.0.md for the full terms and runtime exception.
 */

#ifndef BR_CONFIG_H
#define BR_CONFIG_H

/* Default configuration values — overridable via Kconfig / build flags */

#ifndef CONFIG_MAX_TASKS
#define CONFIG_MAX_TASKS        16
#endif

#ifndef CONFIG_NUM_PRIORITIES
#define CONFIG_NUM_PRIORITIES   8
#endif

#ifndef CONFIG_DEFAULT_STACK_SIZE
#define CONFIG_DEFAULT_STACK_SIZE 1024
#endif

#ifndef CONFIG_TICKLESS
#define CONFIG_TICKLESS         1
#endif

#ifndef CONFIG_RR_TIME_SLICE_US
#define CONFIG_RR_TIME_SLICE_US 10000  /* 10ms default time slice */
#endif

#ifndef CONFIG_ASSERT
#define CONFIG_ASSERT           1      /* 1 = br_assert() checks enabled */
#endif

#endif /* BR_CONFIG_H */
