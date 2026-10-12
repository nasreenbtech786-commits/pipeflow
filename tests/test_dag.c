#include <stdio.h>
#include "dag.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void check(const char *name, int condition)
{
    if (condition) {
        printf("[PASS] %s\n", name);
        tests_passed++;
    } else {
        printf("[FAIL] %s\n", name);
        tests_failed++;
    }
}

int main(void)
{
    PipelineDAG dag;

    /* Test 1: Initialization */
    dag_init(&dag);
    check("DAG initialization", dag.pipeline.count == 0 &&
          dag.edge_count == 0);

    /* Test 2: Valid graph */
    dag.pipeline.count = 3;
    check("Add edge 0 -> 1", dag_add_edge(&dag, 0, 1) == 0);
    check("Add edge 0 -> 2", dag_add_edge(&dag, 0, 2) == 0);
    check("Validate fan-out graph", dag_validate(&dag) == 0);

    /* Test 3: Duplicate edge rejection */
    check("Reject duplicate edge", dag_add_edge(&dag, 0, 1) == -1);

    /* Test 4: Invalid nodes */
    check("Reject negative source", dag_add_edge(&dag, -1, 1) == -1);
    check("Reject out-of-range destination",
          dag_add_edge(&dag, 0, 3) == -1);

    /* Test 5: Self-loop rejection */
    check("Reject self-loop", dag_add_edge(&dag, 1, 1) == -1);

    /* Test 6: Cycle detection */
    check("Add edge 1 -> 2", dag_add_edge(&dag, 1, 2) == 0);
    check("Add edge 2 -> 0", dag_add_edge(&dag, 2, 0) == 0);
    check("Reject cyclic graph", dag_validate(&dag) == -1);

    /* Test 7: Disconnected acyclic graph */
    dag_init(&dag);
    dag.pipeline.count = 4;
    dag_add_edge(&dag, 0, 1);
    dag_add_edge(&dag, 2, 3);
    check("Accept disconnected acyclic graph",
          dag_validate(&dag) == 0);

    printf("\nDAG tests: %d passed, %d failed\n",
           tests_passed, tests_failed);

    return tests_failed == 0 ? 0 : 1;
}
