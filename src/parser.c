#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

int parse_pipeline(const char *line, Pipeline *pipeline)
{
    char *copy;
    char *stage_text;
    char *save_stage;

    pipeline->count = 0;

    copy = strdup(line);

    if (copy == NULL) {
        perror("strdup");
        return -1;
    }

    stage_text = strtok_r(copy, "|", &save_stage);

    while (stage_text != NULL) {

        if (pipeline->count >= MAX_STAGES) {
            fprintf(stderr, "Too many stages\n");
            free(copy);
            return -1;
        }

        Stage *stage = &pipeline->stages[pipeline->count];
        stage->argc = 0;

        char *save_arg;
        char *arg = strtok_r(stage_text, " \t\r\n", &save_arg);

        while (arg != NULL) {

            if (stage->argc >= MAX_ARGS - 1) {
                fprintf(stderr, "Too many arguments\n");
                free(copy);
                return -1;
            }

            stage->argv[stage->argc] = strdup(arg);

            if (stage->argv[stage->argc] == NULL) {
                perror("strdup");
                free(copy);
                return -1;
            }

            stage->argc++;
            arg = strtok_r(NULL, " \t\r\n", &save_arg);
        }

        stage->argv[stage->argc] = NULL;

        if (stage->argc > 0) {
            pipeline->count++;
        }

        stage_text = strtok_r(NULL, "|", &save_stage);
    }

    free(copy);

    return pipeline->count;
}

void free_pipeline(Pipeline *pipeline)
{
    for (int i = 0; i < pipeline->count; i++) {

        for (int j = 0; j < pipeline->stages[i].argc; j++) {
            free(pipeline->stages[i].argv[j]);
            pipeline->stages[i].argv[j] = NULL;
        }

        pipeline->stages[i].argc = 0;
    }

    pipeline->count = 0;
}

