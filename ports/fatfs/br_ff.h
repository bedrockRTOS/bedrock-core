/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef BR_FF_H
#define BR_FF_H

#include "bedrock/bedrock.h"
#include "bedrock/br_blkdev.h"

br_err_t br_ff_attach(br_blkdev_t *dev);
void     br_ff_detach(void);

#endif
