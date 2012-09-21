#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <objbase.h>
#pragma comment(lib, "lua5.1.lib")
#pragma comment(lib, "libmysql.lib")
#pragma comment(lib, "ole32.lib")
#include "pdo_internal.h"

#define FUNC(name)		static int name (lua_State * L)
#define checkstr(L, narg)	( luaL_checklstring((L), (narg), 0) )
#define checkdb(L, narg)	((struct pdo_t *)checknumber((L), (narg)))
#define checknumber(L, narg)	( luaL_checkint((L), (narg)) )

#ifndef _countof
	#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif

#define pthread_mutex_init(a,b) (InitializeCriticalSection((a)),0)
#define pthread_mutex_destroy(a) (DeleteCriticalSection((a)),0)
#define pthread_mutex_lock(a) (EnterCriticalSection(a),0)
#define pthread_mutex_unlock(a) (LeaveCriticalSection(a),0)

#define pdo_mutex_create(pdo_)	pthread_mutex_init(&pdo_->mutex)
#define pdo_mutex_lock(pdo_)	pthread_mutex_lock(&pdo_->mutex)
#define pdo_mutex_unlock(pdo_)	pthread_mutex_unlock(&pdo_->mutex)
#define pdo_mutex_destroy(pdo_)	pthread_mutex_destroy(&pdo_->mutex)

typedef struct { char * name; int (*func)(lua_State *); } f_entry;
typedef struct { char * name; int value; } d_entry;

extern struct pdo_driver_t pdo_sqlite3_driver;
extern struct pdo_driver_t pdo_mysql_driver;
extern struct pdo_driver_t pdo_ado_driver;


struct pdo_driver_t	* pdo_drivers[] = {
	&pdo_sqlite3_driver,
	&pdo_mysql_driver,
#if defined(_WIN32)
	&pdo_ado_driver,
#endif
};

/* nozero to break */
static int pop_break_condition(lua_State * L)
{
	int result;
	
	if (lua_isnil(L, -1))
		result = 0;
	else if (lua_isboolean(L, -1))
		result = lua_toboolean(L, -1);
	else if (lua_isnumber(L, -1))
		result = (int)lua_tonumber(L, -1);
	else
		result = 1;
	
	lua_pop(L, 1);

	return result;
}


static int checknilornoneorfunc(lua_State * L, int narg)
{
	if (lua_isnil(L, narg) || lua_isnone(L, narg))
		return 0;
	if (lua_isfunction(L, narg))
		return 1;
	luaL_typerror(L, narg, "nil, none or function");
	return 0; /* never reached, make compiler happy... */
}


static int exec_callback_wrapper(void * cb_data, int num_columns, char ** values, char ** names)
{
	int index;
	lua_State * L = (lua_State *) cb_data;
	
	lua_pushvalue(L, 3);	/* Callback function, resulting stack position 4 */
	lua_newtable(L);	/* Value array, resulting stack position 5 */
	lua_newtable(L);	/* Names array, resulting stack position 6 */
	
	for (index=0; index < num_columns; index++) {
		/* values[index] maybe NULL */
		lua_pushstring(L, values[index]);	/* Value */
		lua_rawseti(L, 5, index+1);		/* C-index are 0 based, Lua index are 1 based... */
		
		lua_pushstring(L, names[index]);	/* Name */
		lua_rawseti(L, 6, index+1);
	}
	
	/* In: 2 arrays, Out: result code, On error: throw */
	lua_call(L, 2, 1);
	
	return pop_break_condition(L);
}


FUNC( l_pdo_query )
{
	struct pdo_t	*pdo = checkdb(L, 1);
	const char *statement = checkstr(L, 2);
	pdo_callback cb;
	void * cb_data;
	int nrows = 0;
	
	if (checknilornoneorfunc(L, 3)) {
		cb = exec_callback_wrapper;
		cb_data = L;
	} else {
		cb = 0;
		cb_data = 0;
	}

	pdo_mutex_lock(pdo);
	
	lua_pushnumber(L, pdo->driver->query(pdo->handle, &nrows, statement, cb, cb_data));
	lua_pushnumber(L, nrows);

	pdo_mutex_unlock(pdo);

	return 2;
}

