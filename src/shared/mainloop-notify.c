/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2018  Intel Corporation. All rights reserved.
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/un.h>

#include "mainloop.h"
#include "mainloop-notify.h"
#include "timeout.h"

#define WATCHDOG_TRIGGER_FREQ 2

static int notify_fd = -1;

static unsigned int watchdog;

static bool watchdog_callback(void *user_data)
{
	mainloop_sd_notify("WATCHDOG=1");

	return true;
}

void mainloop_notify_init(void)
{
	const char *sock;
	struct sockaddr_un addr;
	const char *watchdog_usec;
	int msec;

	sock = getenv("NOTIFY_SOCKET");
	if (!sock)
		return;

	/* check for abstract socket or absolute path */
	if (sock[0] != '@' && sock[0] != '/')
		return;

	notify_fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (notify_fd < 0)
		return;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sock, sizeof(addr.sun_path) - 1);

	if (addr.sun_path[0] == '@')
		addr.sun_path[0] = '\0';

	if (bind(notify_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		close(notify_fd);
		notify_fd = -1;
		return;
	}

	watchdog_usec = getenv("WATCHDOG_USEC");
	if (!watchdog_usec)
		return;

	msec = atoi(watchdog_usec) / 1000;
	if (msec < 0)
		return;

	watchdog = timeout_add(msec / WATCHDOG_TRIGGER_FREQ,
				watchdog_callback, NULL, NULL);
}

void mainloop_notify_exit(void)
{
	if (notify_fd > 0) {
		close(notify_fd);
		notify_fd = -1;
	}

	timeout_remove(watchdog);
}

int mainloop_sd_notify(const char *state)
{
	int err;

	if (notify_fd <= 0)
		return -ENOTCONN;

	err = send(notify_fd, state, strlen(state), MSG_NOSIGNAL);
	if (err < 0)
		return -errno;

	return err;
}
