#include <pthread.h>
#include <memory>

// RAII Wrappers

struct ReadLock
{
    pthread_rwlock_t &lock; // Reference here to ensure that this wrapper never accidentally owns a lock as it owuld be counter intuitive

    ReadLock(pthread_rwlock_t rwlock) : lock(rwlock)
    {
        pthread_rwlock_rdlock(std::addressof(lock));
    }

    void unlock()
    {
        pthread_rwlock_unlock(std::addressof(lock));
    }

    ~ReadLock()
    {
        unlock();
    }
};

struct WriteLock
{
    pthread_rwlock_t &lock; // Reference here to ensure that this wrapper never accidentally owns a lock as it owuld be counter intuitive

    WriteLock(pthread_rwlock_t rwlock) : lock(rwlock)
    {
        pthread_rwlock_wrlock(std::addressof(lock));
    }

    void unlock()
    {
        pthread_rwlock_unlock(std::addressof(lock));
    }

    ~WriteLock()
    {
        unlock();
    }
};