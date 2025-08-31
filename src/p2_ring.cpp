// src/p2_ring.cpp
// Autor: Fatima Navarro
// Carnet: 24044
// Fecha: 29/08/2025
// Propósito: Implementar buffer circular con pthread_mutex y pthread_cond

#include <pthread.h>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <vector>

inline double now_s() {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

const std::size_t Q = 1024;

struct Ring {
    int buf[1024]; // Usar tamaño fijo en lugar de Q para compatibilidad
    std::size_t head;
    std::size_t tail;
    std::size_t count;
    pthread_mutex_t m;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    bool stop;
    
    Ring() : head(0), tail(0), count(0), stop(false) {
        pthread_mutex_init(&m, nullptr);
        pthread_cond_init(&not_full, nullptr);
        pthread_cond_init(&not_empty, nullptr);
    }
    
    ~Ring() {
        pthread_mutex_destroy(&m);
        pthread_cond_destroy(&not_full);
        pthread_cond_destroy(&not_empty);
    }
};

void ring_push(Ring* r, int v) {
    pthread_mutex_lock(&r->m);
    while (r->count == Q && !r->stop) {
        pthread_cond_wait(&r->not_full, &r->m);
    }
    if (!r->stop) {
        r->buf[r->head] = v;
        r->head = (r->head + 1) % Q;
        r->count++;
        pthread_cond_signal(&r->not_empty);
    }
    pthread_mutex_unlock(&r->m);
}

bool ring_pop(Ring* r, int* out) {
    pthread_mutex_lock(&r->m);
    while (r->count == 0 && !r->stop) {
        pthread_cond_wait(&r->not_empty, &r->m);
    }
    if (r->count == 0 && r->stop) {
        pthread_mutex_unlock(&r->m);
        return false;
    }
    *out = r->buf[r->tail];
    r->tail = (r->tail + 1) % Q;
    r->count--;
    pthread_cond_signal(&r->not_full);
    pthread_mutex_unlock(&r->m);
    return true;
}

void ring_shutdown(Ring* r) {
    pthread_mutex_lock(&r->m);
    r->stop = true;
    pthread_cond_broadcast(&r->not_full);
    pthread_cond_broadcast(&r->not_empty);
    pthread_mutex_unlock(&r->m);
}

struct ProducerArgs {
    Ring* ring;
    int items_to_produce;
    int producer_id;
};

struct ConsumerArgs {
    Ring* ring;
    int* items_consumed;
    int consumer_id;
};

void* producer(void* p) {
    ProducerArgs* args = static_cast<ProducerArgs*>(p);
    
    for (int i = 0; i < args->items_to_produce; i++) {
        int value = args->producer_id * 10000 + i;
        ring_push(args->ring, value);
        
        // Simular trabajo
        if (i % 1000 == 0) {
            usleep(1);
        }
    }
    
    printf("Producer %d finished producing %d items\n", 
           args->producer_id, args->items_to_produce);
    return nullptr;
}

void* consumer(void* p) {
    ConsumerArgs* args = static_cast<ConsumerArgs*>(p);
    int value;
    int consumed = 0;
    
    while (ring_pop(args->ring, &value)) {
        consumed++;
        
        // Simular trabajo
        if (consumed % 1000 == 0) {
            usleep(1);
        }
    }
    
    *args->items_consumed = consumed;
    printf("Consumer %d finished consuming %d items\n", 
           args->consumer_id, consumed);
    return nullptr;
}

int main(int argc, char** argv) {
    int num_producers = (argc > 1) ? std::atoi(argv[1]) : 2;
    int num_consumers = (argc > 2) ? std::atoi(argv[2]) : 2;
    int items_per_producer = (argc > 3) ? std::atoi(argv[3]) : 10000;
    
    printf("Testing with %d producers, %d consumers, %d items per producer\n",
           num_producers, num_consumers, items_per_producer);
    
    Ring ring;
    
    // Crear threads
    std::vector<pthread_t> producers(num_producers);
    std::vector<pthread_t> consumers(num_consumers);
    std::vector<ProducerArgs> prod_args(num_producers);
    std::vector<ConsumerArgs> cons_args(num_consumers);
    std::vector<int> items_consumed(num_consumers, 0);
    
    double start = now_s();
    
    // Inicializar argumentos de productores
    for (int i = 0; i < num_producers; i++) {
        prod_args[i].ring = &ring;
        prod_args[i].items_to_produce = items_per_producer;
        prod_args[i].producer_id = i;
    }
    
    // Inicializar argumentos de consumidores
    for (int i = 0; i < num_consumers; i++) {
        cons_args[i].ring = &ring;
        cons_args[i].items_consumed = &items_consumed[i];
        cons_args[i].consumer_id = i;
    }
    
    // Iniciar productores
    for (int i = 0; i < num_producers; i++) {
        pthread_create(&producers[i], nullptr, producer, &prod_args[i]);
    }
    
    // Iniciar consumidores
    for (int i = 0; i < num_consumers; i++) {
        pthread_create(&consumers[i], nullptr, consumer, &cons_args[i]);
    }
    
    // Esperar a que terminen los productores
    for (int i = 0; i < num_producers; i++) {
        pthread_join(producers[i], nullptr);
    }
    
    // Dejar que los consumidores procesen los items restantes
    sleep(1);
    
    // Apagar ring buffer
    ring_shutdown(&ring);
    
    // Esperar a que terminen los consumidores
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumers[i], nullptr);
    }
    
    double end = now_s();
    
    // Calcular totales
    int total_produced = num_producers * items_per_producer;
    int total_consumed = 0;
    for (int i = 0; i < num_consumers; i++) {
        total_consumed += items_consumed[i];
    }
    
    printf("\nResults:\n");
    printf("Total produced: %d\n", total_produced);
    printf("Total consumed: %d\n", total_consumed);
    printf("Items lost: %d\n", total_produced - total_consumed);
    printf("Time: %.3fs\n", end - start);
    printf("Throughput: %.0f items/sec\n", total_consumed / (end - start));
    
    return 0;
}
