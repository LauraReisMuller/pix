#include "server/locks.h"

// === RWLock ===
#include <pthread.h>
#include <stdexcept>

RWLock::RWLock() : _readers(0), _writers_waiting(0), _writer_active(false) {
    if (pthread_mutex_init(&_mutex, nullptr) != 0)
        throw std::runtime_error("Failed to initialize mutex");
    if (pthread_cond_init(&_cond, nullptr) != 0)
        throw std::runtime_error("Failed to initialize condition variable");
}

RWLock::~RWLock() {
    pthread_mutex_destroy(&_mutex);
    pthread_cond_destroy(&_cond);
}

void RWLock::read_lock() {
    pthread_mutex_lock(&_mutex);

    // Espera até que não haja escritor ativo ou esperando
    while (_writer_active || _writers_waiting > 0)
        pthread_cond_wait(&_cond, &_mutex);

    _readers++;

    pthread_mutex_unlock(&_mutex);
}

void RWLock::write_lock() {
    pthread_mutex_lock(&_mutex);

    _writers_waiting++;

    // Espera até que não haja leitores nem escritor ativo
    while (_writer_active || _readers > 0)
        pthread_cond_wait(&_cond, &_mutex);

    _writers_waiting--;
    _writer_active = true;

    pthread_mutex_unlock(&_mutex);
}

void RWLock::unlock() {
    pthread_mutex_lock(&_mutex);

    if (_writer_active) {
        // Libera o escritor
        _writer_active = false;
    } else if (_readers > 0) {
        // Libera um leitor
        _readers--;
    }

    // Sinaliza para todos (há chance de leitores/escritores esperando)
    pthread_cond_broadcast(&_cond);

    pthread_mutex_unlock(&_mutex);
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