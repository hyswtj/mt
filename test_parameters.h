#ifndef __TEST_PARAMETERS_H
#define __TEST_PARAMETERS_H

#include "zlib.h"

typedef struct
{
    int count;
    int type;
    int id;
    char *file_path;
    unsigned char* input_buf;
    unsigned char* output_buf;
    unsigned long input_buflen;
    unsigned long output_buflen;
    unsigned long single_call_bytes;
    unsigned long  verify_checksum;
    int level;
    int chunksize;
    int corpus;
    int enable_deflate_buffering;
    int enable_inflate_buffering;
    int allow_partial_chunks;
    int streamtype;
    int verify;
    float ratio;
    z_stream strm;
}
test_parameters_t;

#endif
