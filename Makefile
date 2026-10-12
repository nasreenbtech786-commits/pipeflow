CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -Iinclude

ENGINE = pipeflow
ENGINE_SOURCES = src/main.c src/parser.c src/dag.c src/dag_runtime.c

STAGE_SOURCES = $(wildcard stages/*.c)
STAGE_PROGRAMS = $(STAGE_SOURCES:.c=)

.PHONY: all clean test

# Build the engine, all stages, and the basic pipe demonstration.
all: $(ENGINE) $(STAGE_PROGRAMS) basic_pipe

# Build the PipeFlow engine.
$(ENGINE): $(ENGINE_SOURCES) include/parser.h include/dag.h
	$(CC) $(CFLAGS) $(ENGINE_SOURCES) -o $(ENGINE)

# Build every stage in the stages directory.
stages/%: stages/%.c
	$(CC) $(CFLAGS) $< -o $@

# Build the basic pipe demonstration.
basic_pipe: src/basic_pipe.c
	$(CC) $(CFLAGS) $< -o $@

# Remove generated binaries and object files.
clean:
	rm -f $(ENGINE) $(STAGE_PROGRAMS) basic_pipe *.o

# Build everything and run the basic configuration test.
test: all
	./pipeflow configs/basic.conf

# Run a linear pipeline demonstration.
.PHONY: demo
demo: all
	printf 'file_source tests/input.txt | transform | count\n' > /tmp/pipeflow_demo.conf
	./pipeflow /tmp/pipeflow_demo.conf

# Demonstrate early termination and SIGPIPE handling.
.PHONY: sigpipe
sigpipe: all
	./pipeflow configs/basic.conf

# Demonstrate backpressure with a slow consumer.
.PHONY: backpressure
backpressure: all
	printf 'benchmark_source | slow_sink\n' > /tmp/pipeflow_backpressure.conf
	time ./pipeflow /tmp/pipeflow_backpressure.conf


.PHONY: test-dag

test-dag:
	gcc -Wall -Wextra -std=gnu11 -Iinclude \
	    tests/test_dag.c src/dag.c -o /tmp/pipeflow_test_dag
	/tmp/pipeflow_test_dag

.PHONY: test-dag-runtime

test-dag-runtime: all
	gcc $(CFLAGS) tests/test_dag_runtime.c src/dag_runtime.c src/dag.c src/parser.c -o /tmp/test_dag_runtime
	timeout 15s /tmp/test_dag_runtime fanout
	timeout 15s /tmp/test_dag_runtime fanin
	timeout 15s /tmp/test_dag_runtime failure
