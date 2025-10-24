#ifndef LOCKS_H
#define LOCKS_H

#include <pthread.h>
#include <stdexcept>

using namespace std;

/**
 * @brief Wrapper RAII para pthread_rwlock_t
 * Permite múltiplos leitores ou um único escritor
 */
class RWLock {
private:
    pthread_rwlock_t _lock;
    
public:
    RWLock();
    ~RWLock();
    
    void read_lock();
    void write_lock();
    void unlock();
    
    // Previne cópia
    RWLock(const RWLock&) = delete;
    RWLock& operator=(const RWLock&) = delete;
};

/**
 * @brief Guard RAII para leitura (permite múltiplos leitores)
 */
class ReadGuard {
private:
    RWLock& _lock;
    
public:
    explicit ReadGuard(RWLock& lock);
    ~ReadGuard();
    
    ReadGuard(const ReadGuard&) = delete;
    ReadGuard& operator=(const ReadGuard&) = delete;
};

/**
 * @brief Guard RAII para escrita (exclusivo)
 */
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