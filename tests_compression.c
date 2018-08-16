#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "zlib.h"
#include "tests.h"

int
startup_corpus_compression(test_parameters_t* test_parameters)
{
    int numFiles = 0, i = 0;
    int attemptCalgary2 = 0;
    char **pCorpusFileNamesArray = NULL;
    char* fullPathAndFilename;
    unsigned long totalFilesize = 0, individualFilesize = 0, numBytesRead = 0;
    unsigned long spaceRemaining = 0;

    char *customFileNames [] =
    {
            "customfile.bin"
    };

    char *canterburyFileNames [] =
    {
            "alice29.txt", "asyoulik.txt", "cp.html",
            "fields.c","grammar.lsp", "kennedy.xls", "lcet10.txt" ,
            "plrabn12.txt", "ptt5"
    };

    /* Large Calgary Corpus */
    char *calgaryFileNames [] =
    {
            "bib", "book1", "book2", "geo" , "news", "obj1",
            "obj2", "paper1", "paper2", "paper3", "paper4",
            "paper5", "paper6", "pic", "progc",
            "progl" , "progp" ,"trans"
    };
    /* QA performance sample code uses "calgary" file.
     * "calgary" is the concatenation of all the above files
     * in the large calgary corpus.
     * This code accepts "calgary" as an alternative to all
     * the individual files above.
     * Note calgaryFileNames and calgaryFileNames2 are the same
     * input data */
    char *calgaryFileNames2 [] =
    {
            "calgary"
    };

    char *silesiaFileNames [] =
    {
            "dickens", "mozilla", "mr", "nci" , "ooffice", "osdb",
            "reymont", "samba", "sao", "webster", "xml",
            "x-ray"
    };

    switch(test_parameters->corpus)
    {
        case CUSTOM_FILE:
            pCorpusFileNamesArray = customFileNames;
            numFiles = sizeof(customFileNames)/sizeof(char *);
            break;
        case CANTERBURY_CORPUS:
            pCorpusFileNamesArray = canterburyFileNames;
            numFiles = sizeof(canterburyFileNames)/sizeof(char *);
            break;
        case CALGARY_CORPUS:
            pCorpusFileNamesArray = calgaryFileNames;
            numFiles = sizeof(calgaryFileNames)/sizeof(char *);
            break;
        case SILESIA_CORPUS:
            pCorpusFileNamesArray = silesiaFileNames;
            numFiles = sizeof(silesiaFileNames)/sizeof(char *);
            break;
        default:
            pCorpusFileNamesArray = calgaryFileNames;
            numFiles = sizeof(calgaryFileNames)/sizeof(char *);
            break;
    }

    const char* path = "/lib/firmware/";
    FILE *testfile = NULL;
    if (test_parameters->file_path[0] != '\0') {
        path=test_parameters->file_path;
    }

    /* Workout total size of corpus files */
    for(i=0; i<numFiles; i++) {
        fullPathAndFilename = malloc(strlen(path) + strlen(pCorpusFileNamesArray[i]) + 1);
        if (NULL == fullPathAndFilename) {
            fprintf(stderr, "# FAIL: Could not allocate space for Filename.\n");
            return TEST_FAILED;
        }

        strcpy(fullPathAndFilename, path);
        strcat(fullPathAndFilename, pCorpusFileNamesArray[i]);
        testfile = fopen(fullPathAndFilename, "rb");
        if (testfile) {
            fseek(testfile, 0, SEEK_END);
            totalFilesize+=ftell(testfile);
            fclose(testfile);
        }
        else {
            /* In the Calgary Case try calgaryFileNames2 */
            if((test_parameters->corpus != CALGARY_CORPUS) || (attemptCalgary2 == 1))
            {
               fprintf(stderr, "# FAIL: Could not open file: %s\n", fullPathAndFilename);
               free(fullPathAndFilename);
               fullPathAndFilename = NULL;
               return TEST_FAILED;
            }
            else
            {
               attemptCalgary2 = 1;
               pCorpusFileNamesArray = calgaryFileNames2;
               numFiles = sizeof(calgaryFileNames2)/sizeof(char *);
               free(fullPathAndFilename);
               fullPathAndFilename = NULL;
               totalFilesize=0;
               i=-1;
               continue;
            }
        }
        free(fullPathAndFilename);
        fullPathAndFilename = NULL;
    }

    if (totalFilesize < test_parameters->chunksize) {
        fprintf(stderr, "# FAIL: Chunksize: %d is greater then the total filesize: %ld, this is not allowed\n", test_parameters->chunksize, totalFilesize);
        return TEST_FAILED;
    }

    /* Allocate buffers and set size */
    if (test_parameters->allow_partial_chunks) {
        test_parameters->input_buflen=totalFilesize;
    }
    else {
        test_parameters->input_buflen=(totalFilesize - (totalFilesize % test_parameters->chunksize));
    }
    /* Allow a hundred bytes extra in case we do verification and need some extra room in
       case the file decompresses incorrectly and because we don't empty the buffer we need
       at least a byte of additional space. */
    test_parameters->input_buf=(unsigned char *)malloc(test_parameters->input_buflen+100);
    /* Create the output buffer slightly bigger as if you try and compress data
       that won't compress it will slightly expand. The magic numbers to ensure
       enough space are to multiply by 9 then dividing by 8 and then add 5. */
    test_parameters->output_buflen=((test_parameters->input_buflen*9)/8) + 5;
    test_parameters->output_buf=(unsigned char*)malloc(test_parameters->output_buflen);
    spaceRemaining = test_parameters->input_buflen;

    /* Read all corpus files into input buffer */
    for(i=0; i<numFiles; i++)
    {
        individualFilesize=0;
        fullPathAndFilename = malloc(strlen(path) + strlen(pCorpusFileNamesArray[i]) + 1);
        if (NULL == fullPathAndFilename) {
            fprintf(stderr, "# FAIL: Could not allocate space for Filename.\n");
            return TEST_FAILED;
        }
        strcpy(fullPathAndFilename, path);
        strcat(fullPathAndFilename, pCorpusFileNamesArray[i]);
        testfile = fopen(fullPathAndFilename, "rb");
        if (testfile) {
            fseek(testfile, 0, SEEK_END);
            individualFilesize=ftell(testfile);
            if (individualFilesize > spaceRemaining)
                individualFilesize = spaceRemaining;
            fseek(testfile, 0, SEEK_SET);
            fread(test_parameters->input_buf+numBytesRead,
                individualFilesize, 1, testfile);
            if (ferror(testfile)) {
                fprintf(stderr, "# FAIL: Error: reading from file\n");
                free(fullPathAndFilename);
                fullPathAndFilename = NULL;
                fclose(testfile);
                return TEST_FAILED;
            }
            numBytesRead+=individualFilesize;
            spaceRemaining-=individualFilesize;
            fclose(testfile);
        }
        else {
            fprintf(stderr, "# FAIL: Could not open file: %s\n", fullPathAndFilename);
            free(fullPathAndFilename);
            fullPathAndFilename = NULL;
            return TEST_FAILED;
        }
        free(fullPathAndFilename);
        fullPathAndFilename = NULL;
    }

	if (test_parameters->verify) {
        test_parameters->verify_checksum=0;
        test_parameters->verify_checksum = crc32(0, test_parameters->input_buf,(uInt)test_parameters->input_buflen);
    }

    return TEST_PASSED;
}



