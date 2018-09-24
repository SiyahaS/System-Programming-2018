//
// Created by siyahas on 16.03.2018.
//

#include <string.h>
#include "tiff.h"

#ifdef DEBUG
#define DERROR(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##args)
#else
#define DERROR(fmt, args...)
#endif
struct header {
    uint16_t byteOrder;
    uint16_t version;
    uint32_t ifdOffset;
};

struct tag {
    uint16_t tagId;
    uint16_t dataType;
    uint32_t dataCount;
    uint32_t dataOffset;
};

struct ifd {
    uint16_t count;
    uint32_t nextIFDOffset;
    struct tag tags[];
};

struct _tiff {
    struct header header;
    struct ifd ifds[];
};

bool readAll(int fd, void *buffer, size_t size) {
    size_t total = 0;
    int n;

    do {
        n = read(fd, (uint8_t *) buffer + total, size - total);

        if (n <= 0) {
            perror("readall");
            return true;
        }

        total += n;
    } while (total != size);

    return false;
}

void clean32(uint32_t **p) {
    if (*p != NULL)
        free(*p);
    *p = NULL;
}

void clean8(uint8_t **p) {
    if (*p != NULL)
        free(*p);
    *p = NULL;
}

/*
uint16_t swapEndianness16(uint16_t val){
    return val << 8u | val >> 8u;
}

uint32_t swapEndianness32(uint32_t val){
    return (val & 0xff000000) >> 24u |
           (val & 0x00ff0000) >> 8u  |
           (val & 0x0000ff00) << 8u  |
           (val & 0x000000ff) << 24u;
}
*/

const char *const tiffErrorF(struct tiffError const error) {
    switch (error.error) {
        case UNKNOWN_BYTE_ORDER:
            return "UNKNOWN BYTE ORDER";
        case READ_ERROR:
            return "FILE READ ERROR";
        case SEEK_ERROR:
            return "FILE SEEK ERROR";
        case UNKNOWN_COLOR_SPACE:
            return "UNKNOWN COLOR SPACE";
        case MALLOC_ERROR:
            return "MALLOC ERROR";
    }
}

