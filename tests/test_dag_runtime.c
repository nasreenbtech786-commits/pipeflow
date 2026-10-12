#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dag_runtime.h"

static int add_stage(PipelineDAG *dag, const char *command)
{
    Pipeline parsed = {0};
    int rc = parse_pipeline(command, &parsed);

    if (rc <= 0 || parsed.count != 1) {
        fprintf(stderr, "Cannot parse stage: %s (rc=%d, count=%d)\n",
                command, rc, parsed.count);
        free_pipeline(&parsed);
        return -1;
    }

    if (dag->pipeline.count >= MAX_STAGES) {
        free_pipeline(&parsed);
        return -1;
    }

    int index = dag->pipeline.count++;
    dag->pipeline.stages[index] = parsed.stages[0];

    memset(&parsed.stages[0], 0, sizeof(parsed.stages[0]));
    parsed.count = 0;

    return index;
}

static int run_fanout(void)
{
    PipelineDAG dag = {0};
    dag_init(&dag);

    int source = add_stage(&dag, "stages/file_source tests/input.txt");
    int count1 = add_stage(&dag, "stages/count");
    int count2 = add_stage(&dag, "stages/count");

    if (source < 0 || count1 < 0 || count2 < 0)
        goto setup_error;

    if (dag_add_edge(&dag, source, count1) != 0 ||
        dag_add_edge(&dag, source, count2) != 0)
        goto setup_error;

    puts("\n=== FAN-OUT TEST ===");
    puts("One source -> two consumers");
    fflush(stdout);

    int result = dag_execute(&dag);
    free_pipeline(&dag.pipeline);

    return result == 0 ? 0 : 1;

setup_error:
    fprintf(stderr, "Failed to configure fan-out graph\n");
    free_pipeline(&dag.pipeline);
    return 1;
}

static int run_fanin(void)
{
    PipelineDAG dag = {0};
    dag_init(&dag);

    int source1 = add_stage(&dag, "stages/file_source tests/input.txt");
    int source2 = add_stage(&dag, "stages/file_source tests/input.txt");
    int sink = add_stage(&dag, "stages/count");

    if (source1 < 0 || source2 < 0 || sink < 0)
        goto setup_error;

    if (dag_add_edge(&dag, source1, sink) != 0 ||
        dag_add_edge(&dag, source2, sink) != 0)
        goto setup_error;

    puts("\n=== FAN-IN TEST ===");
    puts("Two sources -> one consumer");
    fflush(stdout);

    int result = dag_execute(&dag);
    free_pipeline(&dag.pipeline);

    return result == 0 ? 0 : 1;

setup_error:
    fprintf(stderr, "Failed to configure fan-in graph\n");
    free_pipeline(&dag.pipeline);
    return 1;
}


static int run_failure(void)
{
    PipelineDAG dag = {0};
    dag_init(&dag);

    /* This command must not exist. */
    int bad = add_stage(&dag, "pipeflow_command_that_does_not_exist");

    if (bad < 0) {
        fprintf(stderr, "Could not configure failure test\n");
        free_pipeline(&dag.pipeline);
        return 1;
    }

    puts("\n=== FAILURE-INJECTION TEST ===");
    puts("A nonexistent stage command should make DAG execution fail.");
    fflush(stdout);

    int result = dag_execute(&dag);
    free_pipeline(&dag.pipeline);

    if (result != 0) {
        puts("PASS: DAG execution reported failure.");
        return 0;
    }

    fprintf(stderr, "FAIL: DAG execution reported success for a missing command.\n");
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s fanout|fanin|failure\n", argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "fanout") == 0)
        return run_fanout();

    if (strcmp(argv[1], "fanin") == 0)
        return run_fanin();

    if (strcmp(argv[1], "failure") == 0)
        return run_failure();

    fprintf(stderr, "Unknown test: %s\n", argv[1]);
    return 2;
}
