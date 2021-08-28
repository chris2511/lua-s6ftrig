/*
 * Copyright (c) 2020 Christian Hohnstädt
 * SPDX-License-Identifier: MPL-2.0
 */

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <errno.h>
#include <string.h>
#include <sys/param.h>

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>

#include <s6/ftrigr.h>
#include <s6/ftrigw.h>
#include <s6/s6-supervise.h>

#define S6FTRIG_META "__s6_ftrig_meta"
#define S6FTRIG_CLASS "__s6_ftrig_class"
#define S6FTRIG_USERDATA "__s6_ftrig_userdata"
#define SLASH_EVENTDIR "/" S6_SUPERVISE_EVENTDIR

static int ftrig_subscribe_lua(lua_State *L, ftrigr_t *t)
{
	uint16_t id;
	const char *path;
	char event[MAXPATHLEN];
	size_t len;
	int n = lua_tonumber(L, -2);

	if (lua_isnumber(L, -1) || !lua_isstring(L, -1))
		return luaL_error(L, "Invalid ftrigr path @%d", n);

	path = lua_tolstring(L, -1, &len);
	if (len + sizeof SLASH_EVENTDIR > sizeof event)
		return luaL_error(L, "ftrigr path %d too long: %d", n, len);
	memcpy(event, path, len);
	memcpy(event + len, SLASH_EVENTDIR, sizeof SLASH_EVENTDIR);

	if (!ftrigw_fifodir_make(event, getgid(), 0))
		return luaL_error(L, "ftrigw_fifodir_make(%s): %s",
					path, strerror(errno));

	id = ftrigr_subscribe(t, event, ".", FTRIGR_REPEAT, 0, 0);
	if (id == 0)
		return luaL_error(L, "ftrigr_subscribe(%s @%d): %s", path, n,
					strerror(errno));
	return id;
}

/* Create new ftrig class */
static int ftrig_init(lua_State *L)
{
	int n, svc_table_idx;
	ftrigr_t *t;

	if (!lua_istable(L, 1))
		luaL_error(L, "Expected set of services as argument");

	t = lua_newuserdata(L, sizeof *t);
	luaL_setmetatable(L, S6FTRIG_META);
	{
		ftrigr_t _t = FTRIGR_ZERO;
		*t = _t;
	}

	lua_newtable(L); // 3: Return value
	svc_table_idx = lua_gettop(L);

	ftrigr_startf(t, 0, 0);

	lua_pushnil(L);  /* first key */
	for (n=0; lua_next(L, 1); n++ ) {
		uint16_t id = ftrig_subscribe_lua(L, t);

		lua_pushinteger(L, id);
		lua_pushvalue(L, -2);
		lua_settable(L, svc_table_idx);
		// Drop value, prepare for next iteration
		lua_pop(L, 1);
        }
	lua_newtable(L);
	lua_pushliteral(L, S6FTRIG_USERDATA);
	lua_pushvalue(L, 2);
	lua_settable(L, -3);

	lua_pushliteral(L, "watches");
	lua_pushvalue(L, svc_table_idx);
	lua_settable(L, -3);

	luaL_setmetatable(L, S6FTRIG_CLASS);

	return 1;
}

/* Translate state character to state name */
static int ftrig_state(lua_State* L)
{
	const char *what = lua_tostring(L, 1);

	if (!what)
		return 0;
	switch (*what) {
		case 'd': lua_pushliteral(L, "finish"); break;
		case 'D': lua_pushliteral(L, "down"); break;
		case 'u': lua_pushliteral(L, "up"); break;
		case 'U': lua_pushliteral(L, "ready"); break;
		case 's': lua_pushliteral(L, "start"); break;
		case 'O': lua_pushliteral(L, "once"); break;
		case 'x': lua_pushliteral(L, "exit"); break;
		default:  lua_pushnil(L); break;
	}
	return 1;
}

/* Return the current state of each service */
static int ftrig_current(lua_State *L)
{
	static const char *states = "dDuU";
	lua_pushliteral(L, "watches");
	lua_gettable(L, 1); // 2: Watches
	lua_newtable(L);    // 3: Result table

	lua_pushnil(L); /* first key */
	while (lua_next(L, 2)) {
		int state;
		s6_svstatus_t s;

		if (!s6_svstatus_read(lua_tostring(L, -1), &s)) {
			lua_pop(L, 1);
			continue;
		}
		state = (s.pid && !s.flagfinishing) ? 2 : 0;
		state += s.flagready;
		// result[watches[n]] = states[state]
		lua_pushlstring(L, states +state, 1);
		lua_settable(L, 3);
	}
	return 1;
}

/* Read next ftrig event and return event table */
static int ftrig_wait(lua_State *L)
{
	ftrigr_t *t;
	size_t i = 0;
	stralloc string = STRALLOC_ZERO;

	lua_pushliteral(L, "watches");
	lua_gettable(L, 1); // 2: Watches
	lua_pushliteral(L, S6FTRIG_USERDATA);
	lua_gettable(L, 1); // 3: User data
	lua_newtable(L);    // 4: Result table

	t = lua_touserdata(L, 3);
	if (!t)
		luaL_error(L, "No userdata: " S6FTRIG_USERDATA);

	ftrigr_updateb(t);

	for (i=0; i < genalloc_len(uint16_t, &t->list); i++) {
		int id = genalloc_s(uint16_t, &t->list)[t->head +i];
		string.len = 0;
		if (ftrigr_checksa(t, id, &string) > 0) {
			/* result[watches[id]] = string */
			lua_pushinteger(L, id);
			lua_gettable(L, 2);
			lua_pushlstring(L, string.s, string.len);
			lua_settable(L, 4);
		}
	}
	return 1;
}

/* Returns the file descriptor to poll() */
static int ftrig_fd(lua_State *L)
{
	lua_pushliteral(L, S6FTRIG_USERDATA);
	lua_gettable(L, 1);

	ftrigr_t *t = lua_touserdata(L, -1);
	if (!t)
		return 0;
	lua_pushinteger(L, ftrigr_fd(t));
	return 1;
}

/* Garbage collector for the "ftrigr_t" userdata */
static int userdata_gc(lua_State *L)
{
	ftrigr_t *t = lua_touserdata(L, -1);
	if (t)
		ftrigr_end(t);
	return 0;
}

/* ftrig functions */
static const struct luaL_Reg s6ftrig_functions[] = {
	{ "init", ftrig_init },
	{ "state", ftrig_state },
	{ NULL, NULL }
};

/* ftrig class methods */
static const struct luaL_Reg s6ftrig_class[] = {
	{ "fd", ftrig_fd },
	{ "wait", ftrig_wait },
	{ "current", ftrig_current },
	{ NULL, NULL }
};

int luaopen_s6ftrig(lua_State *L)
{
	if (luaL_newmetatable(L, S6FTRIG_META)) {
		lua_pushliteral(L, "__gc"); \
		lua_pushcfunction(L, userdata_gc); \
		lua_settable(L, -3);
	}
	if (luaL_newmetatable(L, S6FTRIG_CLASS)) {
		lua_pushliteral(L, "__index"); \
		luaL_newlib(L, s6ftrig_class);
		lua_settable(L, -3);
	}
	lua_pop(L,1);
	luaL_newlib(L, s6ftrig_functions);
	return 1;
}
