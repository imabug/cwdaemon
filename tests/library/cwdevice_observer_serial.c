/*
 * This file is a part of cwdaemon project.
 *
 * Copyright (C) 2022 - 2023 Kamil Ignacak <acerion@wp.pl>
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




/**
   @file Code that observes cwdaemon's tty cwdevice

   Code for polling of serial port is based on "statserial" program (see
   copyright notice above).
*/




// TODO (acerion) 2022.12.17: GNU's strerror_r() may not fill provided buffer :(
// Write own replacement of strerror_r() that bevaves as expected.
//
// I don't want to be guessing which version of strerror_r() is being
// used at compile time: XSI variant or GNU variant.
#define _GNU_SOURCE /* strerror_r() */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>




#include "cwdevice_observer_serial.h"




bool cwdevice_observer_serial_open(cwdevice_observer_t * observer)
{
	/* Open serial port. */
	errno = 0;
	int fd = open(observer->source_path, O_RDONLY);
	if (fd == -1) {
		char buf[32] = { 0 };
		char * b = strerror_r(errno, buf, sizeof (buf));
		fprintf(stderr, "[EE] open(%s): %s / %d\n", observer->source_path, b, errno);
		return false;
	}
	observer->source_reference = (uintptr_t) fd;

	return true;
}




void cwdevice_observer_serial_close(cwdevice_observer_t * observer)
{
	int fd = (int) observer->source_reference;
	close(fd);
}




bool cwdevice_observer_serial_poll_once(cwdevice_observer_t * observer, bool * key_is_down, bool * ptt_is_on)
{
	int fd = (int) observer->source_reference;
	errno = 0;
	unsigned int value = 0;
	int status = ioctl(fd, TIOCMGET, &value);
	if (status != 0) {
		char buf[32] = { 0 };
		char * b = strerror_r(errno, buf, sizeof (buf));
		fprintf(stderr, "[EE] ioctl(TIOCMGET): %s / %d\n", b, errno);
		return false;
	}
	const unsigned int keying_pin = observer->tty_pins_config.pin_keying; /* E.g. TIOCM_DTR. */
	const unsigned int ptt_pin    = observer->tty_pins_config.pin_ptt;    /* E.g. TIOCM_RTS. */
	*key_is_down = !!(value & keying_pin);
	*ptt_is_on   = !!(value & ptt_pin);
	return true;
}


