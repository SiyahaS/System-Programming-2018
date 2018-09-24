//
// Created by siyahas on 25.04.2018.
//

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#define BUFF_SIZE 1024

void wc(int fd, size_t *l, size_t *w, size_t *b);

int main(int argc, char *argv[]){
    size_t l = 0, w = 0, b = 0;
    if(argc == 1){
        wc(STDIN_FILENO, &l, &w, &b);
        printf("\t%d\t%d\t%d\n", l, w, b);
    } else {
        for(int i = 2 ; i < argc ; ++i){
            int fd = open(argv[i], O_RDONLY);
            if(fd == -1){
                perror("Could not open file");
            } else{
                l=0;
                w=0;
                b=0;
                wc(fd, &l, &w, &b);
                printf("\t%d\t%d\t%d\n", l, w, b);
            }
        }
    }

    return 0;
}

void wc(int fd, size_t *l, size_t *w, size_t *b){
    char buff[BUFF_SIZE];
    size_t rb;

    while((rb = read(fd, &buff, sizeof(buff))) > 0){
        *b += rb;
        for(int i = 0 ; i < rb ; ++i){
            if(buff[i] == '\n')
                ++(*l);

            if(isspace(buff[i])){
                ++(*w);
            }
        }
    }

    if(rb == -1){
        perror("Could not read file");
    }

    return;
}