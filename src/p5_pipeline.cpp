// src/p5_pipeline.cpp
// Autor: Fatima Navarro
// Carnet: 24044
// Fecha: 29/08/2025
// Propósito: Pipeline de 3 etapas con pthread_barrier y pthread_once

#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <random>
#include <unistd.h>

// Definir constante para macOS
#ifndef PTHREAD_BARRIER_SERIAL_THREAD
#define PTHREAD_BARRIER_SERIAL_THREAD 1
#endif

// Implementación manual de barrier para macOS
struct pthread_barrier_t {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int tripCount;
    int generation;
};

int pthread_barrier_init(pthread_barrier_t* barrier, void* attr, unsigned int count) {
    barrier->count = 0;
    barrier->tripCount = count;
    barrier->generation = 0;
    pthread_mutex_init(&barrier->mutex, nullptr);
    pthread_cond_init(&barrier->cond, nullptr);
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t* barrier) {
    pthread_mutex_lock(&barrier->mutex);
    int gen = barrier->generation;
    
    barrier->count++;
    if (barrier->count == barrier->tripCount) {
        barrier->generation++;
        barrier->count = 0;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }
    
    while (gen == barrier->generation) {
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }
    pthread_mutex_unlock(&barrier->mutex);
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t* barrier) {
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
    return 0;
}

inline double now_s() {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

const int TICKS = 100;
const int BUFFER_SIZE = 50;

// Recursos compartidos
static pthread_barrier_t barrier;
static pthread_once_t once_flag = PTHREAD_ONCE_INIT;
static FILE* log_file = nullptr;
static double start_time;

// Buffers del pipeline
struct PipelineData {
    std::vector<int> raw_data;
    std::vector<int> filtered_data;
    std::vector<int> processed_data;
    long final_result;
    
    pthread_mutex_t raw_mutex;
    pthread_mutex_t filtered_mutex;
    pthread_mutex_t processed_mutex;
    
    PipelineData() : final_result(0) {
        pthread_mutex_init(&raw_mutex, nullptr);
        pthread_mutex_init(&filtered_mutex, nullptr);
        pthread_mutex_init(&processed_mutex, nullptr);
    }
    
    ~PipelineData() {
        pthread_mutex_destroy(&raw_mutex);
        pthread_mutex_destroy(&filtered_mutex);
        pthread_mutex_destroy(&processed_mutex);
    }
} pipeline_data;

static void init_shared() {
    log_file = fopen("pipeline.log", "w");
    if (log_file) {
        fprintf(log_file, "Pipeline execution started\n");
        fflush(log_file);
    }
    printf("Shared resources initialized\n");
    start_time = now_s();
}

static void log_stage_activity(int stage_id, int tick, const char* activity) {
    if (log_file) {
        double current_time = now_s() - start_time;
        fprintf(log_file, "[%.3f] Stage %d, Tick %d: %s\n", current_time, stage_id, tick, activity);
        fflush(log_file);
    }
}

// Etapa 1: Generador de datos
void* stage_generator(void* p) {
    long id = (long)p;
    pthread_once(&once_flag, init_shared);
    
    std::mt19937 gen(id);
    std::uniform_int_distribution<> dis(1, 100);
    
    printf("Stage %ld (Generator) starting\n", id);
    
    for (int t = 0; t < TICKS; t++) {
        // Generar datos aleatorios
        std::vector<int> batch;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            batch.push_back(dis(gen));
        }
        
        // Agregar al buffer de datos crudos
        pthread_mutex_lock(&pipeline_data.raw_mutex);
        for (int val : batch) {
            pipeline_data.raw_data.push_back(val);
        }
        pthread_mutex_unlock(&pipeline_data.raw_mutex);
        
        log_stage_activity(id, t, "Generated data batch");
        
        // Punto de sincronización
        pthread_barrier_wait(&barrier);
    }
    
    printf("Stage %ld (Generator) completed\n", id);
    return nullptr;
}

