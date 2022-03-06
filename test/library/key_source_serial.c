/*
 * Copyright (C) 2022 Kamil Ignacak <acerion@wp.pl>
 *
 * Code for polling of serial port is based on "statserial - Serial Port
 * Status Utility" (GPL2+).
 * Copyright (C) 1994 Jeff Tranter (Jeff_Tranter@Mitel.COM)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




#define _GNU_SOURCE /* strerror_r() */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>




#include "key_source_serial.h"




static const char * g_device = "/dev/ttyS0";




bool cw_key_source_serial_open(cw_key_source_t * source)
{
	/* Open serial port. */
	errno = 0;
	int fd = open(g_device, O_RDONLY);
	if (fd == -1) {
		char buf[32] = { 0 };
		strerror_r(errno, buf, sizeof (buf));
		fprintf(stderr, "[EE]: open(%s): %s\n", g_device, buf);
		return false;
	}
	source->source_reference = (uintptr_t) fd;

	return true;
}




void cw_key_source_serial_close(cw_key_source_t * source)
{
	int fd = (int) source->source_reference;
	close(fd);
}




bool cw_key_source_serial_poll_once(cw_key_source_t * source, bool * key_is_down, bool * ptt_is_on)
{
	int fd = (int) source->source_reference;
	errno = 0;
	unsigned int value = 0;
	int status = ioctl(fd, TIOCMGET, &value);
	if (status != 0) {
		char buf[32] = { 0 };
		strerror_r(errno, buf, sizeof (buf));
		fprintf(stderr, "[EE]: ioctl(TIOCMGET): %s\n", buf);
		return false;
	}
	const unsigned int keying_pin = source->param_keying; /* E.g. TIOCM_DTR. */
	const unsigned int ptt_pin    = source->param_ptt;    /* E.g. TIOCM_RTS. */
	*key_is_down = !!(value & keying_pin);
	*ptt_is_on   = !!(value & ptt_pin);
	return true;
}