tiff_t const readFD(int fd, struct tiffError *const err) {
    tiff_t tiff;

    struct header header;
    size_t headerSize = sizeof(struct header);
    struct ifd ifd;
    size_t ifdSize = sizeof(struct ifd);
    struct tag tag;
    size_t tagSize = sizeof(struct tag);
    struct tag width;
    struct tag height;
    struct tag bitsPerSample;
    struct tag photometricInterpretation;
    struct tag stripOffset;
    struct tag samplesPerPixel;
    struct tag stripByteCounts;
    struct tag rowsPerStrip;

    bool error;

    /* read header */
    error = readAll(fd, &header, sizeof(header));

    if (error) {
        err->data = sizeof(header);
        err->error = READ_ERROR;
        return NULL;
    }

    if (header.byteOrder == II)
        header.ifdOffset = le32toh(header.ifdOffset);
    else if (header.byteOrder == MM)
        header.ifdOffset = be32toh(header.ifdOffset);
    else {
        err->data = header.byteOrder;
        err->error = UNKNOWN_BYTE_ORDER;
        return NULL;
    }

    DERROR("BYTE ORDER: %s\n", (header.byteOrder == II ? "II" : "MM"));

    /* read ifds */
    ifd.nextIFDOffset = header.ifdOffset;

    do {
        error = lseek(fd, ifd.nextIFDOffset, SEEK_SET);

        if (error == -1) {
            err->data = ifd.nextIFDOffset;
            err->error = SEEK_ERROR;
            return NULL;
        }

        DERROR("SEEK IFD: %X\n", ifd.nextIFDOffset);

        error = readAll(fd, &(ifd.count), sizeof(ifd.count));

        if (error) {
            err->data = ifd.count;
            err->error = READ_ERROR;
            return NULL;
        }

        if (header.byteOrder == II) {
            ifd.count = le16toh(ifd.count);
        } else {
            ifd.count = be16toh(ifd.count);
        }

        DERROR("TAG COUNT: %d\n", ifd.count);

        /* read tags one by one */
        for (int i = 0; i < ifd.count; ++i) {
            error = readAll(fd, &tag, sizeof(tag));

            if (error) {
                err->data = sizeof(tag);
                err->error = READ_ERROR;
                return NULL;
            }

            /* correct the endianess */
            if (header.byteOrder == II) {
                tag.tagId = le16toh(tag.tagId);
                tag.dataType = le16toh(tag.dataType);
                tag.dataCount = le32toh(tag.dataCount);
                if (tag.dataType == WORD)
                    tag.dataOffset = le16toh(tag.dataOffset);
                else if (tag.dataType == DWORD || tag.dataType == RATIONAL)
                    tag.dataOffset = le32toh(tag.dataOffset);
            } else {
                tag.tagId = be16toh(tag.tagId);
                tag.dataType = be16toh(tag.dataType);
                tag.dataCount = be32toh(tag.dataCount);
                if (tag.dataType == WORD)
                    tag.dataOffset = be16toh(tag.dataOffset);
                else if (tag.dataType == DWORD || tag.dataType == RATIONAL)
                    tag.dataOffset = be32toh(tag.dataOffset);
            }

            switch (tag.tagId) {
                case IMAGE_WIDTH:
                    DERROR("WIDTH: %d\n", (uint16_t) tag.dataOffset);
                    memcpy(&width, &tag, sizeof(tag));
                    break;
                case IMAGE_LENGTH:
                    DERROR("HEIGHT: %d\n", (uint16_t) tag.dataOffset);
                    memcpy(&height, &tag, sizeof(tag));
                    break;
                case STRIP_BYTE_COUNTS:
                    DERROR("SBC: %X\n", (uint16_t) tag.dataOffset);
                    DERROR("SBCC: %d\n", (uint16_t) tag.dataCount);
                    memcpy(&stripByteCounts, &tag, sizeof(tag));
                    break;
                case STRIP_OFFSETS:
                    DERROR("SOFF: %X\n", tag.dataOffset);
                    DERROR("SOFFC: %d\n", tag.dataCount);
                    DERROR("SOFFT: %d\n", tag.dataType);
                    memcpy(&stripOffset, &tag, sizeof(tag));
                    break;
                case SAMPLES_PER_PIXEL:
                    DERROR("SPP: %d\n", tag.dataOffset);
                    memcpy(&samplesPerPixel, &tag, sizeof(tag));
                    break;
                case BITS_PER_SAMPLE:
                    DERROR("BPS: %d\n", tag.dataOffset);
                    memcpy(&bitsPerSample, &tag, sizeof(tag));
                    break;
                case PHOTOMETRIC_INTERPRETATION:
                    DERROR("COLOR: %d\n", tag.dataOffset);
                    memcpy(&photometricInterpretation, &tag, sizeof(tag));
                    break;
                case ROWS_PER_STRIP:
                    DERROR("RPS: %X\n", tag.dataOffset);
                    memcpy(&rowsPerStrip, &tag, sizeof(tag));
                default:
                    DERROR("TAG %d/%d\n", i + 1, ifd.count);
                    DERROR("--> ID:      %d\n", tag.tagId);
                    DERROR("--> DTYPE:   %d\n", tag.dataType);
                    DERROR("--> DCOUNT:  %d\n", tag.dataCount);
                    DERROR("--> DOFFSET: %d\n", tag.dataOffset);
                    break;
            }
        }

        error = readAll(fd, &(ifd.nextIFDOffset), sizeof(ifd.nextIFDOffset));

        if (error) {
            err->data = sizeof(ifd.nextIFDOffset);
            err->data = READ_ERROR;
            return NULL;
        }
    } while (ifd.nextIFDOffset != (uint32_t) 0);

    if (photometricInterpretation.dataOffset != BLACK_IS_ZERO &&
        photometricInterpretation.dataOffset != WHITE_IS_ZERO && photometricInterpretation.dataOffset) {
        err->data = photometricInterpretation.dataOffset;
        err->error = UNKNOWN_COLOR_SPACE;
        return NULL;
    }

    /* add tiff data to struct */
    tiff = malloc(sizeof(struct tiff));

    if (tiff == NULL) {
        err->error = MALLOC_ERROR;
        return NULL;
    }

    tiff->byteOrder = header.byteOrder;
    tiff->width = width.dataOffset;
    tiff->height = height.dataOffset;

    uint8_t *buff __attribute__((__cleanup__(clean8)));
    uint32_t buffSize = 0;
    if (stripOffset.dataCount == 1) {
        error = lseek(fd, stripOffset.dataOffset, SEEK_SET);
        if (error == -1) {
            err->data = stripOffset.dataOffset;
            err->error = SEEK_ERROR;
            return NULL;
        }

        buff = malloc(sizeof(uint8_t) * stripByteCounts.dataOffset);
        buffSize = sizeof(uint8_t) * stripByteCounts.dataOffset;
        sleep(100);

        if (buff == NULL) {
            err->error = MALLOC_ERROR;
            return NULL;
        }

        error = readAll(fd, buff, sizeof(uint8_t) * stripByteCounts.dataOffset);
        if (error) {
            // todo cleanup
            err->data = sizeof(uint8_t) * stripByteCounts.dataOffset;
            err->error = READ_ERROR;
            return NULL;
        }
    } else {
        uint32_t *offsets __attribute__((__cleanup__(clean32))) = malloc(sizeof(uint32_t) * stripOffset.dataCount);
        uint32_t *byteCounts __attribute__((__cleanup__(clean32))) = malloc(
                sizeof(uint32_t) * stripByteCounts.dataCount);

        if (offsets == NULL || byteCounts == NULL) {
            err->error = MALLOC_ERROR;
            return NULL;
        }

        /* read offsets */
        error = lseek(fd, stripOffset.dataOffset, SEEK_SET);
        if (error == -1) {
            // todo cleanup
            err->data = stripOffset.dataOffset;
            err->error = SEEK_ERROR;
            return NULL;
        }
        error = readAll(fd, offsets, sizeof(uint32_t) * stripOffset.dataCount);
        if (error) {
            // todo cleanup
            err->data = sizeof(uint32_t) * stripOffset.dataCount;
            err->error = READ_ERROR;
            return NULL;
        }

        /* read byte counts */
        error = lseek(fd, stripByteCounts.dataOffset, SEEK_SET);
        if (error == -1) {
            // todo cleanup
            err->data = stripByteCounts.dataOffset;
            err->error = SEEK_ERROR;
            return NULL;
        }
        error = readAll(fd, byteCounts, sizeof(uint32_t) * stripByteCounts.dataCount);
        if (error) {
            // todo cleanup
            err->data = sizeof(uint32_t) * stripByteCounts.dataCount;
            err->error = READ_ERROR;
            return NULL;
        }

        /* find total required buffer size */
        for (int i = 0; i < stripByteCounts.dataCount; ++i) {
            buffSize += byteCounts[i];
        }

        buff = malloc(sizeof(uint8_t) * buffSize);

        if (buff == NULL) {
            err->error = MALLOC_ERROR;
            return NULL;
        }

        /* read into buffer */
        size_t t = 0;
        for (int j = 0; j < stripOffset.dataCount; ++j) {
            lseek(fd, offsets[j], SEEK_SET);
            if (error == -1) {
                // todo cleanup
                err->data = offsets[j];
                err->error = SEEK_ERROR;
                return NULL;
            }
            error = readAll(fd, buff + t, byteCounts[j]);
            if (error) {
                // todo cleanup
                err->data = byteCounts[j];
                err->error = READ_ERROR;
                return NULL;
            }
            t += byteCounts[j];
        }
    }

    if (bitsPerSample.dataOffset == 1) {
        tiff->data = malloc(sizeof(uint8_t) * tiff->width * tiff->height);
        for (int i = 0; i < tiff->height; ++i) {
            size_t start = i * (tiff->width / 8 + (tiff->width % 8 == 0 ? 0 : 1));
            for (int j = 0; j < tiff->width; ++j) {
                uint8_t shift = j % 8;
                int bi = j / 8;
                uint8_t v = (buff[start + bi] & ((uint8_t) 0x01 << (7 - shift))) == WHITE_IS_ZERO ? (uint8_t) 0
                                                                                                  : (uint8_t) 255;

                tiff->data[i * tiff->width + j] = v;
            }
        }
    } else if (bitsPerSample.dataOffset == 8) {
        tiff->data = buff;
        buff = NULL;
    }
    return tiff;
}