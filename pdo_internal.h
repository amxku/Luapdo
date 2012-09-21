
#ifndef PDO_INTERNAL_H
#define PDO_INTERNAL_H

#include <stdarg.h>
#include "tp_os.h"
#ifdef __cplusplus
extern "C" {
#endif
	
#define PDO_ENOTEMPL	-1
#define PDO_SUCCESS		0
#define PDO_EGENERAL	1

typedef void pdo_handle_t;
typedef int pdo_status_t;

typedef int (*pdo_callback)(void * cb_data, int num_columns, char ** values, char ** names);

// Çý¶¯²ã
struct pdo_driver_t {
    /** name */
    const char *name;

    void *(*native_handle)(pdo_handle_t *handle);

    pdo_handle_t *(*open)(const char *params, int * errnumber, const char * error, size_t errlen);
    
    pdo_status_t (*check_conn)(pdo_handle_t *handle);

    pdo_status_t (*close)(pdo_handle_t *handle);

    int (*select_db)(pdo_handle_t *handle, const char *name);
		                                   /* 1st argument to callback */
    int (*query)(pdo_handle_t *handle,                                  /* An open database */
		int *nrows, 
		const char *statement,                           /* SQL to be evaluated */
		pdo_callback callback,  /* Callback function */
		void *arg);
  
  	int (*errnumber)(pdo_handle_t *handle);
  	
    pdo_status_t (*error)(pdo_handle_t *handle, const char * errmsg, size_t size);
  
    const char *(*escape)(pdo_handle_t *handle, const char *string); /* return strdup escape string */
};

// PDO¾ä±ú
struct pdo_t {
	void * handle;
	pthread_mutex_t mutex;
	char errmsg[1024];
	struct pdo_driver_t	*driver;
};

#ifdef __cplusplus
}
#endif

#endif
