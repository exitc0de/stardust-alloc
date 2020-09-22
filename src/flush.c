/* Copyright (C) 2018, Ward Jaradat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <os/console.h>

#define ELEMENT_SIZE 100
#define BUFFER_SIZE (ELEMENT_SIZE * 10000)
static char buf[BUFFER_SIZE];
static char* bufptr = &buf[0];

extern void flush(void)
{
	char *p = &buf[0];
	while (p < bufptr)
	{
		int len = *p;
		printbytes(p + 1, len);
		p += ELEMENT_SIZE;
	}
}