// Etapa 2: Filtro de datos
void* stage_filter(void* p) {
    long id = (long)p;
    pthread_once(&once_flag, init_shared);
    
    printf("Stage %ld (Filter) starting\n", id);
    
    for (int t = 0; t < TICKS; t++) {
        // Procesar datos crudos
        std::vector<int> to_process;
        
        pthread_mutex_lock(&pipeline_data.raw_mutex);
        // Tomar algunos datos del buffer crudo
        int take_count = std::min((int)pipeline_data.raw_data.size(), BUFFER_SIZE);
        for (int i = 0; i < take_count; i++) {
            to_process.push_back(pipeline_data.raw_data[i]);
        }
        pipeline_data.raw_data.erase(pipeline_data.raw_data.begin(), 
                                   pipeline_data.raw_data.begin() + take_count);
        pthread_mutex_unlock(&pipeline_data.raw_mutex);
        
        // Filtrar: mantener solo números pares > 20
        std::vector<int> filtered;
        for (int val : to_process) {
            if (val % 2 == 0 && val > 20) {
                filtered.push_back(val);
            }
        }
        
        // Agregar al buffer filtrado
        pthread_mutex_lock(&pipeline_data.filtered_mutex);
        for (int val : filtered) {
            pipeline_data.filtered_data.push_back(val);
        }
        pthread_mutex_unlock(&pipeline_data.filtered_mutex);
        
        log_stage_activity(id, t, "Filtered data");
        
        // Punto de sincronización
        pthread_barrier_wait(&barrier);
    }
    
    printf("Stage %ld (Filter) completed\n", id);
    return nullptr;
}

// Etapa 3: Reductor de datos
void* stage_reducer(void* p) {
    long id = (long)p;
    pthread_once(&once_flag, init_shared);
    
    printf("Stage %ld (Reducer) starting\n", id);
    
    for (int t = 0; t < TICKS; t++) {
        // Procesar datos filtrados
        std::vector<int> to_reduce;
        
        pthread_mutex_lock(&pipeline_data.filtered_mutex);
        // Tomar todos los datos filtrados disponibles
        to_reduce = pipeline_data.filtered_data;
        pipeline_data.filtered_data.clear();
        pthread_mutex_unlock(&pipeline_data.filtered_mutex);
        
        // Reducir: calcular suma y agregar al resultado final
        long local_sum = 0;
        for (int val : to_reduce) {
            local_sum += val;
        }
        
        pipeline_data.final_result += local_sum;
        
        log_stage_activity(id, t, "Reduced data");
        
        // Punto de sincronización
        pthread_barrier_wait(&barrier);
    }
    
    printf("Stage %ld (Reducer) completed. Final result: %ld\n", id, pipeline_data.final_result);
    return nullptr;
}

void* stage_monitor(void* p) {
    long id = (long)p;
    pthread_once(&once_flag, init_shared);
    
    printf("Stage %ld (Monitor) starting\n", id);
    
    for (int t = 0; t < TICKS; t++) {
        // Monitorear salud del pipeline
        pthread_mutex_lock(&pipeline_data.raw_mutex);
        int raw_size = pipeline_data.raw_data.size();
        pthread_mutex_unlock(&pipeline_data.raw_mutex);
        
        pthread_mutex_lock(&pipeline_data.filtered_mutex);
        int filtered_size = pipeline_data.filtered_data.size();
        pthread_mutex_unlock(&pipeline_data.filtered_mutex);
        
        if (t % 10 == 0) { // Imprimir cada 10 ticks
            printf("Tick %d - Raw buffer: %d, Filtered buffer: %d, Result: %ld\n",
                   t, raw_size, filtered_size, pipeline_data.final_result);
        }
        
        log_stage_activity(id, t, "Monitored pipeline");
        
        // Punto de sincronización
        pthread_barrier_wait(&barrier);
    }
    
    printf("Stage %ld (Monitor) completed\n", id);
    return nullptr;
}

