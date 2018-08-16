/* macros defined to allow use of the cpu get and set affinity functions */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

#include "test_parameters.h"
#include "tests.h"
#include "zlib.h"

#define DEFAULT_CHUNK_SIZE 8096
#define DEFAULT_COMPRESSION_LEVEL -1
#define DEFAULT_THREAD_COUNT 1
#define DEFAULT_CORE_COUNT 1
#define DEFAULT_TEST_COUNT 1
#define CPU_STATUS_LINE_LENGTH 1024
#define TAG_LENGTH 10
#define CPU_TIME_MULTIPLIER 10000
#define CPU_PERCENTAGE_MULTIPLIER 100

static pthread_cond_t ready_cond;
static pthread_cond_t startupfinished_cond;
static pthread_cond_t start_cond;
static pthread_cond_t stop_cond;
static pthread_cond_t end_cond;
static pthread_mutex_t mutex;
static int cleared_to_start;
static int active_thread_count;
static int stop_thread_count;
static int ready_thread_count;
static int startupfinished_thread_count;

/* thread_count - number of threads to create */
static int thread_count = DEFAULT_THREAD_COUNT;

/* define the initial test values */
static int core_count = DEFAULT_CORE_COUNT;
static int test_count = DEFAULT_TEST_COUNT;
static int actual_test_count = 0;
static int test_size = 0;
static int cpu_affinity = 0;
static int test_type = 0;
static int cpu_core_info = 0;
static char *FileNameOrPath;
static int filenamePathSet = 0;
static int compression_level = DEFAULT_COMPRESSION_LEVEL;
static int chunk_size = DEFAULT_CHUNK_SIZE;
static int corpus = CALGARY_CORPUS;
static int enable_deflate_buffering = 1;
static int enable_inflate_buffering = 1;
static int stream_type = GZIP_DEFLATE_STREAM;
static int allow_partial_chunks = 0;
static int verify = 0;
static float ratio = 0;
static int failure_occured = 0;

/* Thread_info structure declaration */
typedef struct
{
    pthread_t th;
    int id;
    int count;
}
THREAD_INFO;

#define MAX_STAT 10
#define MAX_CORE 32
typedef union
{
    struct
    {
        int user;
        int nice;
        int sys;
        int idle;
        int io;
        int irq;
        int softirq;
        int context;
    };
    int d[MAX_STAT];
}
cpu_time_t;

static cpu_time_t cpu_time[MAX_CORE];
static cpu_time_t cpu_time_total;
static cpu_time_t cpu_context;

#define MAX_THREAD 1024

THREAD_INFO tinfo[MAX_THREAD];

/******************************************************************************
* function:
*     cpu_time_add (cpu_time_t *t1, cpu_time_t *t2, int subtract)
*
* @param t1 [IN] - cpu time
* @param t2 [IN] - cpu time
* @param substract [IN] - subtract flag
*
* description:
*   CPU timing calculation functions.
******************************************************************************/
static void cpu_time_add (cpu_time_t *t1, cpu_time_t *t2, int subtract)
{
    int i;

    for (i = 0; i < MAX_STAT; i++)
    {
        if (subtract)
            t1->d[i] -= t2->d[i];
        else
            t1->d[i] += t2->d[i];
    }
}


