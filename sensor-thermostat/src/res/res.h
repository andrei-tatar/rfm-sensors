#ifndef _RES_H_
#define _RES_H_

#include <stdint.h>
#include <avr/pgmspace.h>

typedef struct
{
    const uint8_t *data;
    uint16_t width;
    uint16_t height;
} tImage;

typedef struct
{
    long int code;
    const tImage *image;
} tChar;

typedef struct
{
    int length;
    const tChar *chars;
} tFont;

extern const tImage img_error;
extern const tImage img_sync;
extern const tFont roboto_big;
extern const tFont roboto_medium;

#endif