int
run_corpus_compression(test_parameters_t* test_parameters)
{
   int ret = 0;
   int i = 0;
   int flush;
   int failed=TEST_PASSED;
   int windowbits;
   unsigned long totalout = 0;

   windowbits = MAX_WBITS;

   switch(test_parameters->streamtype)
   {
       case RAW_DEFLATE_STREAM:
           windowbits = -windowbits;
           break;
       case ZLIB_DEFLATE_STREAM:
           /* Do nothing windowbits is correct */
           break;
       case GZIP_DEFLATE_STREAM:
           windowbits += 16;
           break;
       default:
           /* Default to gzip encoding */
           windowbits += 16;
           break;
   }

   for (i = 0; i < test_parameters->count; i++) {
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.next_out = (void *)test_parameters->output_buf;
        strm.avail_out = test_parameters->output_buflen;
        strm.total_out = 0;

        /* Set the flush flag according to command line parameter..
         * default value is to buffer within zlib shim */
        if (test_parameters->enable_deflate_buffering)
            flush=Z_NO_FLUSH;
        else
            flush=Z_SYNC_FLUSH;

        ret = deflateInit2(&strm, test_parameters->level, 8, windowbits, 8, 0);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR) {
            failed=TEST_FAILED;
        }

        if (TEST_PASSED == failed) {
	    do {
                strm.next_in = (void *)test_parameters->input_buf+strm.total_in;
                if (strm.total_in+test_parameters->chunksize >= test_parameters->input_buflen) {
                    strm.avail_in = test_parameters->input_buflen - strm.total_in;
                    flush = Z_FINISH;
                }
                else {
                    strm.avail_in = test_parameters->chunksize;
                }
                ret = deflate(&strm, flush);
                strm.avail_out = test_parameters->output_buflen - strm.total_out;
            } while (ret == Z_OK);

            if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR) {
                printf("# FAIL: deflate stream corrupt, ret:%d \r\n", ret);
                failed=TEST_FAILED;
            }
        }

        test_parameters->single_call_bytes = strm.total_in;
        test_parameters->ratio = (float)strm.total_out / strm.total_in;

		totalout = strm.total_out;

        ret = deflateEnd(&strm);
        if (ret != Z_OK) {
            printf("# FAIL: deflateEnd failed, ret:%d \r\n", ret);
            failed=TEST_FAILED;
        }

    }

    /* Update the compressed length, so it can be properly decompressed... */
    test_parameters->output_buflen = totalout;
    return failed;
}