void test_pipeline(int num_stages) {
    printf("Starting %d-stage pipeline for %d ticks\n", num_stages, TICKS);
    
    // Inicializar barrier para todas las etapas
    pthread_barrier_init(&barrier, NULL, num_stages);
    
    std::vector<pthread_t> threads(num_stages);
    double start = now_s();
    
    if (num_stages >= 3) {
        pthread_create(&threads[0], nullptr, stage_generator, (void*)1);
        pthread_create(&threads[1], nullptr, stage_filter, (void*)2);
        pthread_create(&threads[2], nullptr, stage_reducer, (void*)3);
        
        if (num_stages >= 4) {
            pthread_create(&threads[3], nullptr, stage_monitor, (void*)4);
        }
    }
    
    // Esperar a que todas las etapas terminen
    for (int i = 0; i < num_stages; i++) {
        pthread_join(threads[i], nullptr);
    }
    
    double end = now_s();
    
    printf("\nPipeline Results:\n");
    printf("Execution time: %.3fs\n", end - start);
    printf("Final result: %ld\n", pipeline_data.final_result);
    printf("Throughput: %.2f ticks/sec\n", TICKS / (end - start));
    
    // Cleanup
    pthread_barrier_destroy(&barrier);
    
    if (log_file) {
        fprintf(log_file, "Pipeline execution completed. Final result: %ld\n", pipeline_data.final_result);
        fclose(log_file);
        log_file = nullptr;
    }
}

// Pipeline alternativo sin barriers (usando colas)
struct QueuePipeline {
    std::vector<int> stage1_to_stage2;
    std::vector<int> stage2_to_stage3;
    pthread_mutex_t queue1_mutex;
    pthread_mutex_t queue2_mutex;
    pthread_cond_t queue1_cond;
    pthread_cond_t queue2_cond;
    bool pipeline_done;
    long result;
    
    QueuePipeline() : pipeline_done(false), result(0) {
        pthread_mutex_init(&queue1_mutex, nullptr);
        pthread_mutex_init(&queue2_mutex, nullptr);
        pthread_cond_init(&queue1_cond, nullptr);
        pthread_cond_init(&queue2_cond, nullptr);
    }
    
    ~QueuePipeline() {
        pthread_mutex_destroy(&queue1_mutex);
        pthread_mutex_destroy(&queue2_mutex);
        pthread_cond_destroy(&queue1_cond);
        pthread_cond_destroy(&queue2_cond);
    }
} queue_pipeline;

void* queue_producer(void* p) {
    std::mt19937 gen(1);
    std::uniform_int_distribution<> dis(1, 100);
    
    for (int i = 0; i < TICKS * BUFFER_SIZE; i++) {
        int data = dis(gen);
        
        pthread_mutex_lock(&queue_pipeline.queue1_mutex);
        queue_pipeline.stage1_to_stage2.push_back(data);
        pthread_cond_signal(&queue_pipeline.queue1_cond);
        pthread_mutex_unlock(&queue_pipeline.queue1_mutex);
        
        usleep(100); // Simular trabajo
    }
    
    pthread_mutex_lock(&queue_pipeline.queue1_mutex);
    queue_pipeline.pipeline_done = true;
    pthread_cond_broadcast(&queue_pipeline.queue1_cond);
    pthread_mutex_unlock(&queue_pipeline.queue1_mutex);
    
    printf("Queue Producer completed\n");
    return nullptr;
}

