CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -Iinclude

ENGINE = pipeflow
ENGINE_SOURCES = src/main.c src/parser.c

STAGE_SOURCES = $(wildcard stages/*.c)
STAGE_PROGRAMS = $(STAGE_SOURCES:.c=)

.PHONY: all clean test

all: $(ENGINE) $(STAGE_PROGRAMS) basic_pipe

$(ENGINE): $(ENGINE_SOURCES) include/parser.h
	$(CC) $(CFLAGS) $(ENGINE_SOURCES) -o $(ENGINE)

stages/%: stages/%.c
	$(CC) -Wall -Wextra -std=gnu11 $< -o $@

basic_pipe: src/basic_pipe.c
	$(CC) -Wall -Wextra -std=gnu11 $< -o $@

clean:
	rm -f $(ENGINE) $(STAGE_PROGRAMS) basic_pipe *.o

test: all
	./pipeflow configs/basic.conf

