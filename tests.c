#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <pthread.h>

#include "tests.h"


int tests_startup(test_parameters_t* test_parameters)
{
    switch (test_parameters->type)
    {
        case TEST_CORPUS_COMPRESSION:
            return tests_startup_corpus_compression(test_parameters);
            break;
        case TEST_CORPUS_DECOMPRESSION:
            return tests_startup_corpus_decompression(test_parameters);
            break;
        default:
            fprintf(stderr, "Unknown test type %d\n", test_parameters->type);
            return TEST_FAILED;
            break;
    }
}


/******************************************************************************
* function:
*   tests_run (int count,
*              int type
*              ENGINE *e
*              int id
*              int print_output
*              int verify
*              int performance)
*
* @param count        [IN] - Number of iteration count
* @param type         [IN] - Testing type
* @param size         [IN] - testing data size
* @param e            [IN] - OpenSSL engine pointer
* @param id           [IN] - Thread ID
* @param print_output [IN] - Print hex out flag
* @param verfiy       [IN] - Verify output flag
* @param performance  [IN] - performance output flag
*
* description:
*   select which application to run based on user input
******************************************************************************/

int tests_run(test_parameters_t* test_parameters)
{
    int rc=1;
    printf("\n|-----------------------------------------------------|\n");
    printf("|----------Thread ID %d, running in progress-----------|\n",test_parameters->id);
    printf("|-----------------------------------------------------|\n");

    switch (test_parameters->type)
    {
        case TEST_CORPUS_COMPRESSION:
            rc=tests_run_corpus_compression(test_parameters);
            break;
        case TEST_CORPUS_DECOMPRESSION:
            rc=tests_run_corpus_decompression(test_parameters);
            break;
        default:
            fprintf(stderr, "Unknown test type %d\n", test_parameters->type);
            rc=TEST_FAILED;
            break;
    }

    printf("\n|-----------------------------------------------------|\n");
    printf("|----------Thread ID %3d finished---------------------|\n",
           test_parameters->id);
    printf("|-----------------------------------------------------|\n");
    return rc;
}

int tests_shutdown(test_parameters_t* test_parameters)
{
    switch (test_parameters->type)
    {
        case TEST_CORPUS_COMPRESSION:
            return tests_shutdown_corpus_compression(test_parameters);
            break;
        case TEST_CORPUS_DECOMPRESSION:
            return tests_shutdown_corpus_decompression(test_parameters);
            break;
        default:
            fprintf(stderr, "Unknown test type %d\n", test_parameters->type);
            return TEST_FAILED;
            break;
    }
}
