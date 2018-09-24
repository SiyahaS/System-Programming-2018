#include "reader.h"
#include "tokenizer.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

void help() {
    printf("These shell commands are defined internally. Type 'help' to see this list.\n"
                   "Type 'help name' to find out more about the function 'name'.\n"
                   " pwd\n"
                   " cd path\n"
                   " help command\n"
                   " exit\n");
}

void help_pwd() {
    printf("pwd: pwd\n"
                   "\tPrint the name of the current working directory.\n"
                   "\tExit Status:\n"
                   "\tReturns 0 unless the current directory cannot be read.\n");
}

void help_cd() {
    printf("cd: cd [dir]\n"
                   "\tChange the shell working directory.\n"
                   ".......\n");
}

struct builtin {
    enum keyword keyword;
    char *args[100];
    char *inforward;
    char *outforward;
    bool pipe[2];
};

struct executable {
    char *path;
    char *args[100];
    char *inforward;
    char *outforward;
    bool pipe[2]; // pipe[0]: in && pipe[1]: out
};

enum command_type {
    EXECUTABLE = 1,
    BUILTIN
};

struct command {
    enum command_type type;
    union {
        struct executable executable;
        struct builtin builtin;
    } data;
};

void builtin(struct builtin builtin, int pin, int pout, int fin, int fout) {

}

bool execIt(struct executable executable, int pin, int pout, int fin, int fout) {
    switch (fork()){
        case 0:
            // child
            break;
        case -1:
            // error
            DERROR("FORK(%s): %s\n", executable.path, strerror(errno));
            perror("FORK");
            return true;
        default:
            // parent
            return false; // just go back to execution
    }
    if(pin != -1) {
        dup2(pin, STDIN_FILENO);
        close(pin);
    }

    if(pout != -1){
        dup2(pout, STDOUT_FILENO);
        close(pout);
    }

    if(fin != -1){
        dup2(fin, STDIN_FILENO);
        close(fin);
    }

    if(fout != -1){
        dup2(fout, STDOUT_FILENO);
        close(fout);
    }
    execvp(executable.path, executable.args);
    perror("EXECVP");
    _exit(1);
}

int parseCommand(struct tokenizer tokenizer, struct command *command, struct token *last) {
    struct token token;
    enum {
        INITIAL, ARGS, SIN, SOUT
    } state = INITIAL;
    int argc = 0;

    while ((token = tokenizer_next(&tokenizer)).type != ENDL && token.type != PIPE) {
        if (state == INITIAL) {
            if (token.type == STRING || token.type == WORD) {
                command->type = EXECUTABLE;
                command->data.executable.path = token.data.string; // token.data.string and token.data.word are equivalent.
                command->data.executable.args[argc++] = token.data.string;
                DERROR("EXECUTABLE: %s\n", token.data.string);
            } else if (token.type == KEYWORD) {
                command->type = BUILTIN;
                command->data.builtin.keyword = token.data.keyword;
                command->data.builtin.args[argc++] = token.data.string;
                DERROR("BUILTIN: %s\n", strKeyword(token.data.keyword));
            } else {
                DERROR("INVALID FIRST TOKEN: %s\n", strTokenType(token.type));
                return 1;
            }
            state = ARGS;
        } else if (state == SIN) {
            if (command->type == EXECUTABLE)
                command->data.executable.inforward = token.data.string;
            else if (command->type == BUILTIN)
                command->data.builtin.inforward = token.data.string;
            DERROR("INFORWARD: %s", token.data.string);
            state = ARGS;
        } else if (state == SOUT) {
            if (command->type == EXECUTABLE)
                command->data.executable.outforward = token.data.string;
            else if (command->type == BUILTIN)
                command->data.builtin.outforward = token.data.string;
            DERROR("OUTFORWARD: %s", token.data.string);
            state = ARGS;
        } else if (state == ARGS) {
            if (token.type == IN)
                state = SIN;
            else if (token.type == OUT)
                state = SOUT;
            else {
                if (command->type == EXECUTABLE)
                    command->data.executable.args[argc++] = token.data.string;
                else if (command->type == BUILTIN)
                    command->data.executable.args[argc++] = token.data.string;
                DERROR("ARGUMENT ADDED: %s\n", token.data.string);
            }
        }
    }

    if(state == SIN){
        return 2;
    }else if(state == SOUT){
        return 3;
    }

    if (token.type == PIPE) {
        if (command->type == EXECUTABLE)
            command->data.executable.pipe[1] = true;
        else if (command->type == BUILTIN)
            command->data.executable.pipe[1] = true;
        DERROR("PIPE\n");
    }

    if(command->type == EXECUTABLE){
        command->data.executable.args[argc++] = NULL;
    }else if (command->type == BUILTIN){
        command->data.builtin.args[argc++] = NULL;
    }
    *last = token;
    return 0;
}

int main() {
    struct reader reader;
    struct tokenizer tokenizer;
    struct token token;
    struct command command;
    bool currInPipe = false, nextInPipe = false;
    int pipeO[2] = {-1, -1}; // pipe[0]: read && pipe[1]: write
    int pipeN[2] = {-1, -1}; // pipe[0]: read && pipe[1]: write
    int error;
    int pin, pout, fin, fout;

    reader_create(&reader, STDIN_FILENO);
    tokenizer_create(&tokenizer, &reader);

    memset(&command, 0, sizeof(command));

    while (!parseCommand(tokenizer, &command, &token)) {
        pin = pout = fin = fout = -1;
        if(command.type == EXECUTABLE){
            if(command.data.executable.pipe[1]){
                if(error = pipe(pipeN)){
                    DERROR("PIPE ERROR: %s", strerror(errno));
                    perror("pipe");
                }
                pout = pipeN[1];
                nextInPipe = true;
            }else{
                nextInPipe = false;
            }

            if(currInPipe){
                pin = pipeO[0];
            }

            if(command.data.executable.inforward != NULL){
                fin = open(command.data.executable.inforward, O_RDONLY);

                if(fin == -1){
                    DERROR("INFORWARD ERROR(%s): %s\n", command.data.executable.inforward, strerror(errno));
                    perror("INFORWARD ERROR");
                    exit(1);
                }
            }

            if(command.data.executable.outforward != NULL){
                fout = open(command.data.executable.outforward, O_WRONLY | O_CREAT, S_IRWXU);

                if(fout == -1){
                    DERROR("OUTFORWARD ERROR(%s): %s\n", command.data.executable.outforward, strerror(errno));
                    perror("OUTFORWARD ERROR");
                    exit(1);
                }
            }

            execIt(command.data.executable, pin, pout, fin, fout);
            if(fin != -1)
                close(fin);
            if(fout != -1)
                close(fout);
        } else if(command.type == BUILTIN){

        }

        if(pipeO[0] != -1 || pipeO[1] != -1){
            close(pipeO[0]);
            close(pipeO[1]);
            pipeO[0] = -1;
            pipeO[1] = -1;
        }
        memcpy(pipeO, pipeN, sizeof(pipeN));
        memset(&command, 0, sizeof(command));
        currInPipe = nextInPipe;

        if(token.type == ENDL)
            break;
    }
    return 0;
}