void* queue_filter(void* p) {
    while (true) {
        pthread_mutex_lock(&queue_pipeline.queue1_mutex);
        while (queue_pipeline.stage1_to_stage2.empty() && !queue_pipeline.pipeline_done) {
            pthread_cond_wait(&queue_pipeline.queue1_cond, &queue_pipeline.queue1_mutex);
        }
        
        if (queue_pipeline.stage1_to_stage2.empty() && queue_pipeline.pipeline_done) {
            pthread_mutex_unlock(&queue_pipeline.queue1_mutex);
            break;
        }
        
        int data = queue_pipeline.stage1_to_stage2.front();
        queue_pipeline.stage1_to_stage2.erase(queue_pipeline.stage1_to_stage2.begin());
        pthread_mutex_unlock(&queue_pipeline.queue1_mutex);
        
        // Filtrar
        if (data % 2 == 0 && data > 20) {
            pthread_mutex_lock(&queue_pipeline.queue2_mutex);
            queue_pipeline.stage2_to_stage3.push_back(data);
            pthread_cond_signal(&queue_pipeline.queue2_cond);
            pthread_mutex_unlock(&queue_pipeline.queue2_mutex);
        }
        
        usleep(50); // Simular trabajo
    }
    
    pthread_mutex_lock(&queue_pipeline.queue2_mutex);
    queue_pipeline.pipeline_done = true;
    pthread_cond_broadcast(&queue_pipeline.queue2_cond);
    pthread_mutex_unlock(&queue_pipeline.queue2_mutex);
    
    printf("Queue Filter completed\n");
    return nullptr;
}

void* queue_consumer(void* p) {
    while (true) {
        pthread_mutex_lock(&queue_pipeline.queue2_mutex);
        while (queue_pipeline.stage2_to_stage3.empty() && !queue_pipeline.pipeline_done) {
            pthread_cond_wait(&queue_pipeline.queue2_cond, &queue_pipeline.queue2_mutex);
        }
        
        if (queue_pipeline.stage2_to_stage3.empty() && queue_pipeline.pipeline_done) {
            pthread_mutex_unlock(&queue_pipeline.queue2_mutex);
            break;
        }
        
        int data = queue_pipeline.stage2_to_stage3.front();
        queue_pipeline.stage2_to_stage3.erase(queue_pipeline.stage2_to_stage3.begin());
        pthread_mutex_unlock(&queue_pipeline.queue2_mutex);
        
        queue_pipeline.result += data;
        usleep(25); // Simular trabajo
    }
    
    printf("Queue Consumer completed. Result: %ld\n", queue_pipeline.result);
    return nullptr;
}

void test_queue_pipeline() {
    printf("\n=== Queue-based Pipeline ===\n");
    
    pthread_t producer, filter, consumer;
    double start = now_s();
    
    pthread_create(&producer, nullptr, queue_producer, nullptr);
    pthread_create(&filter, nullptr, queue_filter, nullptr);
    pthread_create(&consumer, nullptr, queue_consumer, nullptr);
    
    pthread_join(producer, nullptr);
    pthread_join(filter, nullptr);
    pthread_join(consumer, nullptr);
    
    double end = now_s();
    
    printf("Queue Pipeline Results:\n");
    printf("Execution time: %.3fs\n", end - start);
    printf("Final result: %ld\n", queue_pipeline.result);
    printf("Throughput: %.2f items/sec\n", (TICKS * BUFFER_SIZE) / (end - start));
}

int main(int argc, char** argv) {
    int test_type = (argc > 1) ? std::atoi(argv[1]) : 1;
    
    switch (test_type) {
        case 1:
            test_pipeline(3);
            break;
        case 2:
            test_pipeline(4); // Incluir etapa de monitoreo
            break;
        case 3:
            test_queue_pipeline();
            break;
        default:
            printf("Usage: %s <test_type>\n", argv[0]);
            printf("  1: 3-stage barrier pipeline\n");
            printf("  2: 4-stage pipeline with monitor\n");
            printf("  3: Queue-based pipeline\n");
            printf("\nRunning all tests...\n");
            
            test_pipeline(3);
            
            // Reset para el siguiente test
            pipeline_data.raw_data.clear();
            pipeline_data.filtered_data.clear();
            pipeline_data.processed_data.clear();
            pipeline_data.final_result = 0;
            
            // Reinicializar once_flag correctamente
            static pthread_once_t new_once_flag = PTHREAD_ONCE_INIT;
            once_flag = new_once_flag;
            
            test_pipeline(4);
            test_queue_pipeline();
            break;
    }
    
    return 0;
}
