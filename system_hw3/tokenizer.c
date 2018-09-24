//
// Created by siyahas on 15.04.2018.
//

#include "dynamic.h"
#include "reader.h"
#include "tokenizer.h"
#include <string.h>

char *strKeyword(enum keyword keyword){
    if(keyword == PWD)
        return "PWD";
    else if(keyword == CD)
        return "CD";
    else if(keyword == HELP)
        return "HELP";
    else if(keyword == EXIT)
        return "EXIT";
    return NULL;
}

char *strTokenType(enum token_type type){
    //KEYWORD, STRING, WORD, PIPE, IN, OUT, ENDL
    if(type == KEYWORD)
        return "KEYWORD";
    else if(type == STRING)
        return "STRING";
    else if(type == WORD)
        return "WORD";
    else if(type == PIPE)
        return "PIPE";
    else if(type == IN)
        return "IN";
    else if(type == OUT)
        return "OUT";
    else if(type == ENDL)
        return "ENDL";

    return NULL;
}

void tokenizer_create(tokenizer_t tokenizer, reader_t reader) {
    tokenizer->reader = reader;
    dynamic_create(&tokenizer->lineBuffer);
    tokenizer->last = false;
}

struct token tokenizer_next(tokenizer_t tokenizer) {
    if (tokenizer->last == '\n') {
        tokenizer->last = '\0';
        return (struct token) {.type = ENDL};
    } else if (tokenizer->last == '|') {
        tokenizer->last = '\0';
        return (struct token) {.type = PIPE};
    } else if (tokenizer->last == '<') {
        tokenizer->last = '\0';
        return (struct token) {.type = IN};
    } else if (tokenizer->last == '>') {
        tokenizer->last = '\0';
        return (struct token) {.type = OUT};
    }

    char c;
    char *data;
    size_t start;
    do {
        c = reader_getc(tokenizer->reader);
        DERROR("[TOKENIZER][NEXT] %d\n", c);
    } while (c == ' ');
    dynamic_add(&tokenizer->lineBuffer, &c, sizeof(c));

    dynamic_get(&tokenizer->lineBuffer, (void **) &data, &start);
    start -= 1;

    if (c == '"') { // STRING
        while ((c = reader_getc(tokenizer->reader))) {
            DERROR("[TOKENIZER][STRING] %d\n", c);
            dynamic_add(&tokenizer->lineBuffer, &c, sizeof(c));
            if (c == '"') {
                size_t size;
                dynamic_get(&tokenizer->lineBuffer, (void **) &data, &size);
                if (data[size - 2] != '\\')
                    break;
            }
        }
        dynamic_add(&tokenizer->lineBuffer, "\0", sizeof(char));
        DERROR("[TOKENIZER][STRING] %s\n", &data[start]);
        return (struct token) {.type = STRING, .data.string = &data[start]};
    } else if (c == '|') { // PIPE
        DERROR("[TOKENIZER][PIPE]\n");
        return (struct token) {.type = PIPE};
    } else if (c == '<') { // IN
        DERROR("[TOKENIZER][IN]\n");
        return (struct token) {.type = IN};
    } else if (c == '>') { // OUT
        DERROR("[TOKENIZER][OUT]\n");
        return (struct token) {.type = OUT};
    } else if (c == '\n') { // ENDL
        DERROR("[TOKENIZER][ENDL]\n");
        return (struct token) {.type = ENDL};
    }

    // WORD or KEYWORD
    while ((c = reader_getc(tokenizer->reader))) {
        DERROR("[TOKENIZER][(KEY)WORD] %d\n", c);
        if (c == ' ') {
            size_t size;
            dynamic_get(&tokenizer->lineBuffer, (void **) &data, &size);
            if (data[size - 1] != '\\') {
                dynamic_add(&tokenizer->lineBuffer, "\0", sizeof(char));
                break;
            } else {
                dynamic_add(&tokenizer->lineBuffer, &c, sizeof(c));
            }
        } else if (c == '\n' || c == '|' || c == '<' || c == '>') {
            dynamic_add(&tokenizer->lineBuffer, "\0", sizeof(char));
            tokenizer->last = c;
            break;
        } else {
            dynamic_add(&tokenizer->lineBuffer, &c, sizeof(c));
        }
    }

    // KEYWORD ?
    if (strncmp(&data[start], "pwd", 3) == 0) { // PWD
        DERROR("[TOKENIZER][PWD]\n");
        return (struct token) {.type = KEYWORD, .data.keyword = PWD};
    } else if (strncmp(&data[start], "cd", 2) == 0) { // CD
        DERROR("[TOKENIZER][CD]\n");
        return (struct token) {.type = KEYWORD, .data.keyword = CD};
    } else if (strncmp(&data[start], "help", 4) == 0) { // HELP
        DERROR("[TOKENIZER][HELP]\n");
        return (struct token) {.type = KEYWORD, .data.keyword = HELP};
    } else if (strncmp(&data[start], "exit", 4) == 0) { // EXIT
        DERROR("[TOKENIZER][EXIT]\n");
        return (struct token) {.type = KEYWORD, .data.keyword = EXIT};
    }

    // WORD
    dynamic_add(&tokenizer->lineBuffer, "\0", sizeof(char));
    DERROR("[TOKENIZER][WORD] %s\n", &data[start]);
    return (struct token) {.type = WORD, .data.word = &data[start]};
}

void tokenizer_nextLine(tokenizer_t tokenizer) {
    char *data;
    size_t size;
    dynamic_get(&tokenizer->lineBuffer, (void **) &data, &size);
    if (size == 0 || data[size - 1] != '\n') {
        while (reader_getc(tokenizer->reader) != '\n');
    }
    dynamic_clear(&tokenizer->lineBuffer);
}

void tokenizer_destroy(tokenizer_t tokenizer) {
    dynamic_destroy(&tokenizer->lineBuffer);
}