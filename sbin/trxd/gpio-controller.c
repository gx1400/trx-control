/*
 * Copyright (c) 2023 Marc Balmer HB9SSB
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Control GPIO pins on the host machine */

#include <sys/ioctl.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "pathnames.h"
#include "trxd.h"

extern int luaopen_trxd(lua_State *);
extern int luaopen_json(lua_State *);

extern trx_controller_tag_t *trx_controller_tag;
extern int verbose;

void *
gpio_controller(void *arg)
{
	gpio_controller_tag_t *tag = (gpio_controller_tag_t *)arg;
	lua_State *L;
	int fd, n, driver_ref;
	struct stat sb;
	char trx_driver[PATH_MAX];
	pthread_t gpio_handler_thread;

	L = NULL;
	if (pthread_detach(pthread_self()))
		err(1, "gpio-controller: pthread_detach");
	if (verbose)
		printf("gpio-controller: initialising gpio\n");

	/*
	 * Lock this transceivers mutex, so that no other thread accesses
	 * while we are initialising.
	 */
	if (pthread_mutex_lock(&tag->mutex))
		err(1, "gpio-controller: pthread_mutex_lock");
	if (verbose > 1)
		printf("gpio-controller: mutex locked\n");

	if (pthread_mutex_lock(&tag->mutex2))
		err(1, "gpio-controller: pthread_mutex_lock");
	if (verbose > 1)
		printf("gpio-controller: mutex2 locked\n");

	/* Setup Lua */
	L = luaL_newstate();
	if (L == NULL)
		err(1, "gpio-controller: luaL_newstate");

	luaL_openlibs(L);

	luaopen_trxd(L);
	lua_setglobal(L, "trxd");
	luaopen_json(L);
	lua_setglobal(L, "json");

	tag->is_running = 1;

	/*
	 * We are ready to go, unlock the mutex, so that client-handlers,
	 * trx-handlers, and, try-pollers can access it.
	 */

	if (verbose)
		printf("gpio-controller: ready to control gpio\n");

	if (pthread_mutex_unlock(&tag->mutex))
		err(1, "gpio-controller: pthread_mutex_unlock");
	if (verbose > 1)
		printf("gpio-controller: mutex unlocked\n");

	while (1) {
		int nargs = 1;

		/* Wait on cond, this releases the mutex */
		if (verbose > 1)
			printf("gpio-controller: wait for cond1\n");
		while (tag->handler == NULL) {
			if (pthread_cond_wait(&tag->cond1, &tag->mutex2))
				err(1, "gpio-controller: pthread_cond_wait");
			if (verbose > 1)
				printf("gpio-controller: cond1 changed\n");
		}

		if (verbose > 1) {
			printf("gpio-controller: request for %s", tag->handler);
			if (tag->data)
				printf(" with data '%s'\n", tag->data);
			printf("\n");
		}

		lua_pushstring(L, tag->data);

		switch (lua_pcall(L, 1, 1, 0)) {
		case LUA_OK:
			break;
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			syslog(LOG_ERR, "Lua error: %s",
				lua_tostring(L, -1));
			break;
		}
		if (lua_type(L, -1) == LUA_TSTRING)
			tag->reply = (char *)lua_tostring(L, -1);
		else
			tag->reply = "";

		lua_pop(L, 2);
		tag->handler = NULL;

		if (pthread_cond_signal(&tag->cond2))
			err(1, "gpio-controller: pthread_cond_signal");
		if (verbose > 1)
			printf("gpio-controller: cond2 signaled\n");
	}
	lua_close(L);
	return NULL;
}