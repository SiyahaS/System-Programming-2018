//
// Created by siyahas on 15.04.2018.
//

#include "dynamic.h"
#include "reader.h"
#include "debug.h"
#include <stdbool.h>

#ifndef SYSTEM_HW3_SPLITTER_H
#define SYSTEM_HW3_SPLITTER_H


enum token_type {
    KEYWORD, STRING, WORD, PIPE, IN, OUT, ENDL
};

enum keyword {
    LS, PWD, CD, HELP, CAT, WC, EXIT
};

char *strKeyword(enum keyword keyword);

char *strTokenType(enum token_type type);

struct token {
    enum token_type type;
    union {
        enum keyword keyword;
        char *string;
        char *word;
    } data;
};

typedef struct tokenizer {
    struct dynamic lineBuffer;
    struct reader *reader;
    char last;
} *tokenizer_t;

void tokenizer_create(tokenizer_t tokenizer, reader_t reader);

struct token tokenizer_next(tokenizer_t tokenizer);

void tokenizer_nextLine(tokenizer_t tokenizer);

void tokenizer_destroy(tokenizer_t tokenizer);

#endif // SYSTEM_HW3_SPLITTER_H