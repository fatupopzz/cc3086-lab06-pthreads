// src/p1_counter.cpp
// Autor: Fatima Navarro
// Carnet: 24044
// Fecha: 29/08/2025
// Propósito: Demostrar race conditions y comparar mutex vs sharded counters

#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <ctime>
#include <atomic>
#include <cstdlib>

inline double now_s() {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

struct Args {
    long iters;
    long* global;
    pthread_mutex_t* mtx;
    int thread_id;
    long* local_counter; // Para sharded approach
};

void* worker_naive(void* p) {
    Args* a = static_cast<Args*>(p);
    for (long i = 0; i < a->iters; i++) {
        (*a->global)++; // Race condition intencional
    }
    return nullptr;
}

void* worker_mutex(void* p) {
    Args* a = static_cast<Args*>(p);
    for (long i = 0; i < a->iters; i++) {
        pthread_mutex_lock(a->mtx);
        (*a->global)++;
        pthread_mutex_unlock(a->mtx);
    }
    return nullptr;
}

void* worker_sharded(void* p) {
    Args* a = static_cast<Args*>(p);
    for (long i = 0; i < a->iters; i++) {
        a->local_counter[a->thread_id]++;
    }
    return nullptr;
}

void* worker_atomic(void* p) {
    Args* a = static_cast<Args*>(p);
    std::atomic<long>* atomic_counter = reinterpret_cast<std::atomic<long>*>(a->global);
    for (long i = 0; i < a->iters; i++) {
        atomic_counter->fetch_add(1, std::memory_order_relaxed);
    }
    return nullptr;
}

void run_test(const char* name, void* (*worker)(void*), int T, long it, bool use_atomic = false) {
    long global = 0;
    std::atomic<long> atomic_global(0);
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    std::vector<pthread_t> th(T);
    std::vector<long> local_counters(T, 0);
    std::vector<Args*> args_ptrs(T); // Para cleanup
    
    double start = now_s();
    
    for (int i = 0; i < T; i++) {
        Args* a = (Args*)malloc(sizeof(Args));
        a->iters = it;
        a->global = use_atomic ? reinterpret_cast<long*>(&atomic_global) : &global;
        a->mtx = &mtx;
        a->thread_id = i;
        a->local_counter = local_counters.data();
        args_ptrs[i] = a;
        
        pthread_create(&th[i], nullptr, worker, a);
    }
    
    for (int i = 0; i < T; i++) {
        pthread_join(th[i], nullptr);
    }
    
    // Fase de reduce para sharded
    if (worker == worker_sharded) {
        for (int i = 0; i < T; i++) {
            global += local_counters[i];
        }
    }
    
    double end = now_s();
    long expected = (long)T * it;
    long actual = use_atomic ? atomic_global.load() : global;
    
    printf("%s: total=%ld (expected=%ld) time=%.3fs ops/sec=%.0f\n",
           name, actual, expected, end - start, expected / (end - start));
    
    // Cleanup
    for (int i = 0; i < T; i++) {
        free(args_ptrs[i]);
    }
    pthread_mutex_destroy(&mtx);
}

int main(int argc, char** argv) {
    int T = (argc > 1) ? std::atoi(argv[1]) : 4;
    long it = (argc > 2) ? std::atol(argv[2]) : 1000000;
    
    printf("Testing with %d threads, %ld iterations per thread\n", T, it);
    printf("Expected total: %ld\n\n", (long)T * it);
    
    // Ejecutar multiples veces para mostrar comportamiento no determinista
    printf("=== NAIVE (Race Condition) ===\n");
    for (int run = 0; run < 3; run++) {
        run_test("NAIVE", worker_naive, T, it);
    }
    
    printf("\n=== MUTEX PROTECTED ===\n");
    run_test("MUTEX", worker_mutex, T, it);
    
    printf("\n=== SHARDED COUNTERS ===\n");
    run_test("SHARDED", worker_sharded, T, it);
    
    printf("\n=== ATOMIC (C++17) ===\n");
    run_test("ATOMIC", worker_atomic, T, it, true);
    
    return 0;
}
