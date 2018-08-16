#ifndef __TESTS_H
#define __TESTS_H

#include "test_parameters.h"

static __inline__ unsigned long long rdtsc(void)
{
    unsigned long a, d;

    asm volatile ("rdtsc":"=a" (a), "=d"(d));

    return (((unsigned long long)a) | (((unsigned long long)d) << 32));
}

int tests_startup (test_parameters_t* test_parameters);
int tests_run (test_parameters_t* test_parameters);
int tests_shutdown (test_parameters_t* test_parameters);

/* This function will read in the files to a memory buffer ready for
   the corpus compression test */
int tests_startup_corpus_compression (test_parameters_t* test_parameters);

/* This function runs the actual corpus compression test. It will submit
   the buffer created in the startup function above in chunks until the
   zlib API has finished compressing. It uses the Z_NO_FLUSH flag. It will
   do this as many times as specified in the test_parameters */
int tests_run_corpus_compression (test_parameters_t* test_parameters);

/* This function will cleanup up the memory allocation of buffers after 
   the corpus compression. */ 
int tests_shutdown_corpus_compression (test_parameters_t* test_parameters);


/* This function will read in the files to a memory buffer ready. It will
   then compress the files so they are in a form ready to be submitted to
   the corpus decompression test */
int tests_startup_corpus_decompression (test_parameters_t* test_parameters);

/* This function runs the actual corpus decompression test. It will submit
   the buffer created in the startup function above as one large buffer to
   the zlib API and read output into one big output buffer big enough to
   hold the whole decompressed file. It will run the decompression as many 
   times as specified in the test parameters */ 
int tests_run_corpus_decompression (test_parameters_t* test_parameters);

/* This function will cleanup up the memory allocation of buffers after 
   the corpus decompression. */ 
int tests_shutdown_corpus_decompression (test_parameters_t* test_parameters);


/* Defines for zlib corner tests maximum length for stateless operation */
#define DEFLATE_LENGTH          108544

#define TEST_CORPUS_COMPRESSION               1 
#define TEST_CORPUS_DECOMPRESSION             2
#define TEST_TYPE_MAX           TEST_CORPUS_DECOMPRESSION
#define CUSTOM_FILE                           0       
#define CANTERBURY_CORPUS                     1
#define CALGARY_CORPUS                        2
#define SILESIA_CORPUS                        3
#define CORPUS_MAX              SILESIA_CORPUS
#define RAW_DEFLATE_STREAM                    0
#define ZLIB_DEFLATE_STREAM                   1
#define GZIP_DEFLATE_STREAM                   2
#define STREAMTYPE_MAX          GZIP_DEFLATE_STREAM
#define WINDOW_SIZE_8K                        5
#define WINDOW_SIZE_16K                       6
#define WINDOW_SIZE_32K                       7
#define TEST_PASSED                           0
#define TEST_FAILED                           1
#define DEBUG(...) 

#define QAT_DEV "/dev/qat_mem"

#endif

