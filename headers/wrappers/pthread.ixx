module;
#include <pthread.h>

export module pthread;


export namespace pthread {
    using ::pthread_cond_t;
    using ::pthread_condattr_t;
    using ::pthread_key_t;
    using ::pthread_mutex_t;
    using ::pthread_mutexattr_t;
    using ::pthread_once_t;
    using ::pthread_rwlock_t;
    using ::pthread_rwlockattr_t;
    using ::pthread_t;

    using ::pthread_cond_broadcast;
    using ::pthread_cond_destroy;
    using ::pthread_cond_init;
    using ::pthread_cond_signal;
    using ::pthread_cond_wait;
    using ::pthread_condattr_destroy;
    using ::pthread_condattr_init;
    using ::pthread_create;
    using ::pthread_equal;
    using ::pthread_getspecific;
    using ::pthread_join;
    using ::pthread_key_create;
    using ::pthread_key_delete;
    using ::pthread_mutex_destroy;
    using ::pthread_mutex_init;
    using ::pthread_mutex_lock;
    using ::pthread_mutex_trylock;
    using ::pthread_mutex_unlock;
    using ::pthread_mutexattr_destroy;
    using ::pthread_mutexattr_init;
    using ::pthread_mutexattr_settype;
    using ::pthread_once;
    using ::pthread_rwlock_destroy;
    using ::pthread_rwlock_init;
    using ::pthread_rwlock_rdlock;
    using ::pthread_rwlock_unlock;
    using ::pthread_rwlock_wrlock;
    using ::pthread_rwlockattr_destroy;
    using ::pthread_rwlockattr_init;
    using ::pthread_setspecific;
    using ::pthread_self;
}
