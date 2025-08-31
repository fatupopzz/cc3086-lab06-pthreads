// src/p3_rw.cpp
// Autor: Fatima Navarro
// Carnet: 24044
// Fecha: 29/08/2025
// Propósito: Comparar pthread_rwlock vs pthread_mutex en hash map compartido

#include <pthread.h>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <random>

inline double now_s() {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

const int NBUCKET = 1024;

struct Node {
    int k, v;
    Node* next;
    Node(int key, int val) : k(key), v(val), next(nullptr) {}
};

struct MapRW {
    Node* b[1024]; // Usar tamaño fijo
    pthread_rwlock_t rw;
    
    MapRW() {
        for (int i = 0; i < NBUCKET; i++) {
            b[i] = nullptr;
        }
        pthread_rwlock_init(&rw, nullptr);
    }
    
    ~MapRW() {
        for (int i = 0; i < NBUCKET; i++) {
            Node* curr = b[i];
            while (curr) {
                Node* next = curr->next;
                delete curr;
                curr = next;
            }
        }
        pthread_rwlock_destroy(&rw);
    }
    
    int hash(int k) const {
        return ((unsigned int)k) % NBUCKET;
    }
};

struct MapMutex {
    Node* b[1024]; // Usar tamaño fijo
    pthread_mutex_t m;
    
    MapMutex() {
        for (int i = 0; i < NBUCKET; i++) {
            b[i] = nullptr;
        }
        pthread_mutex_init(&m, nullptr);
    }
    
    ~MapMutex() {
        for (int i = 0; i < NBUCKET; i++) {
            Node* curr = b[i];
            while (curr) {
                Node* next = curr->next;
                delete curr;
                curr = next;
            }
        }
        pthread_mutex_destroy(&m);
    }
    
    int hash(int k) const {
        return ((unsigned int)k) % NBUCKET;
    }
};

int map_get_rw(MapRW* m, int k) {
    pthread_rwlock_rdlock(&m->rw);
    
    int bucket = m->hash(k);
    Node* curr = m->b[bucket];
    int result = -1;
    
    while (curr) {
        if (curr->k == k) {
            result = curr->v;
            break;
        }
        curr = curr->next;
    }
    
    pthread_rwlock_unlock(&m->rw);
    return result;
}

void map_put_rw(MapRW* m, int k, int v) {
    pthread_rwlock_wrlock(&m->rw);
    
    int bucket = m->hash(k);
    Node* curr = m->b[bucket];
    
    // Verificar si la key existe
    while (curr) {
        if (curr->k == k) {
            curr->v = v;
            pthread_rwlock_unlock(&m->rw);
            return;
        }
        curr = curr->next;
    }
    
    // Insertar nuevo nodo al inicio
    Node* new_node = new Node(k, v);
    new_node->next = m->b[bucket];
    m->b[bucket] = new_node;
    
    pthread_rwlock_unlock(&m->rw);
}

int map_get_mutex(MapMutex* m, int k) {
    pthread_mutex_lock(&m->m);
    
    int bucket = m->hash(k);
    Node* curr = m->b[bucket];
    int result = -1;
    
    while (curr) {
        if (curr->k == k) {
            result = curr->v;
            break;
        }
        curr = curr->next;
    }
    
    pthread_mutex_unlock(&m->m);
    return result;
}

void map_put_mutex(MapMutex* m, int k, int v) {
    pthread_mutex_lock(&m->m);
    
    int bucket = m->hash(k);
    Node* curr = m->b[bucket];
    
    // Verificar si la key existe
    while (curr) {
        if (curr->k == k) {
            curr->v = v;
            pthread_mutex_unlock(&m->m);
            return;
        }
        curr = curr->next;
    }
    
    // Insertar nuevo nodo al inicio
    Node* new_node = new Node(k, v);
    new_node->next = m->b[bucket];
    m->b[bucket] = new_node;
    
    pthread_mutex_unlock(&m->m);
}

struct WorkerArgsRW {
    MapRW* map;
    int operations;
    int read_percentage;
    int thread_id;
    int* ops_completed;
};

struct WorkerArgsMutex {
    MapMutex* map;
    int operations;
    int read_percentage;
    int thread_id;
    int* ops_completed;
};

void* worker_rw(void* p) {
    WorkerArgsRW* args = static_cast<WorkerArgsRW*>(p);
    std::mt19937 gen(args->thread_id);
    std::uniform_int_distribution<> dis(0, 99);
    std::uniform_int_distribution<> key_dis(0, 9999);
    
    int completed = 0;
    
    for (int i = 0; i < args->operations; i++) {
        int key = key_dis(gen);
        
        if (dis(gen) < args->read_percentage) {
            // Operación de lectura
            map_get_rw(args->map, key);
        } else {
            // Operación de escritura
            map_put_rw(args->map, key, key * 2);
        }
        completed++;
    }
    
    *args->ops_completed = completed;
    return nullptr;
}

void* worker_mutex(void* p) {
    WorkerArgsMutex* args = static_cast<WorkerArgsMutex*>(p);
    std::mt19937 gen(args->thread_id);
    std::uniform_int_distribution<> dis(0, 99);
    std::uniform_int_distribution<> key_dis(0, 9999);
    
    int completed = 0;
    
    for (int i = 0; i < args->operations; i++) {
        int key = key_dis(gen);
        
        if (dis(gen) < args->read_percentage) {
            // Operación de lectura
            map_get_mutex(args->map, key);
        } else {
            // Operación de escritura
            map_put_mutex(args->map, key, key * 2);
        }
        completed++;
    }
    
    *args->ops_completed = completed;
    return nullptr;
}

void test_scenario(const char* name, int num_threads, int ops_per_thread, int read_percentage) {
    printf("\n=== %s (Threads: %d, Ops: %d, Reads: %d%%) ===\n", 
           name, num_threads, ops_per_thread, read_percentage);
    
    // Test con rwlock
    {
        MapRW map_rw;
        std::vector<pthread_t> threads(num_threads);
        std::vector<WorkerArgsRW> args(num_threads);
        std::vector<int> ops_completed(num_threads);
        
        // Inicializar argumentos
        for (int i = 0; i < num_threads; i++) {
            args[i].map = &map_rw;
            args[i].operations = ops_per_thread;
            args[i].read_percentage = read_percentage;
            args[i].thread_id = i;
            args[i].ops_completed = &ops_completed[i];
        }
        
        double start = now_s();
        
        for (int i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], nullptr, worker_rw, &args[i]);
        }
        
        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], nullptr);
        }
        
        double end = now_s();
        
        int total_ops = 0;
        for (int i = 0; i < num_threads; i++) {
            total_ops += ops_completed[i];
        }
        
        printf("RWLOCK: %.3fs, %.0f ops/sec\n", 
               end - start, total_ops / (end - start));
    }
    
    // Test con mutex
    {
        MapMutex map_mutex;
        std::vector<pthread_t> threads(num_threads);
        std::vector<WorkerArgsMutex> args(num_threads);
        std::vector<int> ops_completed(num_threads);
        
        // Inicializar argumentos
        for (int i = 0; i < num_threads; i++) {
            args[i].map = &map_mutex;
            args[i].operations = ops_per_thread;
            args[i].read_percentage = read_percentage;
            args[i].thread_id = i;
            args[i].ops_completed = &ops_completed[i];
        }
        
        double start = now_s();
        
        for (int i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], nullptr, worker_mutex, &args[i]);
        }
        
        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], nullptr);
        }
        
        double end = now_s();
        
        int total_ops = 0;
        for (int i = 0; i < num_threads; i++) {
            total_ops += ops_completed[i];
        }
        
        printf("MUTEX:  %.3fs, %.0f ops/sec\n", 
               end - start, total_ops / (end - start));
    }
}

int main(int argc, char** argv) {
    int num_threads = (argc > 1) ? std::atoi(argv[1]) : 4;
    int ops_per_thread = (argc > 2) ? std::atoi(argv[2]) : 100000;
    
    printf("Readers/Writers Performance Comparison\n");
    
    test_scenario("90/10 Read/Write", num_threads, ops_per_thread, 90);
    test_scenario("70/30 Read/Write", num_threads, ops_per_thread, 70);
    test_scenario("50/50 Read/Write", num_threads, ops_per_thread, 50);
    
    return 0;
}