/******************************************************************************
* function:
*    read_stat (int init)
*
* @param init [IN] - op flag
*
* description:
*  read in CPU status from proc/stat file
******************************************************************************/
static void read_stat (int init)
{
    char line[CPU_STATUS_LINE_LENGTH];
    char tag[TAG_LENGTH];
    FILE *fp;
    int index = 0;
    int i;
    cpu_time_t tmp;

    fp = fopen ("/proc/stat", "r");
    if (NULL == fp)
    {
        fprintf (stderr, "Can't open proc stat\n");
        exit (1);
    }

    while (!feof (fp))
    {
        if (fgets (line, sizeof line - 1, fp) == NULL)
            break;

        if (!strncmp (line, "ctxt", 4))
        {
            if (sscanf (line, "%*s %d", &tmp.context) < 1)
                goto parse_fail;

            cpu_time_add (&cpu_context, &tmp, init);
            continue;
        }

        if (strncmp (line, "cpu", 3))
            continue;

        if (sscanf (line, "%s %d %d %d %d %d %d %d",
                tag,
                &tmp.user,
                &tmp.nice,
                &tmp.sys,
                &tmp.idle,
                &tmp.io,
                &tmp.irq,
                &tmp.softirq) < 8)
        {
            goto parse_fail;
        }

        if (!strcmp (tag, "cpu"))
            cpu_time_add (&cpu_time_total, &tmp, init);
        else if (!strncmp (tag, "cpu", 3))
        {
            index = atoi (&tag[3]);
            if ((0 <= index) && (MAX_CORE >= index))
                cpu_time_add (&cpu_time[index], &tmp, init);
        }
    }

    if (!init && cpu_core_info)
    {
        printf ("      %10s %10s %10s %10s %10s %10s %10s\n",
                "user", "nice", "sys", "idle", "io", "irq", "sirq");
        for (i = 0; i < MAX_CORE + 1; i++)
        {
            cpu_time_t *t;

            if (i == MAX_CORE)
            {
                printf ("total ");
                t = &cpu_time_total;
            }
            else
            {
                printf ("cpu%d  ", i);
                t = &cpu_time[i];
            }

            printf (" %10d %10d %10d %10d %10d %10d %10d\n",
                    t->user,
                    t->nice,
                    t->sys,
                    t->idle,
                    t->io,
                    t->irq,
                    t->softirq);
        }

        printf ("Context switches: %d\n", cpu_context.context);
    }

    fclose (fp);
    return;

parse_fail:
    fprintf (stderr, "Failed to parse %s\n", line);
    exit (1);
}

/******************************************************************************
* function:
*           *test_name(int test)
*
* @param test [IN] - test case
*
* description:
*   test_name selection list
******************************************************************************/
static char *test_name(int test)
{
    switch (test)
    {
        case TEST_CORPUS_COMPRESSION:
            return "Corpus Compression";
            break;
        case TEST_CORPUS_DECOMPRESSION:
            return "Corpus Decompression";
            break;
        case 0:
            return "invalid";
            break;
    }
    return "*unknown*";
}

/******************************************************************************
* function:
*           *corpus_name(int selectedcorpus)
*
* @param selectedcorpus [IN] - corpus number
*
* description:
*   corpus_name maps enum to textual name
******************************************************************************/
static char *corpus_name(int selectedcorpus)
{
    switch (selectedcorpus)
    {
        case CANTERBURY_CORPUS:
            return "Canterbury Corpus";
            break;
        case CALGARY_CORPUS:
            return "Calgary Corpus";
            break;
        case SILESIA_CORPUS:
            return "Silesia Corpus";
            break;
        case CUSTOM_FILE:
            return "Custom (customfile.bin)";
            break;
    }
    return "*unknown*";
}

/******************************************************************************
* function:
*           *streamtype_name(int selectedstreamtype)
*
* @param selectedstreamtype [IN] - number representing the type of stream.
*
* description:
*   streamtype_name maps an enum to a textual name
******************************************************************************/
static char *streamtype_name(int selectedstreamtype)
{
    switch (selectedstreamtype)
    {
        case RAW_DEFLATE_STREAM:
            return "Raw Deflate Stream";
            break;
        case ZLIB_DEFLATE_STREAM:
            return "Zlib Format Deflate Stream";
            break;
        case GZIP_DEFLATE_STREAM:
            return "Gzip Format Deflate Stream";
            break;
    }
    return "*unknown*";
}

