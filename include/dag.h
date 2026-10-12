#ifndef DAG_H
#define DAG_H

#include "parser.h"

#define MAX_EDGES (MAX_STAGES * MAX_STAGES)

typedef struct {
    int from;
    int to;
} DagEdge;

typedef struct {
    Pipeline pipeline;
    DagEdge edges[MAX_EDGES];
    int edge_count;
} PipelineDAG;

void dag_init(PipelineDAG *dag);
int dag_add_edge(PipelineDAG *dag, int from, int to);
int dag_validate(const PipelineDAG *dag);

#endif
