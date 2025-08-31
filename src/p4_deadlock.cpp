// src/p4_deadlock.cpp
// Autor: Fatima Navarro
// Carnet: 24044
// Fecha: 29/08/2025
// Propósito: Demostrar deadlock y técnicas de prevención

#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <ctime>
#include <errno.h>
#include <cstdlib>
#include <signal.h>

inline double now_s() {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;

// Variables para controlar timeout en macOS
volatile bool test_finished = false;
pthread_t timeout_thread;

// Versión deadlock - threads adquieren locks en orden diferente
void* t1_deadlock(void*) {
    printf("T1: Acquiring A...\n");
    pthread_mutex_lock(&A);
    printf("T1: Got A, sleeping...\n");
    usleep(1000); // Dar oportunidad al otro thread de adquirir B
    
    printf("T1: Acquiring B...\n");
    pthread_mutex_lock(&B);
    printf("T1: Got both locks!\n");
    
    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    printf("T1: Released both locks\n");
    test_finished = true;
    return nullptr;
}

void* t2_deadlock(void*) {
    printf("T2: Acquiring B...\n");
    pthread_mutex_lock(&B);
    printf("T2: Got B, sleeping...\n");
    usleep(1000); // Dar oportunidad al otro thread de adquirir A
    
    printf("T2: Acquiring A...\n");
    pthread_mutex_lock(&A);
    printf("T2: Got both locks!\n");
    
    pthread_mutex_unlock(&A);
    pthread_mutex_unlock(&B);
    printf("T2: Released both locks\n");
    test_finished = true;
    return nullptr;
}

// Versión corregida 1 - Ordenar locks consistentemente (A antes de B)
void* t1_ordered(void*) {
    printf("T1: Acquiring A (ordered)...\n");
    pthread_mutex_lock(&A);
    printf("T1: Got A, sleeping...\n");
    usleep(1000);
    
    printf("T1: Acquiring B (ordered)...\n");
    pthread_mutex_lock(&B);
    printf("T1: Got both locks!\n");
    
    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    printf("T1: Released both locks\n");
    return nullptr;
}

void* t2_ordered(void*) {
    printf("T2: Acquiring A (ordered)...\n");
    pthread_mutex_lock(&A);
    printf("T2: Got A, sleeping...\n");
    usleep(1000);
    
    printf("T2: Acquiring B (ordered)...\n");
    pthread_mutex_lock(&B);
    printf("T2: Got both locks!\n");
    
    pthread_mutex_unlock(&B);
    pthread_mutex_unlock(&A);
    printf("T2: Released both locks\n");
    return nullptr;
}

// Versión corregida 2 - trylock con backoff
void* t1_trylock(void*) {
    for (int attempt = 0; attempt < 10; attempt++) {
        printf("T1: Attempt %d - Acquiring A...\n", attempt + 1);
        pthread_mutex_lock(&A);
        printf("T1: Got A, trying B...\n");
        
        if (pthread_mutex_trylock(&B) == 0) {
            printf("T1: Got both locks!\n");
            pthread_mutex_unlock(&B);
            pthread_mutex_unlock(&A);
            printf("T1: Released both locks\n");
            return nullptr;
        }
        
        printf("T1: Couldn't get B, backing off...\n");
        pthread_mutex_unlock(&A);
        usleep(100 * (attempt + 1)); // Backoff exponencial
    }
    
    printf("T1: Failed to acquire both locks after 10 attempts\n");
    return nullptr;
}

void* t2_trylock(void*) {
    for (int attempt = 0; attempt < 10; attempt++) {
        printf("T2: Attempt %d - Acquiring B...\n", attempt + 1);
        pthread_mutex_lock(&B);
        printf("T2: Got B, trying A...\n");
        
        if (pthread_mutex_trylock(&A) == 0) {
            printf("T2: Got both locks!\n");
            pthread_mutex_unlock(&A);
            pthread_mutex_unlock(&B);
            printf("T2: Released both locks\n");
            return nullptr;
        }
        
        printf("T2: Couldn't get A, backing off...\n");
        pthread_mutex_unlock(&B);
        usleep(100 * (attempt + 1)); // Backoff exponencial
    }
    
    printf("T2: Failed to acquire both locks after 10 attempts\n");
    return nullptr;
}

// Thread de timeout para macOS (ya que no tenemos pthread_timedjoin_np)
void* timeout_monitor(void* arg) {
    int timeout_seconds = *((int*)arg);
    sleep(timeout_seconds);
    if (!test_finished) {
        printf("TIMEOUT: Test exceeded %d seconds - likely deadlock detected!\n", timeout_seconds);
        // En una implementación real, aquí podrías enviar señales para cancelar los threads
        exit(1);
    }
    return nullptr;
}

void test_deadlock_scenario(const char* name, void* (*f1)(void*), void* (*f2)(void*)) {
    printf("\n=== %s ===\n", name);
    
    test_finished = false;
    pthread_t x, y;
    double start = now_s();
    
    // Para el test de deadlock, usar timeout
    if (f1 == t1_deadlock) {
        int timeout = 5;
        pthread_create(&timeout_thread, nullptr, timeout_monitor, &timeout);
    }
    
    pthread_create(&x, nullptr, f1, nullptr);
    pthread_create(&y, nullptr, f2, nullptr);
    
    pthread_join(x, nullptr);
    pthread_join(y, nullptr);
    
    test_finished = true;
    
    // Cancelar timeout thread si está corriendo
    if (f1 == t1_deadlock) {
        pthread_cancel(timeout_thread);
        pthread_join(timeout_thread, nullptr);
    }
    
    double end = now_s();
    printf("Both threads completed successfully in %.3fs\n", end - start);
    
    // Resetear estado de mutex
    pthread_mutex_destroy(&A);
    pthread_mutex_destroy(&B);
    pthread_mutex_init(&A, nullptr);
    pthread_mutex_init(&B, nullptr);
}

// Simular escenario más complejo de deadlock
struct Resource {
    int id;
    pthread_mutex_t mutex;
    int value;
    
    void init(int _id) {
        id = _id;
        value = 0;
        pthread_mutex_init(&mutex, nullptr);
    }
    
    void destroy() {
        pthread_mutex_destroy(&mutex);
    }
};

struct TransferArgs {
    Resource* from;
    Resource* to;
    int amount;
    int iterations;
    const char* thread_name;
};

void* transfer_worker(void* p) {
    TransferArgs* args = static_cast<TransferArgs*>(p);
    
    for (int i = 0; i < args->iterations; i++) {
        // Adquirir locks en orden consistente (ID menor primero) para prevenir deadlock
        Resource* first = (args->from->id < args->to->id) ? args->from : args->to;
        Resource* second = (args->from->id < args->to->id) ? args->to : args->from;
        
        pthread_mutex_lock(&first->mutex);
        printf("%s: Acquired lock on resource %d\n", args->thread_name, first->id);
        
        usleep(100); // Simular trabajo
        
        pthread_mutex_lock(&second->mutex);
        printf("%s: Acquired lock on resource %d\n", args->thread_name, second->id);
        
        // Realizar transferencia
        if (args->from->value >= args->amount) {
            args->from->value -= args->amount;
            args->to->value += args->amount;
            printf("%s: Transferred %d from resource %d to resource %d\n",
                   args->thread_name, args->amount, args->from->id, args->to->id);
        }
        
        pthread_mutex_unlock(&second->mutex);
        pthread_mutex_unlock(&first->mutex);
        
        usleep(50); // Pequeña pausa entre operaciones
    }
    
    return nullptr;
}

void test_bank_transfer() {
    printf("\n=== Bank Transfer Simulation ===\n");
    
    Resource account1, account2, account3;
    account1.init(1);
    account2.init(2);
    account3.init(3);
    
    account1.value = 1000;
    account2.value = 500;
    account3.value = 750;
    
    printf("Initial balances: Account1=%d, Account2=%d, Account3=%d\n",
           account1.value, account2.value, account3.value);
    
    pthread_t t1, t2, t3;
    
    TransferArgs args1, args2, args3;
    args1 = {&account1, &account2, 50, 5, "T1"};
    args2 = {&account2, &account3, 30, 5, "T2"};
    args3 = {&account3, &account1, 40, 5, "T3"};
    
    double start = now_s();
    
    pthread_create(&t1, nullptr, transfer_worker, &args1);
    pthread_create(&t2, nullptr, transfer_worker, &args2);
    pthread_create(&t3, nullptr, transfer_worker, &args3);
    
    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);
    pthread_join(t3, nullptr);
    
    double end = now_s();
    
    printf("Final balances: Account1=%d, Account2=%d, Account3=%d\n",
           account1.value, account2.value, account3.value);
    printf("Total balance: %d (should remain 2250)\n",
           account1.value + account2.value + account3.value);
    printf("Completed in %.3fs\n", end - start);
    
    account1.destroy();
    account2.destroy();
    account3.destroy();
}