/******************************************************************************
* function:
*           usage(char *program)
*
*
* @param program [IN] - input argument
*
* description:
*   test application usage help
******************************************************************************/
static void usage(char *program)
{
    int i;

    printf("\nUsage:\n");
    printf("\t%s [-t <type>] [-c <count>] [-n <count>] [-nc <count>]"
           " [-k <size>] [-o <corpus>] [-u]"
           " [-af] [-f <filepath>] [-l <compressionlevel>]"
           " [-ddb] [-dib] [-s <streamtype>]"
           " [-pc] [-v] [-h]\n", program);
    printf("Where:\n");
    printf("\t-t   specifies the test type to run (see below)\n");
    printf("\t-c   specifies the test iteration count\n");
    printf("\t-n   specifies the number of threads to run\n");
    printf("\t-nc  specifies the number of CPU cores\n");
    printf("\t-k   specifies the chunk size in bytes\n");
    printf("\t-o   specifies the corpus to use for the tests (see below)\n");
    printf("\t-u   display cpu usage per core\n");
    printf("\t-af  enables core affinity\n");
    printf("\t-f   specifies the filepath for a corpus test\n");
    printf("\t-l   specifies the compression level\n");
    printf("\t-ddb disables internal buffering for shim deflate\n");
    printf("\t-dib disables internal buffering for shim inflate\n");
    printf("\t-s   specifies the type of deflate stream (see below)\n");
    printf("\t-pc  allow partial chunks\n");
    printf("\t-v   enable verification of data (use with -c 1)\n");
    printf("\t-h   print this usage\n");
    printf("\nand where the -t test type is:\n\n");

    for (i = 1; i <= TEST_TYPE_MAX; i++)
        printf("\t%-2d = %s\n", i, test_name(i));

    printf("\nand where the -o corpus is:\n\n");
    for (i = 0; i <= CORPUS_MAX; i++)
        printf("\t%-2d = %s\n", i, corpus_name(i));

    printf("\nand where the -s streamtype is:\n\n");
    for (i = 0; i <= STREAMTYPE_MAX; i++)
        printf("\t%-2d = %s\n", i, streamtype_name(i));

    exit(EXIT_SUCCESS);
}

