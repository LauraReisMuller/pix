#include "server/locks.h"

// === RWLock ===
RWLock::RWLock() {
    if (pthread_rwlock_init(&_lock, nullptr) != 0) {
        throw runtime_error("Failed to initialize rwlock");
    }
}

RWLock::~RWLock() {
    pthread_rwlock_destroy(&_lock);
}

void RWLock::read_lock() {
    pthread_rwlock_rdlock(&_lock);
}

void RWLock::write_lock() {
    pthread_rwlock_wrlock(&_lock);
}

void RWLock::unlock() {
    pthread_rwlock_unlock(&_lock);
}

// === ReadGuard ===
ReadGuard::ReadGuard(RWLock& lock) : _lock(lock) {
    _lock.read_lock();
}

ReadGuard::~ReadGuard() {
    _lock.unlock();
}

// === WriteGuard ===
WriteGuard::WriteGuard(RWLock& lock) : _lock(lock) {
    _lock.write_lock();
}

WriteGuard::~WriteGuard() {
    _lock.unlock();
}