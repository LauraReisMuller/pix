/**
 * Implementação de um RWLock (leitores/escritores) usando pthreads.
 *
 * Comportamento:
 *  - `read_lock` bloqueia se houver um escritor activo ou escritores à espera
 *  - `write_lock` aguarda até não haver leitores nem escritor ativo
 *  - `unlock` liberta a lock apropriada e sinaliza condicionalmente
 *
 * Helpers:
 *  - ReadGuard/WriteGuard são wrappers RAII para facilitar o uso
 */

#include "server/locks.h"

// === RWLock ===
#include <pthread.h>
#include <stdexcept>

RWLock::RWLock() : _readers(0), _writers_waiting(0), _writer_active(false) {
    if (pthread_mutex_init(&_mutex, nullptr) != 0)
        throw runtime_error("Failed to initialize mutex");
    if (pthread_cond_init(&_cond, nullptr) != 0)
        throw runtime_error("Failed to initialize condition variable");
}

RWLock::~RWLock() {
    pthread_mutex_destroy(&_mutex);
    pthread_cond_destroy(&_cond);
}

void RWLock::read_lock() {
    pthread_mutex_lock(&_mutex);

    while (_writer_active || _writers_waiting > 0)
        pthread_cond_wait(&_cond, &_mutex);

    _readers++;

    pthread_mutex_unlock(&_mutex);
}

void RWLock::write_lock() {
    pthread_mutex_lock(&_mutex);

    _writers_waiting++;

    while (_writer_active || _readers > 0)
        pthread_cond_wait(&_cond, &_mutex);

    _writers_waiting--;
    _writer_active = true;

    pthread_mutex_unlock(&_mutex);
}

void RWLock::unlock() {
    pthread_mutex_lock(&_mutex);

    if (_writer_active) {
        _writer_active = false;
    } else if (_readers > 0) {
        _readers--;
    }
    pthread_cond_broadcast(&_cond);

    pthread_mutex_unlock(&_mutex);
}

ReadGuard::ReadGuard(RWLock& lock) : _lock(lock) {
    _lock.read_lock();
}

ReadGuard::~ReadGuard() {
    _lock.unlock();
}

WriteGuard::WriteGuard(RWLock& lock) : _lock(lock) {
    _lock.write_lock();
}

WriteGuard::~WriteGuard() {
    _lock.unlock();
}