/******************************************************************************
* function:
*           parse_option(int *index,
*                        int argc,
*                       char *argv[],
*                        int *value)
*
* @param index [IN] - index pointer
* @param argc [IN] - input argument count
* @param argv [IN] - argument buffer
* @param value [IN] - input value pointer
*
* description:
*   user input arguments check
******************************************************************************/
static void parse_option(int *index, int argc, char *argv[], int *value)
{
    if (*index + 1 >= argc)
    {
        fprintf(stderr, "\nParameter expected\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    (*index)++;

    *value = atoi(argv[*index]);
}

/******************************************************************************
* function:
*           parse_option_long(int *index,
*                        int argc,
*                       char *argv[],
*                        long *value)
*
* @param index [IN] - index pointer
* @param argc [IN] - input argument count
* @param argv [IN] - argument buffer
* @param value [IN] - input value pointer
*
* description:
*   user input arguments check
******************************************************************************/
/*static void parse_option_long(int *index, int argc, char *argv[], long *value)
{
    if (*index + 1 >= argc)
    {
        fprintf(stderr, "\nParameter expected\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    (*index)++;

    *value = atol(argv[*index]);
}*/

/******************************************************************************
* function:
*           handle_option(int argc,
*                         char *argv[],
*                         int *index)
*
* @param argc [IN] - input argument count
* @param argv [IN] - argument buffer
* @param index [IN] - index pointer
*
* description:
*   input operation handler
******************************************************************************/
static void handle_option(int argc, char *argv[], int *index)
{
    char *option = argv[*index];

    if (!strcmp(option, "-n")) {
        parse_option(index, argc, argv, &thread_count);
        if (thread_count > MAX_THREAD) {
            fprintf(stderr, "Error: Exceeded maximum number of threads\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (!strcmp(option, "-t"))
        parse_option(index, argc, argv, &test_type);
    else if (!strcmp(option, "-c"))
        parse_option(index, argc, argv, &test_count);
    else if (!strcmp(option, "-af"))
        cpu_affinity = 1;
    else if (!strcmp(option, "-nc"))
        parse_option(index, argc, argv, &core_count);
    else if (!strcmp(option, "-f"))
    {
        if (*index + 1 >= argc)
        {
            fprintf(stderr, "\nParameter expected\n");
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }

        (*index)++;

        FileNameOrPath = argv[*index];
        filenamePathSet = 1;
    }
    else if (!strcmp(option, "-ddb"))
    {
        enable_deflate_buffering = 0;
        printf("Buffering within shim on deflate side disabled !\n");
    }
    else if (!strcmp(option, "-dib"))
    {
        enable_inflate_buffering = 0;
        printf("Buffering within shim on inflate side disabled !\n");
    }
    else if (!strcmp(option, "-l"))
        parse_option(index, argc, argv, &compression_level);
    else if (!strcmp(option, "-k"))
        parse_option(index, argc, argv, &chunk_size);
    else if (!strcmp(option, "-o"))
        parse_option(index, argc, argv, &corpus);
    else if (!strcmp(option, "-s"))
        parse_option(index, argc, argv, &stream_type);
    else if (!strcmp(option, "-pc"))
    {
                allow_partial_chunks = 1;
    }
    else if (!strcmp(option, "-u"))
        cpu_core_info = 1;
    else if (!strcmp(option, "-v"))
        verify = 1;
    else if (!strcmp(option, "-h"))
        usage(argv[0]);
    else
    {
        fprintf(stderr, "\nInvalid option '%s'\n", option);
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
}

/******************************************************************************
* function:
*           *thread_worker(void *arg)
*
* @param arg [IN] - thread structure info
*
* description:
*   thread worker setups. the threads will launch at the same time after
*   all of them in ready condition.
******************************************************************************/
static void *thread_worker(void *arg)
{
    THREAD_INFO *info = (THREAD_INFO *) arg;
    int rc1, rc2, rc3, rc4;
    int abort=0;
    test_parameters_t test_parameters;
    test_parameters.count = info->count;
    test_parameters.type = test_type;
    test_parameters.id = info->id;
    test_parameters.level = compression_level;
    test_parameters.enable_deflate_buffering = enable_deflate_buffering;
    test_parameters.enable_inflate_buffering = enable_inflate_buffering;
    test_parameters.streamtype = stream_type;
    test_parameters.verify = verify;
    test_parameters.chunksize = chunk_size;
    test_parameters.corpus = corpus;
    test_parameters.allow_partial_chunks = allow_partial_chunks;
    test_parameters.verify_checksum = 0;

    if (filenamePathSet)
    {
        test_parameters.file_path = FileNameOrPath;
    }
    else
    {
        test_parameters.file_path = "\0";
    }

    /* mutex lock for thread count */
    rc1 = pthread_mutex_lock(&mutex);
    ready_thread_count++;
    rc2 = pthread_cond_broadcast(&ready_cond);
    rc3 = pthread_mutex_unlock(&mutex);

    rc4 = tests_startup(&test_parameters);
    if ((rc1 != 0) || (rc2 != 0) || (rc3 != 0) || (rc4 != TEST_PASSED))
    {
        failure_occured=1;
        abort=1;
    }

    /* mutex lock for thread count */
    rc1 = pthread_mutex_lock(&mutex);
    startupfinished_thread_count++;
    rc2 = pthread_cond_broadcast(&startupfinished_cond);
    rc3 = pthread_mutex_unlock(&mutex);
    if ((rc1 != 0) || (rc2 != 0) || (rc3 != 0))
    {
        failure_occured=1;
        abort=1;
    }

    /* waiting for thread clearance */
    rc1 = pthread_mutex_lock(&mutex);
    if (rc1 != 0) {
        failure_occured=1;
        abort=1;
    }


    while (!cleared_to_start) {
        rc2 = pthread_cond_wait(&start_cond, &mutex);
        if (rc2 != 0) {
            failure_occured=1;
            abort=1;
        }
    }

    rc3 = pthread_mutex_unlock(&mutex);
    if (rc3 != 0) {
        failure_occured=1;
        abort=1;
    }

    if (!abort)
    {
        rc1 = tests_run(&test_parameters);
        if (rc1 != TEST_PASSED)
            failure_occured=1;
        test_size=test_parameters.single_call_bytes;
        ratio=test_parameters.ratio;
    }
    /* update active threads */
    rc1 = pthread_mutex_lock(&mutex);
    active_thread_count--;
    rc2 = pthread_cond_broadcast(&stop_cond);
    rc3 = pthread_mutex_unlock(&mutex);

    rc4 = tests_shutdown(&test_parameters);
    if ((rc1 != 0) || (rc2 != 0) || (rc3 != 0) || (rc4 != TEST_PASSED))
        failure_occured=1;

    rc1 = pthread_mutex_lock(&mutex);
    stop_thread_count--;
    rc2 = pthread_cond_broadcast(&end_cond);
    rc3 = pthread_mutex_unlock(&mutex);
    /* set a failure but maybe too late by now */
    if ((rc1 != 0) || (rc2 != 0) || (rc3 != 0))
        failure_occured=1;



    return NULL;
}

/******************************************************************************
* function:
*           performance_test(void)
*
* description:
*   performers test application running on user definition .
******************************************************************************/
static void performance_test(void)
{
    int i;
    int coreID = 0;
    int rc = 0;
    int sts = 1;
    cpu_set_t cpuset;
    struct timeval start_time;
    struct timeval stop_time;
    unsigned long elapsed = 0;
    unsigned long long rdtsc_start = 0;
    unsigned long long rdtsc_end = 0;
    int bytes_to_bits = 8;
    float throughput = 0.0;

    rc = pthread_mutex_init(&mutex, NULL);
    if (rc != 0) {
        fprintf(stderr, "Failure to init Mutex, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }
    rc = pthread_cond_init(&ready_cond, NULL);
    if (rc != 0) {
        fprintf(stderr, "Failed call to pthread_cond_init, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }
    rc = pthread_cond_init(&startupfinished_cond, NULL);
    if (rc != 0) {
        fprintf(stderr, "Failed call to pthread_cond_init, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }
    rc = pthread_cond_init(&start_cond, NULL);
    if (rc != 0) {
        fprintf(stderr, "Failed call to pthread_cond_init, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }
    rc = pthread_cond_init(&stop_cond, NULL);
    if (rc != 0) {
        fprintf(stderr, "Failed call to pthread_cond_init, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }
    rc = pthread_cond_init(&end_cond, NULL);
    if (rc != 0) {
        fprintf(stderr, "Failed call to pthread_cond_init, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }
    actual_test_count = test_count / thread_count;
    actual_test_count = actual_test_count * thread_count;

    for (i = 0; i < thread_count; i++)
    {
        THREAD_INFO *info = &tinfo[i];

        info->id = i;
        info->count = test_count / thread_count;
        if (info->count == 0)
        {
            fprintf(stderr, "Error: count set incorrectly resulting in 0 iterations per thread\n");
            exit(EXIT_FAILURE);
        }

        rc = pthread_create(&info->th, NULL, thread_worker, (void *)info);
        if (rc != 0) {
            fprintf(stderr, "Failure to create thread, status = %d\n", rc);
            exit(EXIT_FAILURE);
        }

        /* cpu affinity setup */
        if (cpu_affinity == 1)
        {
            CPU_ZERO(&cpuset);

            /* assigning thread to different cores */
            coreID = (i % core_count);
            CPU_SET(coreID, &cpuset);

            sts = pthread_setaffinity_np(info->th, sizeof(cpu_set_t), &cpuset);
            if (sts != 0)
            {
                fprintf(stderr, "pthread_setaffinity_np error, status = %d \n", sts);
                exit(EXIT_FAILURE);
            }
            sts = pthread_getaffinity_np(info->th, sizeof(cpu_set_t), &cpuset);
            if (sts != 0)
            {
                fprintf(stderr, "pthread_getaffinity_np error, status = %d \n", sts);
                exit(EXIT_FAILURE);
            }

            if (CPU_ISSET(coreID, &cpuset))
                printf("Thread %d assigned on CPU core %d\n", i, coreID);
        }
    }

    /* set all threads to ready condition */
    rc = pthread_mutex_lock(&mutex);
    if (rc != 0) {
        fprintf(stderr, "Failure to get Mutex Lock, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }

    while (ready_thread_count < thread_count)
    {
        rc = pthread_cond_wait(&ready_cond, &mutex);
        if (rc != 0) {
            fprintf(stderr, "Failure calling pthread_cond_wait, status = %d\n", rc);
            exit(EXIT_FAILURE);
        }
    }

    rc = pthread_mutex_unlock(&mutex);
    if (rc != 0) {
        fprintf(stderr, "Failure to release Mutex Lock, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* set all threads to startup finished condition */
    rc = pthread_mutex_lock(&mutex);
    if (rc != 0) {
        fprintf(stderr, "Failure to get Mutex Lock, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }

    while (startupfinished_thread_count < thread_count)
    {
        rc = pthread_cond_wait(&startupfinished_cond, &mutex);
        if (rc != 0) {
            fprintf(stderr, "Failure calling pthread_cond_wait, status = %d\n", rc);
            exit(EXIT_FAILURE);
        }
    }

    rc = pthread_mutex_unlock(&mutex);
    if (rc != 0) {
        fprintf(stderr, "Failure to release Mutex Lock, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }
    printf("Beginning test ....\n");
    /* all threads start at the same time */
    read_stat (1);
    gettimeofday(&start_time, NULL);
    rdtsc_start = rdtsc();
    rc = pthread_mutex_lock(&mutex);
    if (rc != 0) {
        fprintf(stderr, "Failure to get Mutex Lock, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }
    cleared_to_start = 1;
    rc = pthread_cond_broadcast(&start_cond);
    if (rc != 0) {
        fprintf(stderr, "Failure calling pthread_cond_broadcast, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }
    rc = pthread_mutex_unlock(&mutex);
    if (rc != 0) {
        fprintf(stderr, "Failure to release Mutex Lock, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }

    /* wait for other threads stop */
    rc = pthread_mutex_lock(&mutex);
    if (rc != 0) {
        fprintf(stderr, "Failure to get Mutex Lock, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }

    while (active_thread_count > 0) {
        rc = pthread_cond_wait(&stop_cond, &mutex);
        if (rc != 0) {
            fprintf(stderr, "Failure calling pthread_cond_wait, status = %d\n", rc);
            exit(EXIT_FAILURE);
        }
    }

    rc = pthread_mutex_unlock(&mutex);
    if (rc != 0) {
        fprintf(stderr, "Failure to release Mutex Lock, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }

    rdtsc_end = rdtsc();
    gettimeofday(&stop_time, NULL);
    read_stat (0);

    rc = pthread_mutex_lock(&mutex);
    if (rc != 0) {
        fprintf(stderr, "Failure to get Mutex Lock, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }

    while (stop_thread_count > 0) {
        rc = pthread_cond_wait(&end_cond, &mutex);
        if (rc != 0) {
            fprintf(stderr, "Failure calling pthread_cond_wait, status = %d\n", rc);
            exit(EXIT_FAILURE);
        }
    }

    rc = pthread_mutex_unlock(&mutex);
    if (rc != 0) {
        fprintf(stderr, "Failure to release Mutex Lock, status = %d\n", rc);
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < thread_count; i++)
    {
        if (pthread_join(tinfo[i].th, NULL))
            printf("Could not join thread id - %d !\n", i);
    }


    printf("All threads complete\n\n");

    if (failure_occured)
    {
        printf("AT LEAST ONE FAILURE OCCURED DURING THE TESTS - DO NOT TRUST THE FIGURES PRODUCED\n");
    }
    else
    {
       printf("# PASS verify for ZLIB\n");
    }

    /* generate report */
    elapsed = (stop_time.tv_sec - start_time.tv_sec) * 1000000 +
        (stop_time.tv_usec - start_time.tv_usec);


    /* Cast test_size * test_count to avoid int overflow */
    throughput = ((float)test_size * (float)actual_test_count *
                  bytes_to_bits / (float)elapsed);

    printf("Elapsed time   = %.3f msec\n", (float)elapsed / 1000);
    printf("Operations     = %d\n", actual_test_count);

    printf("Time per op    = %.3f usec (%d ops/sec)\n",
           (float)elapsed / actual_test_count,
           (int)((float)test_count * 1000000.0 / (float)elapsed));

    printf("Elapsed cycles = %llu\n", rdtsc_end - rdtsc_start);

    printf("Throughput     = %.2f (Mbps)\n", throughput);

    printf("\nCSV summary:\n");

    printf("Algorithm,"
           "Test_type,"
           "Deflate_buffering_enabled,"
           "Inflate_buffering_enabled,"
           "Compression_Level,"
           "Chunk_Size,"
           "Stream_type,"
           "Core_affinity,"
           "Elapsed_usec,"
           "Cores,"
           "Threads,"
           "Count,"
           "Data_per_test,"
           "Mbps,"
           "CPU_%%,"
           "User_%%,"
           "Kernel_%%,"
           "Ratio,"
           "Context_switches,"
           "Cycles\n");

    unsigned long cpu_time = 0;
    unsigned long cpu_user = 0;
    unsigned long cpu_kernel = 0;

    cpu_time = (cpu_time_total.user +
                cpu_time_total.nice +
                cpu_time_total.sys +
                cpu_time_total.io +
                cpu_time_total.irq +
                cpu_time_total.softirq) * CPU_TIME_MULTIPLIER / core_count;
    cpu_user = cpu_time_total.user * CPU_TIME_MULTIPLIER / core_count;
    cpu_kernel = cpu_time_total.sys * CPU_TIME_MULTIPLIER / core_count;

    printf("csv,%s,%d,%s,%s,%d,%d,%d,%s,%lu,%d,%d,%d,%d,%.2f,%lu,%lu,%lu,%.3f,%d,%llu\n",
           test_name(test_type),
           test_type,
           enable_deflate_buffering ? "Yes" : "No",
           enable_inflate_buffering ? "Yes" : "No",
           compression_level,
           chunk_size,
           stream_type,
           cpu_affinity ? "Yes" : "No",
           elapsed,
           core_count, thread_count, actual_test_count, test_size, throughput,
           cpu_time * CPU_PERCENTAGE_MULTIPLIER / elapsed,
           cpu_user * CPU_PERCENTAGE_MULTIPLIER / elapsed,
           cpu_kernel * CPU_PERCENTAGE_MULTIPLIER / elapsed,
           ratio,
           cpu_context.context,
           rdtsc_end-rdtsc_start);
}

void CHECK_ERR(int err, char *msg)
{
    if (err != Z_OK) {
        fprintf(stderr, "# FAIL %s error: %d\n", msg, err);
        exit(1);
    }
}

/******************************************************************************
* function:
*           main(int argc,
*                char *argv[])
*
* @param argc [IN] - input argument count
* @param argv [IN] - argument buffer
*
* description:
*    main function is used to define the testing type.
******************************************************************************/
int main(int argc, char *argv[])
{
    int i = 0;
    
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
            break;

        handle_option(argc, argv, &i);
    }

    if (i < argc)
    {
        fprintf(stderr,
                "This program does not take arguments, please use -h for usage.\n");
        exit(EXIT_FAILURE);
    }

    active_thread_count = thread_count;
    stop_thread_count = thread_count;
    ready_thread_count = 0;
    startupfinished_thread_count = 0;

    printf("\nzlib performance test application\n");
    printf("\nTest parameters:\n\n");
    printf("\tTest type:                        %d (%s)\n", test_type, test_name(test_type));
    printf("\tCompression level:                %d\n", compression_level);
    printf("\tStream type:                      %d (%s)\n", stream_type, streamtype_name(stream_type));
    printf("\tTest count:                       %d\n", test_count);
    printf("\tThread count:                     %d\n", thread_count);
    printf("\tNumber of cores:                  %d\n", core_count);
    printf("\tChunk size:                       %d\n", chunk_size);
    printf("\tCorpus used:                      %d (%s)\n", corpus, corpus_name(corpus));
    printf("\tBuffering in deflate enabled:     %s\n", enable_deflate_buffering ? "Yes" : "No");
    printf("\tBuffering in inflate enabled:     %s\n", enable_inflate_buffering ? "Yes" : "No");
    printf("\tAllow Partial Chunks:             %s\n", allow_partial_chunks ? "Yes" : "No");
    printf("\tCPU core affinity:                %s\n", cpu_affinity ? "Yes" : "No");
    printf("\tVerification:                     %s\n", verify ? "Yes" : "No");    

    printf("\n");

    performance_test();

    return 0;
}
