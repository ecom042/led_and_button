/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "button.h"

int main(void)
{
	printf("Button is running on %s board\n", CONFIG_BOARD_TARGET);

	button_init();

	return 0;
}
