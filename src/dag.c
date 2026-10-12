#include <stdio.h>
#include <string.h>
#include "dag.h"

void dag_init(PipelineDAG *dag)
{
    if (dag != NULL)
        memset(dag, 0, sizeof(*dag));
}

int dag_add_edge(PipelineDAG *dag, int from, int to)
{
    if (dag == NULL ||
        from < 0 || to < 0 ||
        from >= dag->pipeline.count ||
        to >= dag->pipeline.count ||
        from == to ||
        dag->edge_count < 0 ||
        dag->edge_count >= MAX_EDGES)
        return -1;

    for (int i = 0; i < dag->edge_count; i++) {
        if (dag->edges[i].from == from &&
            dag->edges[i].to == to)
            return -1;
    }

    dag->edges[dag->edge_count].from = from;
    dag->edges[dag->edge_count].to = to;
    dag->edge_count++;

    return 0;
}

static int visit(const PipelineDAG *dag, int node,
                 int state[MAX_STAGES])
{
    state[node] = 1;

    for (int i = 0; i < dag->edge_count; i++) {
        if (dag->edges[i].from != node)
            continue;

        int next = dag->edges[i].to;

        if (state[next] == 1)
            return -1;

        if (state[next] == 0 &&
            visit(dag, next, state) != 0)
            return -1;
    }

    state[node] = 2;
    return 0;
}

int dag_validate(const PipelineDAG *dag)
{
    if (dag == NULL ||
        dag->pipeline.count <= 0 ||
        dag->pipeline.count > MAX_STAGES ||
        dag->edge_count < 0 ||
        dag->edge_count > MAX_EDGES)
        return -1;

    for (int i = 0; i < dag->edge_count; i++) {
        int from = dag->edges[i].from;
        int to = dag->edges[i].to;

        if (from < 0 || to < 0 ||
            from >= dag->pipeline.count ||
            to >= dag->pipeline.count ||
            from == to)
            return -1;

        for (int j = 0; j < i; j++) {
            if (dag->edges[j].from == from &&
                dag->edges[j].to == to)
                return -1;
        }
    }

    int state[MAX_STAGES] = {0};

    for (int i = 0; i < dag->pipeline.count; i++) {
        if (state[i] == 0 && visit(dag, i, state) != 0) {
            fprintf(stderr, "DAG contains a cycle\n");
            return -1;
        }
    }

    return 0;
}
