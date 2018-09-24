#include <stdio.h>
#include <errno.h>
#include "tiff.h"

int main(int argc, char *argv[]) {
    struct tiffError error;
    int fd;

    if (argc != 2) {
        printf("Usage: %s filepath\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("FILE ERROR");
        return 1;
    }

    tiff_t tiff = readFD(fd, &error);

    if (tiff == NULL) {
        printf("%s: %X", tiffErrorF(error), error.data);
        return 1;
    }

    printf("Width: %d pixels\n", tiff->width);
    printf("Height: %d pixels\n", tiff->height);
    printf("Byte order: %s\n", (tiff->byteOrder == II ? "Intel" : "Motorola"));

    for (int i = 0; i < tiff->width; ++i) {
        for (int j = 0; j < tiff->height; ++j) {
            fprintf(stdout, "%s",
                    (tiff->data[i * tiff->width + j] == 0 ? "0" : (tiff->data[i * tiff->width + j] == 255 ? "1"
                                                                                                          : "X")));
        }
        fprintf(stdout, "\n");
    }

    close(fd);
    free(tiff->data);
    free(tiff);
    return 0;
}