#ifndef PARSER_H
#define PARSER_H

#define MAX_STAGES 32
#define MAX_ARGS 16

typedef struct {
    char *argv[MAX_ARGS];
    int argc;
} Stage;

typedef struct {
    Stage stages[MAX_STAGES];
    int count;
} Pipeline;

int parse_pipeline(const char *line, Pipeline *pipeline);
void free_pipeline(Pipeline *pipeline);

#endif

