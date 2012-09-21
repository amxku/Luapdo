#if defined(_WIN32)
#include <windows.h>
#else
#include "windef.h"
#endif
#include <mysql/mysql.h>
#include "pdo_internal.h"

size_t strnlen(const char *s, size_t max)
{
	register const char *p = s;
#ifdef __BCC__
	/* bcc can optimize the counter if it thinks it is a pointer... */
	register const char *maxp = (const char *) max;
#else
# define maxp max
#endif
	
	while (maxp && *p) {
		++p;
		--maxp;
	}
	
	return p - s;
}

char *
strndup (char const *s, size_t n)
{
  size_t len = strnlen (s, n);
  char *new = malloc (len + 1);

  if (new == NULL)
    return NULL;

  new[len] = '\0';
  return memcpy (new, s, len);
}


void * pdo_mysql_native(pdo_handle_t *handle)
{
	return (void *)handle;
}

pdo_handle_t *pdo_mysql_open(const char *params, int * errnumber, const char * error, size_t errlen)
{
	const char *ptr;
	const char *key;
	size_t klen;
    const char *value;
    size_t vlen;
	int i;
	static const char *const delims = " \r\n\t;|,";
	unsigned int port = 0;
	MYSQL *real_conn = NULL;
	MYSQL *conn = NULL;
    unsigned long flags = 0;

#if MYSQL_VERSION_ID >= 50013
    my_bool do_reconnect = 1;
#endif

	struct {
	   const char *field;
	   const char *value;
	} fields[] = {
	   {"host", NULL},
	   {"user", NULL},
	   {"pass", NULL},
	   {"dbname", NULL},
	   {"port", NULL},
	   {"sock", NULL},
	   {"flags", NULL},
	   {"group", NULL},
	   {"reconnect", NULL},
	   {"connecttimeout", NULL},
	   {"readtimeout", NULL},
	   {"writetimeout", NULL},
	   {NULL, NULL}
	};


	conn = mysql_init(NULL);

	if (!conn) {
		return NULL;
	}

	mysql_options(conn, MYSQL_OPT_COMPRESS, NULL);

	for (ptr = strchr(params, '='); ptr; ptr = strchr(ptr, '=')) {
        /* don't dereference memory that may not belong to us */
        if (ptr == params) {
            ++ptr;
            continue;
        }
        for (key = ptr-1; isspace(*key); --key);
        klen = 0;
        while (isalpha(*key)) {
            /* don't parse backwards off the start of the string */
            if (key == params) {
                --key;
                ++klen;
                break;
            }
            --key;
            ++klen;
        }
        ++key;
        for (value = ptr+1; isspace(*value); ++value);
        vlen = strcspn(value, delims);
        for (i = 0; fields[i].field != NULL; i++) {
            if (!strnicmp(fields[i].field, key, klen)) {
                fields[i].value = strndup(value, vlen);
                break;
            }
        }
        ptr = value+vlen;
    }

	if (fields[4].value != NULL) {
        port = atoi(fields[4].value);
    }
    if (fields[6].value != NULL &&
        !strcmp(fields[6].value, "CLIENT_FOUND_ROWS")) {
        flags |= CLIENT_FOUND_ROWS; /* only option we know */
    }
    if (fields[7].value != NULL) {
		mysql_options(conn, MYSQL_READ_DEFAULT_GROUP, fields[7].value);
    }
#if MYSQL_VERSION_ID >= 50013
    if (fields[8].value != NULL) {
		do_reconnect = atoi(fields[8].value) ? 1 : 0;
    }
#endif

    if (fields[9].value != NULL) {
		int	timeout = atoi(fields[9].value);
		mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    }

    if (fields[10].value != NULL) {
		int	timeout = atoi(fields[10].value);
		mysql_options(conn, MYSQL_OPT_READ_TIMEOUT, &timeout);
    }

	if (fields[11].value != NULL) {
		int	timeout = atoi(fields[11].value);
		mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    }
	real_conn = mysql_real_connect(conn, fields[0].value,
		fields[1].value, fields[2].value,
		fields[3].value, port, fields[5].value, flags);

	/* free resource */
	for (i = 0; fields[i].field != NULL; i++) {
		free(fields[i].value);
    }

	if (errnumber) {
		*errnumber = mysql_errno(conn);
	}

    if(NULL == real_conn) {
        if (error) {
			strncpy(error, mysql_error(conn), errlen);
        }
        mysql_close(conn);
        return NULL;
    }

#if MYSQL_VERSION_ID >= 50013
    /* Some say this should be AFTER mysql_real_connect */
    mysql_options(real_conn, MYSQL_OPT_RECONNECT, &do_reconnect);
#endif

	return (pdo_handle_t *)real_conn;

}

pdo_status_t pdo_mysql_check_conn(pdo_handle_t *handle)
{
	return (mysql_ping(handle) ? PDO_EGENERAL : PDO_SUCCESS);
}


pdo_status_t pdo_mysql_close(pdo_handle_t *handle)
{
	mysql_close(handle);
	return PDO_SUCCESS;
}

pdo_status_t pdo_mysql_select_db(pdo_handle_t *handle, const char *name)
{
	return mysql_select_db(handle, name);
}

int pdo_mysql_query(pdo_handle_t *handle,                                  /* An open database */
	int *nrows, 
	const char *statement,                           /* SQL to be evaluated */
	pdo_callback callback,  /* Callback function */
	void *arg)
{
	int ret;
	MYSQL_RES *result = NULL;
	ret = mysql_query(handle, statement);
	if (ret != 0) {
        ret = mysql_errno(handle);
    }
	if (nrows) {
		*nrows = (int) mysql_affected_rows(handle);
	}

	/* 不管如何，取出结果集，防止2014错误 */
	result = mysql_store_result(handle);

	if (result) {
		if (callback) {
			MYSQL_ROW values = NULL;
			int i;
			MYSQL_FIELD *fields;
			int num_columns;
			char **names = NULL;

			fields = mysql_fetch_fields(result);
			num_columns = mysql_field_count(handle);
			names = malloc(sizeof(char *) * num_columns);

			if (names) {
				for (i = 0; i < num_columns; i++) {
					names[i] = fields[i].name;
				}

				while ((values = mysql_fetch_row(result)) != NULL) {
					/* nozero to break */
					if (callback(arg, num_columns, values, names))
					{
						break;
					}
				}
				free(names);
			}
		}

		mysql_free_result(result);			
	}


	return ret;
}

int pdo_mysql_errno(pdo_handle_t *handle)
{
	return mysql_errno(handle);
}

pdo_status_t pdo_mysql_error(pdo_handle_t *handle, const char * error, size_t errlen)
{
	strncpy(error, mysql_error(handle), errlen);
	return PDO_SUCCESS;
}

const char *pdo_mysql_escape(pdo_handle_t *handle, const char *string)
{
	unsigned long len = strlen(string);
    char *ret = malloc(2*len + 1);
    mysql_real_escape_string(handle, ret, string, len);
	return ret;
}

const struct pdo_driver_t pdo_mysql_driver = {
	"mysql",
	pdo_mysql_native,
	pdo_mysql_open,
	pdo_mysql_check_conn,
	pdo_mysql_close,
	pdo_mysql_select_db,
	pdo_mysql_query,
	pdo_mysql_errno,
	pdo_mysql_error,
	pdo_mysql_escape,
};