int main(int argc, char** argv) {
    if (argc > 1) {
        int test_type = std::atoi(argv[1]);
        
        switch (test_type) {
            case 1:
                test_deadlock_scenario("DEADLOCK VERSION", t1_deadlock, t2_deadlock);
                break;
            case 2:
                test_deadlock_scenario("ORDERED LOCKS", t1_ordered, t2_ordered);
                break;
            case 3:
                test_deadlock_scenario("TRYLOCK WITH BACKOFF", t1_trylock, t2_trylock);
                break;
            case 4:
                test_bank_transfer();
                break;
            default:
                printf("Invalid test type. Use 1-4.\n");
                return 1;
        }
    } else {
        printf("Deadlock Detection and Prevention Demo\n");
        printf("Usage: %s <test_type>\n", argv[0]);
        printf("  1: Demonstrate deadlock\n");
        printf("  2: Fixed with ordered locks\n");
        printf("  3: Fixed with trylock and backoff\n");
        printf("  4: Bank transfer simulation\n");
        printf("\nRunning safe tests only (2, 3, 4)...\n");
        
        test_deadlock_scenario("ORDERED LOCKS", t1_ordered, t2_ordered);
        test_deadlock_scenario("TRYLOCK WITH BACKOFF", t1_trylock, t2_trylock);
        test_bank_transfer();
    }
    
    pthread_mutex_destroy(&A);
    pthread_mutex_destroy(&B);
    
    return 0;
}
