/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *                      and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2013 Kamil Ignacak <acerion@wp.pl>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#endif

#include "cwdaemon.h"

/*
 * null port functions... these do, well, nothing, except for provide
 * a convienient placeholder.
 */

/*
 * dev_is_null(name): check to see whether 'name' is a null type device.
 */
int
dev_is_null(const char *fname)
{
       if (strcmp(fname, "null") != 0)
               return (-1);
       return (0);
}

int
null_init (cwdevice * dev, int fd)
{
       dev->fd = fd;
       return 0;
}

int
null_free (cwdevice * dev)
{
       return 0;
}

int
null_reset (cwdevice * dev)
{
       return 0;
}

int
null_cw (cwdevice * dev, int onoff)
{
       return 0;
}

int
null_ptt (cwdevice * dev, int onoff)
{
       return 0;
}
