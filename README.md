# Lab06: Acceso a Recursos Compartidos con Pthreads

**Autor:** Fatima Navarro  
**Carnet:** 24044  
**Curso:** CC3086 Programación de Microprocesadores  
**Universidad del Valle de Guatemala**

## Descripción

Implementación de 5 prácticas progresivas sobre sincronización y acceso seguro a recursos compartidos usando POSIX Threads:

1. **Race Conditions** - Contadores compartidos con diferentes estrategias
2. **Buffer Circular** - Productor/consumidor con condition variables  
3. **Readers/Writers** - Comparación rwlock vs mutex
4. **Deadlock Prevention** - Técnicas de prevención y detección
5. **Pipeline** - Sincronización con barriers y pthread_once

## Compilación

```bash
# Compilar todo
make all

# Ejecutar tests
make run-tests

# Benchmarks de rendimiento
make benchmark
```

## Adaptaciones para macOS

- Implementación manual de `pthread_barrier_t`
- Timeout custom para detección de deadlocks
- Optimizaciones para Apple Silicon

## Estructura

```
Lab06/
├── src/           # Código fuente (.cpp)
├── bin/           # Ejecutables (generado)
├── include/       # Headers compartidos
├── data/          # Resultados CSV (generado)
├── docs/          # Documentación
├── scripts/       # Scripts de automatización
└── Makefile       # Build system
```

## Ejecución Individual

```bash
./bin/p1_counter [threads] [iterations]
./bin/p2_ring [producers] [consumers] [items_per_producer]  
./bin/p3_rw [threads] [operations_per_thread]
./bin/p4_deadlock [test_type: 1-4]
./bin/p5_pipeline [test_type: 1-3]
```

## Herramientas de Validación

```bash
# ThreadSanitizer (detectar race conditions)
make tsan
./bin/p1_counter_tsan 2 100000

# AddressSanitizer (detectar memory issues)
make asan
./bin/p2_ring_asan 1 1 1000
```

## Resultados Destacados

- **Sharded counters:** 5x mejor performance que mutex
- **RWLocks:** 39% mejora con 90% lecturas
- **Deadlock prevention:** 100% éxito con ordenación global
- **Pipeline throughput:** 30.8 ticks/sec con barriers

## Video Demo

https://youtu.be/7Atx4ZyAKao

## Licencia

Proyecto académico - Universidad del Valle de Guatemala