int
shutdown_corpus_compression(test_parameters_t* test_parameters)
{
    z_stream strm;
    int ret = 0;
    int failed=TEST_PASSED;
    int flush;
    int windowbits;
    unsigned long verify_checksum = 0;

    if (test_parameters->input_buf) {
        if (test_parameters->verify) {
            windowbits = MAX_WBITS;

            switch(test_parameters->streamtype)
            {
                case RAW_DEFLATE_STREAM:
                    windowbits = -windowbits;
                break;
                case ZLIB_DEFLATE_STREAM:
                    /* Do nothing windowbits is correct */
                break;
                case GZIP_DEFLATE_STREAM:
                    windowbits += 16;
                break;
                default:
                    /* Default to gzip encoding */
                    windowbits += 16;
                break;
            }

            ret = Z_OK;
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;
            /* Note: Input buffer and Output Buffer are swapped over for the decompression. */
            strm.next_out = (void *)test_parameters->input_buf;
            /* Add one hundred to strm.avail_out to work around the fact that for performance timings
               we are not emptying the buffer we decompress to (input buffer in this
               case) */
            strm.avail_out = test_parameters->input_buflen+100;
            strm.total_out = 0;

            /* Set the flush flag according to command line parameter..
             * default value is to buffer within zlib shim */
            if (test_parameters->enable_inflate_buffering)
                flush=Z_NO_FLUSH;
            else
                flush=Z_SYNC_FLUSH;

            ret = inflateInit2(&strm, windowbits);
            if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR) {
                fprintf(stderr,"# FAIL: deflate stream corrupt on Inflate init\n");
                failed = TEST_FAILED;
            }
            strm.next_in = (void *)test_parameters->output_buf;
            strm.avail_in = test_parameters->output_buflen;
            if (TEST_PASSED == failed) {
                do {
                    strm.next_in = (void *)test_parameters->output_buf+strm.total_in;
                    if (strm.total_in+test_parameters->chunksize >= test_parameters->output_buflen) {
                        strm.avail_in = test_parameters->output_buflen - strm.total_in;
                        flush = Z_FINISH;
                    }
                    else {
                        strm.avail_in = test_parameters->chunksize;
                    }
                    ret = inflate(&strm, flush);
                } while (ret == Z_OK);

                if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR) {
                   fprintf(stderr,"# FAIL: deflate stream corrupt on Inflate\n");
                   failed = TEST_FAILED;
                }
            }
            inflateEnd(&strm);

            verify_checksum = crc32(0, test_parameters->input_buf, (uInt)test_parameters->input_buflen);
            if (test_parameters->verify_checksum == verify_checksum && TEST_PASSED == failed) {
                fprintf(stderr, "\nVerification: PASS\n\n");
            }
            else {
                fprintf(stderr, "\nVerification: FAIL\n\n");
                failed = TEST_FAILED;
            }
        }

        free(test_parameters->input_buf);
        test_parameters->input_buf = NULL;
        test_parameters->input_buflen = 0;
    }
    if (test_parameters->output_buf) {
        free(test_parameters->output_buf);
        test_parameters->output_buf = NULL;
        test_parameters->output_buflen = 0;
    }
    return failed;
}



/******************************************************************************
* function:
*     tests_startup_corpus_compression  (test_parameters_t* test_parameters)
*
* @param test_parameters [IN] - struct containing all the parameters/buffers used.
*                               it is passed in as a pointer as some of the values will get updated
*                               within the function. Specifically the input/output buffers and lengths
*                               will get created/set within this function.
*
* description:
*	setup a corpus compression job where we run compression across a buffer of concatinated corpus files
*
******************************************************************************/
int
tests_startup_corpus_compression(test_parameters_t* test_parameters)
{
   return startup_corpus_compression(test_parameters);
}

/******************************************************************************
* function:
*     tests_run_corpus_compression  (test_parameters_t* test_parameters)
*
* @param test_parameters [IN] - struct containing all the parameters/buffers used.
*                               it is passed in as a pointer as some of the values will get updated
*                               within the function.
*
* description:
*	run a corpus compression job where we run compression across a buffer of concatinated corpus files
*
******************************************************************************/
int
tests_run_corpus_compression(test_parameters_t* test_parameters)
{
    return run_corpus_compression(test_parameters);
}

/******************************************************************************
* function:
*     tests_shutdown_corpus_compression  (test_parameters_t* test_parameters)
*
* @param test_parameters [IN] - struct containing all the parameters/buffers used.
*                               it is passed in as a pointer as some of the values will get updated
*                               within the function. Specifically the input/output buffers and lengths
*                               will get freed/set to zero within this function.
*
* description:
*	shutdown a corpus compression job where we run compression across a buffer of concatinated corpus files
*
******************************************************************************/
int
tests_shutdown_corpus_compression(test_parameters_t* test_parameters)
{
    return shutdown_corpus_compression(test_parameters);
}


