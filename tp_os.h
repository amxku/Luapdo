#ifndef TP_OS_H
#define TP_OS_H

#if defined(_WIN32)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <process.h>
#include <time.h>

#define usleep(x)    Sleep((x + 999) / 1000)

/*
 * Defines that adapt Windows API threads to pthreads API
 */
#define pthread_mutex_t CRITICAL_SECTION
#define pthread_mutexattr_t CRITICAL_SECTION

#define pthread_mutexattr_init(a)
#define pthread_mutexattr_settype(a, b)
#define pthread_mutexattr_destroy(a)

#define pthread_mutex_init(a,b) (InitializeCriticalSection((a)),0)
#define pthread_mutex_destroy(a) (DeleteCriticalSection((a)),0)
#define pthread_mutex_lock(a) (EnterCriticalSection(a),0)
#define pthread_mutex_unlock(a) (LeaveCriticalSection(a),0)
#define pthread_self GetCurrentThreadId
#define getpid  GetCurrentProcessId

#define PTHREAD_MUTEX_RECURSIVE_NP 1

typedef struct thread_attr {
    DWORD dwStackSize;
    DWORD dwCreatingFlag;
    int detachstate;
    int priority;
} pthread_attr_t;


enum {
/*
* pthread_attr_{get,set}detachstate
    */
  PTHREAD_CREATE_JOINABLE       = 0,  /* Default */
  PTHREAD_CREATE_DETACHED       = 1,
};


#ifndef HAVE_STRUCT_TIMESPEC
#define HAVE_STRUCT_TIMESPEC 1
struct timespec {
    long tv_sec;
    long tv_nsec;
};
#endif /* HAVE_STRUCT_TIMESPEC */

#ifndef ETIMEDOUT
#  define ETIMEDOUT 10060     /* This is the value in winsock.h. */
#endif


extern int pthread_attr_destroy(pthread_attr_t *attr);
extern int pthread_attr_setstacksize(pthread_attr_t * attr, size_t stacksize);
extern int pthread_attr_setdetachstate (pthread_attr_t * attr, int detachstate);
extern int pthread_attr_init (pthread_attr_t * attr);

/* _endthreadex not close handle, _endthread() close */
#define pthread_exit(a) _endthreadex(0)

/*
 * Simple thread creation implementation using pthread API
 */
typedef struct {
        HANDLE handle;
        void *(*start_routine)(void*);
        void *arg;
} pthread_t;

extern int pthread_create(pthread_t *thread, pthread_attr_t * attr,
                          void *(*start_routine)(void*), void *arg);

/*
 * To avoid the need of copying a struct, we use small macro wrapper to pass
 * pointer to win32_pthread_join instead.
 */
#define pthread_join(a, b) win32_pthread_join(&(a), (b))

extern int win32_pthread_join(pthread_t *thread, void **value_ptr);

typedef HANDLE  sem_t;
#define sem_init(p,s,v)   (!(*p = CreateSemaphore(NULL, (long)(v), (long)(v), NULL)))
#define sem_post(p)     (!ReleaseSemaphore(*p, 1, NULL))
#define sem_wait(p)     (!(WaitForSingleObject(*p, INFINITE) == WAIT_OBJECT_0))
#define sem_destroy(p)  (!CloseHandle(*p))

typedef HANDLE  pthread_cond_t;
int pthread_cond_wait( pthread_cond_t * cond, pthread_mutex_t * mutex );
int pthread_cond_timedwait( pthread_cond_t * cond, pthread_mutex_t * mutex, const struct timespec *abstime);
int pthread_cond_signal( pthread_cond_t * cond );
int pthread_cond_init( pthread_cond_t * cond, void * attr );
int pthread_cond_destroy( pthread_cond_t * cond );
#else

#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>

#endif

int
mt_init_recursive(pthread_mutex_t * mutex);

#endif /* TP_OS_H */
