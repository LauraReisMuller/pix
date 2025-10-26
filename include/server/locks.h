#ifndef LOCKS_H
#define LOCKS_H

#include <pthread.h>
#include <stdexcept>

using namespace std;

class RWLock {
private:
    pthread_mutex_t _mutex;
    pthread_cond_t _cond;

    int _readers;
    int _writers_waiting;
    bool _writer_active;

public:
    RWLock();
    ~RWLock();

    void read_lock();
    void write_lock();
    void unlock();

    RWLock(const RWLock&) = delete;
    RWLock& operator=(const RWLock&) = delete;
};

class ReadGuard {
private:
    RWLock& _lock;

public:
    explicit ReadGuard(RWLock& lock);
    ~ReadGuard();

    ReadGuard(const ReadGuard&) = delete;
    ReadGuard& operator=(const ReadGuard&) = delete;
};

class WriteGuard {
private:
    RWLock& _lock;

public:
    explicit WriteGuard(RWLock& lock);
    ~WriteGuard();

    WriteGuard(const WriteGuard&) = delete;
    WriteGuard& operator=(const WriteGuard&) = delete;
};

#endif // LOCKS_H
