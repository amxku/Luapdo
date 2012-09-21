/*
 *  Author: dbgger
 *
 */

#include <stdio.h>
#include "sqlite3.h"
#include "pdo_internal.h"

void * pdo_sqlite3_native(pdo_handle_t *handle)
{
	return (void *)handle;
}

pdo_handle_t *pdo_sqlite3_open(const char *params, int * errnumber, const char * error, size_t errlen)
{
	sqlite3	*conn = NULL;
	int sqlres;
    if (!params)
        return NULL;
	sqlres = sqlite3_open(params, &conn);
	if (sqlres != SQLITE_OK) {
        if (error) {
			if (errnumber) {
				*errnumber = sqlite3_errcode(conn);
			}
			strncpy(error, sqlite3_errmsg(conn), errlen);
        }
        sqlite3_close(conn);
        return NULL;
    }
	return (pdo_handle_t *)conn;
}

pdo_status_t pdo_sqlite3_check_conn(pdo_handle_t *handle)
{
	return (handle != NULL) ? PDO_SUCCESS : PDO_EGENERAL;
}


pdo_status_t pdo_sqlite3_close(pdo_handle_t *handle)
{
	return sqlite3_close(handle);
}

pdo_status_t pdo_sqlite3_select_db(pdo_handle_t *handle, const char *name)
{
	return PDO_ENOTEMPL;
}

int pdo_sqlite3_query(pdo_handle_t *handle,                                  /* An open database */
	int *nrows, 
	const char *statement,                           /* SQL to be evaluated */
	pdo_callback callback,  /* Callback function */
	void *arg)
{
	int	ret;
	ret = sqlite3_exec(handle, statement, callback, arg, NULL);

	if (nrows)
	{
		*nrows = sqlite3_changes(handle);
	}
	return ret;
}

int pdo_sqlite3_errno(pdo_handle_t *handle)
{
	return sqlite3_errcode(handle);
}

pdo_status_t pdo_sqlite3_error(pdo_handle_t *handle, const char * error, size_t errlen)
{
	strncpy(error, sqlite3_errmsg(handle), errlen);
	return PDO_SUCCESS;
}

const char *pdo_sqlite3_escape(pdo_handle_t *handle, const char *string)
{
	char *ret = NULL;
	char *sql = sqlite3_mprintf("%q", string);
	ret = strdup(sql);
	sqlite3_free(sql);

	return ret;
}

const struct pdo_driver_t pdo_sqlite3_driver = {
	"sqlite3",
	pdo_sqlite3_native,
	pdo_sqlite3_open,
	pdo_sqlite3_check_conn,
	pdo_sqlite3_close,
	pdo_sqlite3_select_db,
	pdo_sqlite3_query,
	pdo_sqlite3_errno,
	pdo_sqlite3_error,
	pdo_sqlite3_escape,
};


