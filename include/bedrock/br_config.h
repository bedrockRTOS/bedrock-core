/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: BSD 3-Clause
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

#endif /* BR_CONFIG_H */
