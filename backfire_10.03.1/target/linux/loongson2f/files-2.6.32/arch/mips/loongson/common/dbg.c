/*
 * Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <dbg.h>
#include <loongson.h>

#define PROM_PRINTF_BUF_LEN 1024

void prom_printf(char *fmt, ...)
{
	static char buf[PROM_PRINTF_BUF_LEN];
	va_list args;
	char *ptr;


	va_start(args, fmt);
	vscnprintf(buf, sizeof(buf), fmt, args);

	ptr = buf;

	while (*ptr != 0) {
		if (*ptr == '\n')
			prom_putchar('\r');

		prom_putchar(*ptr++);
	}
	va_end(args);
}
