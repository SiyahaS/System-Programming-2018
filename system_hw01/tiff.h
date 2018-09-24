//
// Created by siyahas on 16.03.2018.
//

#ifndef SYSTEM_HW01_TIFF_H
#define SYSTEM_HW01_TIFF_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>3
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <endian.h>

enum byteOrder {
    II = 0x4949,
    MM = 0x4D4D
};

enum colorSpaces {
    WHITE_IS_ZERO = 0,
    BLACK_IS_ZERO,
    RGB,
    PALETTE,
    TRANSPARENCY
};

enum tagId {
    NEW_SUBFILE_TYPE = 0x00FE,
    SUBFILE_TYPE,
    IMAGE_WIDTH,
    IMAGE_LENGTH,
    BITS_PER_SAMPLE,
    COMPRESSION,
    PHOTOMETRIC_INTERPRETATION = 0x0106,
    THRESHOLDING,
    CELL_WIDTH,
    CELL_LENGTH,
    FILL_ORDER,
    IMAGE_DESCRIPTION = 0x010E,
    MAKE,
    MODEL,
    STRIP_OFFSETS,
    ORIENTATION,
    SAMPLES_PER_PIXEL = 0x0115,
    ROWS_PER_STRIP,
    STRIP_BYTE_COUNTS,
    MIN_SAMPLE_VALUE,
    MAX_SAMPLE_VALUE,
    X_RESOLUTION,
    Y_RESOLUTION,
    PLANAR_CONFIGURATION,
    FREE_OFFSET = 0x0120,
    FREE_BYTE_COUNTS,
    GRAY_RESPONSE_UNIT,
    GRAY_RESPONSE_CURVE,
    RESOLUTION_UNIT = 0x0128,
    SOFTWARE = 0x0131,
    DATE_TIME,
    ARTIST = 0x013B,
    HOST_COMPUTER,
    COLOR_MAP = 0x0140,
    EXTRA_SAMPLES = 0x0152,
    COPYRIGHT = 0x8298
};

enum dataType {
    BYTE = 1,
    ASCIIZ,
    WORD,
    DWORD,
    RATIONAL
};

typedef struct tiff {
    enum byteOrder byteOrder;
    uint32_t width;
    uint32_t height;
    uint8_t *data;
} *tiff_t;

enum tiffErrorE {
    UNKNOWN_BYTE_ORDER = 1,
    READ_ERROR,
    SEEK_ERROR,
    UNKNOWN_COLOR_SPACE,
    MALLOC_ERROR,
};

struct tiffError {
    uint32_t data;
    enum tiffErrorE error;
};

const char *const tiffErrorF(struct tiffError const error);

tiff_t const readFD(int fd, struct tiffError *const error) __attribute__((warn_unused_result));

#endif //SYSTEM_HW01_TIFF_H