FUNC( l_pdo_open )
{
	const char *name = checkstr(L, 1);
	const char *params = checkstr(L, 2);
	int	i = 0;
	int errnumber = 0;
	int	count = _countof(pdo_drivers);
	struct pdo_t *pdo = (struct pdo_t *)malloc(sizeof(struct pdo_t));

	if (!pdo) {
		return 0;
	}

	pdo->handle = NULL;
	pdo->driver = NULL;

	memset(pdo, 0, sizeof(struct pdo_t));

	for (i = 0; i < count; i++) {
	
		if (0 == strcmpi(pdo_drivers[i]->name, name)) {
			pdo->driver = pdo_drivers[i];
			pdo->handle = pdo->driver->open(params, &errnumber, pdo->errmsg, sizeof(pdo->errmsg));
			break;
		}
	}

	if (!pdo->handle) {
		lua_pushnil(L);
		lua_pushnumber(L, errnumber);
		lua_pushstring(L, pdo->errmsg);
		free(pdo);
		pdo = NULL;
	} else {
		pdo_mutex_create(pdo);
		lua_pushnumber(L, (int)pdo);
		lua_pushnumber(L, errnumber);
		lua_pushstring(L, pdo->errmsg);
	}

	return 3;
}

FUNC( l_pdo_close )
{
	struct pdo_t *pdo = checkdb(L, 1);
	if (pdo) {

		pdo_mutex_lock(pdo);
		pdo->driver->close(pdo->handle);
		pdo_mutex_unlock(pdo);

		pdo_mutex_destroy(pdo);
		free(pdo);
	}

	return 0;
}

FUNC( l_pdo_errno )
{
	struct pdo_t *pdo = checkdb(L, 1);

	pdo_mutex_lock(pdo);

	lua_pushnumber(L, (int)pdo->driver->errnumber(pdo->handle));

	pdo_mutex_unlock(pdo);

	return 1;
}

FUNC( l_pdo_error )
{
	struct pdo_t *pdo = checkdb(L, 1);

	pdo_mutex_lock(pdo);

	pdo->driver->error(pdo->handle, pdo->errmsg, sizeof(pdo->errmsg));

	pdo_mutex_unlock(pdo);

	lua_pushstring(L, pdo->errmsg);
	return 1;
}

FUNC( l_pdo_check_conn )
{
	struct pdo_t *pdo = checkdb(L, 1);
	
	pdo_mutex_lock(pdo);
	
	lua_pushboolean(L, pdo->driver->check_conn(pdo->handle) == PDO_SUCCESS);
	
	pdo_mutex_unlock(pdo);
	
	return 1;
}



FUNC( l_pdo_escape )
{
	struct pdo_t *pdo = checkdb(L, 1);
	const char *string = NULL;
	const char *escape = NULL;

	// 传递进NIL,checkstr不能检测???
	if (lua_isnil(L, 2)) {
		lua_pushstring(L, "");
		return 1;
	}

	string = checkstr(L, 2);

	escape = pdo->driver->escape(pdo->handle, string);

	if (escape)
	{
		lua_pushstring(L, escape);

		/* free resource */
		free((void *)escape);
	}
	else
	{
		lua_pushnil(L);
	}

	return 1;
}

static const struct luaL_reg pdo_apis_entries [] = {
	{ "open",	l_pdo_open },
	{ "query",	l_pdo_query },
	{ "close",	l_pdo_close },
	{ "errno",	l_pdo_errno },
	{ "error",	l_pdo_error },
	{ "escape",	l_pdo_escape },
	{ "check_conn",	l_pdo_check_conn },	
	{ 0, 0 }
};

d_entry pdo_consts[] = {
	{ "ENOTEMPL",	PDO_ENOTEMPL},
	{ "SUCCESS",	PDO_SUCCESS},
	{ "EGENERAL",	PDO_EGENERAL},
	{ 0, 0 }
};

__declspec(dllexport) int luaopen_pdo(lua_State * L)
{
	int i;

	/* ado needed */
	CoInitialize(NULL);

	luaL_openlib(L, "pdo", pdo_apis_entries, 0);

	for( i = 0; pdo_consts[i].name != NULL; i++) {
	  lua_pushstring( L, pdo_consts[i].name);
	  lua_pushnumber( L, pdo_consts[i].value);
	  lua_settable( L, -3);
	}
	lua_pop( L, 1);
  
	return 0;	/* api, error codes, type codes, auth requests */
}
