# Makefile para Lab06 - Pthreads
# Autor: Fatima Navarro
# Carnet: 24044
# Fecha: 29/08/2025

# Compiladores
CXX = clang++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra -pthread
DEBUG_FLAGS = -O0 -g -DDEBUG
TSAN_FLAGS = -O1 -g -fsanitize=thread -fno-omit-frame-pointer
ASAN_FLAGS = -O1 -g -fsanitize=address -fno-omit-frame-pointer

# Directorios
BIN = bin
SRC = src
INCLUDE = include
SCRIPTS = scripts
DATA = data
DOCS = docs

# Archivos fuente
SOURCES = $(wildcard $(SRC)/*.cpp)
# Nombres de ejecutables
EXECUTABLES = $(patsubst $(SRC)/%.cpp,$(BIN)/%,$(SOURCES))
# Versiones debug
DEBUG_EXECUTABLES = $(patsubst $(SRC)/%.cpp,$(BIN)/%_debug,$(SOURCES))
# Versiones con sanitizers
TSAN_EXECUTABLES = $(patsubst $(SRC)/%.cpp,$(BIN)/%_tsan,$(SOURCES))
ASAN_EXECUTABLES = $(patsubst $(SRC)/%.cpp,$(BIN)/%_asan,$(SOURCES))

.PHONY: all clean debug sanitizers tsan asan run-tests help dirs

# Target por defecto
all: dirs $(EXECUTABLES)

# Crear directorios necesarios
dirs:
	mkdir -p $(BIN) $(SCRIPTS) $(DATA) $(DOCS)

# Compilación regular
$(BIN)/%: $(SRC)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) $< -o $@

# Versiones debug
debug: dirs $(DEBUG_EXECUTABLES)

$(BIN)/%_debug: $(SRC)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $< -o $@

# ThreadSanitizer (para detectar race conditions)
tsan: dirs $(TSAN_EXECUTABLES)

$(BIN)/%_tsan: $(SRC)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) $(TSAN_FLAGS) $< -o $@

# AddressSanitizer (para detectar errores de memoria)
asan: dirs $(ASAN_EXECUTABLES)

$(BIN)/%_asan: $(SRC)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) $(ASAN_FLAGS) $< -o $@

# Construir todas las versiones con sanitizers
sanitizers: tsan asan

# Limpiar archivos generados
clean:
	rm -rf $(BIN)
	rm -f *.log
	rm -f pipeline.log
	rm -f core.*
	rm -f $(DATA)/*.csv

# Tests individuales
test-p1:
	@echo "=== Testing Practice 1 (Counter) ==="
	@./$(BIN)/p1_counter 4 1000000

test-p2:
	@echo "=== Testing Practice 2 (Ring Buffer) ==="
	@./$(BIN)/p2_ring 2 2 10000

test-p3:
	@echo "=== Testing Practice 3 (Readers/Writers) ==="
	@./$(BIN)/p3_rw 4 50000

test-p4-safe:
	@echo "=== Testing Practice 4 (Deadlock Prevention) ==="
	@echo "Testing ordered locks:"
	@./$(BIN)/p4_deadlock 2
	@echo "Testing trylock with backoff:"
	@./$(BIN)/p4_deadlock 3
	@echo "Testing bank transfer simulation:"
	@./$(BIN)/p4_deadlock 4

test-p4-deadlock:
	@echo "=== Testing Practice 4 (Intentional Deadlock) ==="
	@echo "WARNING: This will demonstrate a deadlock and exit after 5 seconds"
	@./$(BIN)/p4_deadlock 1 || echo "Deadlock successfully demonstrated"

test-p5:
	@echo "=== Testing Practice 5 (Pipeline) ==="
	@./$(BIN)/p5_pipeline 1

# Ejecutar todos los tests seguros
run-tests: all test-p1 test-p2 test-p3 test-p4-safe test-p5
	@echo "All tests completed successfully!"

# Benchmarks de rendimiento
benchmark: all
	@echo "=== Performance Benchmarks ==="
	@echo "Counter (4 threads, 5M ops):"
	@./$(BIN)/p1_counter 4 5000000
	@echo ""
	@echo "Ring Buffer (3 producers, 3 consumers, 50K items):"
	@./$(BIN)/p2_ring 3 3 50000
	@echo ""
	@echo "Readers/Writers (6 threads, 500K ops):"
	@./$(BIN)/p3_rw 6 500000
	@echo ""
	@echo "Pipeline (4 stages):"
	@./$(BIN)/p5_pipeline 2

# Test de escalabilidad
scalability-test: all
	@echo "=== Scalability Test - Counter ==="
	@echo "threads,time,ops_per_sec" > $(DATA)/scalability.csv
	@for threads in 1 2 4 8; do \
		echo "Testing with $$threads threads..."; \
		./$(BIN)/p1_counter $$threads 2000000 | tail -1; \
		echo ""; \
	done

# Test con diferentes configuraciones
config-test: all
	@echo "=== Configuration Tests ==="
	@echo "Testing ring buffer with different producer/consumer ratios:"
	@for ratio in "1 1" "1 2" "2 1" "2 2" "3 2"; do \
		set -- $$ratio; \
		echo "Producers: $$1, Consumers: $$2"; \
		./$(BIN)/p2_ring $$1 $$2 20000; \
		echo ""; \
	done

# Tests con sanitizers (para debugging)
test-tsan: tsan
	@echo "=== Running ThreadSanitizer Tests ==="
	@echo "Testing p1_counter for race conditions:"
	@./$(BIN)/p1_counter_tsan 2 100000 || echo "Race conditions detected (expected)"

test-asan: asan
	@echo "=== Running AddressSanitizer Tests ==="
	@echo "Testing memory safety:"
	@./$(BIN)/p2_ring_asan 1 1 1000

# Generar datos para análisis
generate-data: all
	@echo "=== Generating Performance Data ==="
	@mkdir -p $(DATA)
	@echo "scenario,threads,operations,time,throughput" > $(DATA)/performance.csv
	@echo "Generating counter data..."
	@for t in 1 2 4 8; do \
		for ops in 1000000 2000000; do \
			result=$$(./$(BIN)/p1_counter $$t $$ops | grep MUTEX | head -1); \
			echo "counter_mutex,$$t,$$ops,$$result" >> $(DATA)/performance_raw.txt; \
		done; \
	done
	@echo "Performance data saved to $(DATA)/"

# Crear scripts de ejecución
create-scripts: dirs
	@echo "#!/bin/bash" > $(SCRIPTS)/run_all_tests.sh
	@echo "# Script generado automáticamente" >> $(SCRIPTS)/run_all_tests.sh
	@echo "echo 'Ejecutando todas las prácticas...'" >> $(SCRIPTS)/run_all_tests.sh
	@echo "make run-tests" >> $(SCRIPTS)/run_all_tests.sh
	@chmod +x $(SCRIPTS)/run_all_tests.sh
	@echo "Script creado: $(SCRIPTS)/run_all_tests.sh"

# Verificar que todos los programas compilan
verify: all
	@echo "=== Verification Tests ==="
	@for prog in $(EXECUTABLES); do \
		if [ -x "$$prog" ]; then \
			echo "✓ $$prog compiled successfully"; \
		else \
			echo "✗ $$prog failed to compile"; \
			exit 1; \
		fi; \
	done
	@echo "All programs compiled successfully!"

# Información de sistema y compilador
info:
	@echo "=== System Information ==="
	@echo "OS: $$(uname -s) $$(uname -r)"
	@echo "Architecture: $$(uname -m)"
	@echo "Compiler: $(CXX)"
	@$(CXX) --version
	@echo "CPU cores: $$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 'unknown')"
	@echo "Available memory: $$(sysctl -n hw.memsize 2>/dev/null | awk '{print int($$1/1024/1024/1024) " GB"}' || echo 'unknown')"

# Ayuda
help:
	@echo "Makefile para Lab06 - Programación con Pthreads"
	@echo "Autor: Fatima Navarro (24044)"
	@echo ""
	@echo "Targets disponibles:"
	@echo "  all              - Compilar todos los programas (default)"
	@echo "  debug            - Compilar versiones debug"
	@echo "  tsan             - Compilar con ThreadSanitizer"
	@echo "  asan             - Compilar con AddressSanitizer" 
	@echo "  sanitizers       - Compilar todas las versiones con sanitizers"
	@echo "  clean            - Limpiar archivos generados"
	@echo ""
	@echo "Testing:"
	@echo "  run-tests        - Ejecutar todos los tests seguros"
	@echo "  test-p[1-5]      - Ejecutar test individual"
	@echo "  test-p4-deadlock - Demostrar deadlock intencional"
	@echo "  benchmark        - Ejecutar benchmarks de rendimiento"
	@echo "  scalability-test - Test de escalabilidad"
	@echo "  test-tsan        - Tests con ThreadSanitizer"
	@echo "  test-asan        - Tests con AddressSanitizer"
	@echo ""
	@echo "Utilidades:"
	@echo "  verify           - Verificar que todo compila"
	@echo "  info             - Información del sistema"
	@echo "  generate-data    - Generar datos de rendimiento"
	@echo "  create-scripts   - Crear scripts de ejecución"
	@echo ""
	@echo "Programas individuales:"
	@echo "  ./$(BIN)/p1_counter [threads] [iterations]"
	@echo "  ./$(BIN)/p2_ring [producers] [consumers] [items_per_producer]"
	@echo "  ./$(BIN)/p3_rw [threads] [operations_per_thread]"
	@echo "  ./$(BIN)/p4_deadlock [test_type: 1-4]"
	@echo "  ./$(BIN)/p5_pipeline [test_type: 1-3]"

# Demo completo
demo: all
	@echo "=== Lab06 Complete Demo ==="
	@echo "Ejecutando demostración completa de todas las prácticas..."
	@echo ""
	@make test-p1
	@echo ""
	@make test-p2
	@echo ""
	@make test-p3
	@echo ""
	@make test-p4-safe
	@echo ""
	@make test-p5
	@echo ""
	@echo "=== Demo completed successfully! ==="
