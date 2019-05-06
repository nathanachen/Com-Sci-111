#define _POSIX_C_SOURCE 199309L
#include "SortedList.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>

int opt_yield = 0;
int yFlag = 0; int mFlag = 0; int sFlag = 0; int syncFlag = 0;

// for measuring start/end times
struct timespec timeStructStart;
struct timespec timeStructEnd;

int numThreads = 1; // argument for --threads=
int numIterations = 1; // argument for --iterations=

SortedListElement_t *elem;
SortedList_t * head;
FILE * fp; // for writing to CSV file

pthread_mutex_t lock;
int spinLock = 0;

void handl(int sigNo){
    fprintf(stderr, "ERROR: Segmentation fault. numThreads = %d, numIterations = %d, signalno = %d\n", numThreads, numIterations,sigNo);
    exit(2);
}

void putSpinLock()
{
    while(__sync_lock_test_and_set(&spinLock, 1))
    {
        while(spinLock == 1)
            ;
    }
}

void *add(void *arg)
{
    if (mFlag == 1)
    {
        long start = (long) arg;
        long num; long len;
        for(num = start * numThreads; num < numIterations; num++)
        {
            pthread_mutex_lock(&lock);
            SortedList_insert(head, &elem[num]);
            pthread_mutex_unlock(&lock);
        }
        pthread_mutex_lock(&lock);
        len = SortedList_length(head);
        pthread_mutex_unlock(&lock);
        num = len - len + numThreads * start;
        for(num = start * numThreads; num < numIterations; num++)
        {
            pthread_mutex_lock(&lock);
            SortedListElement_t * del = SortedList_lookup(head, elem[num].key);
            if (del == NULL)
            {
                fprintf(stderr, "ERROR: Unable to lookup. \n");
                exit(2);
            }
            SortedList_delete(del);
            pthread_mutex_unlock(&lock);
        }
    }
    else if (sFlag == 1)
    {
        long start = (long) arg;
        long num; long len;
        for(num = start * numThreads; num < numIterations; num++)
        {
            putSpinLock();
            SortedList_insert(head, &elem[num]);
            __sync_lock_release(&spinLock);
        }
        putSpinLock();
        len = SortedList_length(head);
        __sync_lock_release(&spinLock);
        num = len - len + numThreads * start;
        for(num = start * numThreads; num < numIterations; num++)
        {
            putSpinLock();
            SortedListElement_t * del = SortedList_lookup(head, elem[num].key);
            if (del == NULL)
            {
                fprintf(stderr, "ERROR: Unable to lookup. \n");
                exit(2);
            }
            SortedList_delete(del);
            __sync_lock_release(&spinLock);
        }
    }
    else
    {
        long start = (long) arg;
        long num; long len;
        for(num = start * numThreads; num < numIterations; num++)
        {
            SortedList_insert(head, &elem[num]);
        }
        len = SortedList_length(head);
        num = len - len + numThreads * start;
        for(num = start * numThreads; num < numIterations; num++)
        {
            SortedListElement_t * del = SortedList_lookup(head, elem[num].key);
            if (del == NULL)
            {
                fprintf(stderr, "ERROR: Unable to lookup. \n");
                exit(2);
            }
            SortedList_delete(del);
        }
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    int startTimeSuccess = clock_gettime(CLOCK_REALTIME, &timeStructStart);
    if (startTimeSuccess != 0)
    {
        fprintf(stderr, "ERROR: Unable to get starting time. \n");
        exit(2);
    }
    int um;
    struct option options[] = {
        {"threads", 1, NULL, 't'},
        {"iterations", 1, NULL, 'i'},
        {"yield", 1, NULL, 'y'},
        {"sync", 1, NULL, 's'},
        {0, 0, 0, 0} };
        
    // Parse command line
    while ((um = getopt_long(argc, argv, "", options, NULL)) != -1)
    {
        if (um == 't')
        {
            numThreads = atoi(optarg);
            if (numThreads < 0)
            {
                fprintf(stderr, "ERROR: Use --threads=# \n");
                exit(1);
            }
        }
        else if (um == 'i')
        {
            numIterations = atoi(optarg);
            if (numIterations <= 0)
            {
                fprintf(stderr, "ERROR: Use --iterations=# \n");
                exit(1);
            }
        }
        else if (um == 'y')
        {
            yFlag = 1;
            for (unsigned int i = 0; i < strlen(optarg); i++)
            {
                if (optarg[i] == 'l') {
                    opt_yield = opt_yield | LOOKUP_YIELD;
                }
                else if (optarg[i] == 'i')
                {
                    opt_yield = opt_yield | INSERT_YIELD;
                }
                else if (optarg[i] == 'd'){
                    opt_yield = opt_yield | DELETE_YIELD;
                }
                else
                {
                    fprintf(stderr, "ERROR: Argument not recognized. \n");
                    exit(1);
                }
            }
        }
        else if (um == 's')
        {
            syncFlag = 1;
            if (*optarg == 'm')
            {
                mFlag = 1;
            }
            if (*optarg == 's')
            {
                sFlag = 1;
            }
        }
        else
        {
            fprintf(stderr, "wrong and/or unknown option used, try again. Use --threads=# or --iterations=#\n or --yield=arg");
            exit(1); //unrecognized argument
        }
    }
    
    // Handle segmentation faults
    signal(SIGSEGV, handl);
    
    // initialize empty list
    head = (SortedList_t *) malloc(sizeof(SortedList_t));
    head->next = head;
    head->prev = head;
    head->key = NULL;
    
    int numElem = numThreads * numIterations;
    elem = (SortedListElement_t*) malloc((numElem) * sizeof(SortedListElement_t));
    
    // creates and initializes (with random keys) the required number (threads x iterations) of list elements
    int len = 5;
    unsigned int timevar = (unsigned int)time(NULL);
    srand(timevar);
    for (int i = 0; i < numElem; i++)
    {
        char * characters = malloc((len + 1) * sizeof(char));
        for (int j = 0; j < len; j++)
        {
            int off = rand() % 26;
            characters[j] = 'a' + off;
        }
        characters[len] = '\0';
        elem[i].key = characters;
    }
    // Create the threads
    long i;
    int threadSuccess;
    pthread_t *thread;
    if (mFlag == 1)
    {
        int result = pthread_mutex_init(&lock, NULL);
        if (result != 0)
        {
            fprintf(stderr, "ERROR: Unable to initialize mutex. \n");
            exit(1);
        }
    }
    thread = (pthread_t *) malloc (numThreads*sizeof(pthread_t));
    for (i = 0; i < numThreads; i++)
    {
        threadSuccess = pthread_create(&thread[i], NULL, add, (void *)i);
        if (threadSuccess != 0)
        {
            fprintf(stderr, "ERROR: Failed to pthread_create. \n");
            exit(2);
        }
    }
    // Join threads
    for (i = 0; i < numThreads; i++)
    {
        threadSuccess = pthread_join(thread[i], NULL);
        if (threadSuccess != 0)
        {
            fprintf(stderr, "ERROR: Failed to use pthread_join. \n");
            exit(2);
        }
    }
    
    // Note end time
    int endTimeSuccess = clock_gettime(CLOCK_REALTIME, &timeStructEnd);
    if (endTimeSuccess != 0)
    {
        fprintf(stderr, "ERROR: Unable to get ending time. \n");
        exit(2);
    }
    
    // CSV
    long totalOps = 3*numThreads*numIterations;
    long elapsedTimeNano = timeStructEnd.tv_nsec - timeStructStart.tv_nsec;
    long timePerOp = elapsedTimeNano / totalOps;
    
    if (yFlag == 0)
    {
        if (mFlag == 1)
        {
            fprintf(stdout, "list-none-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-none-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else if (sFlag == 1)
        {
            fprintf(stdout, "list-none-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-none-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else
        {
            fprintf(stdout, "list-none-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-none-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
    }
    else if (yFlag != 0 && (opt_yield & INSERT_YIELD) != 0 && (opt_yield & DELETE_YIELD) == 0 && (opt_yield & LOOKUP_YIELD) == 0)
    {
        if (mFlag == 1)
        {
            fprintf(stdout, "list-i-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-i-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else if (sFlag == 1)
        {
            fprintf(stdout, "list-i-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-i-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else
        {
            fprintf(stdout, "list-i-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-i-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
    }
    else if(yFlag != 0 && (opt_yield & INSERT_YIELD) == 0 && (opt_yield & DELETE_YIELD) != 0 && (opt_yield & LOOKUP_YIELD) == 0)
    {
        if (mFlag == 1)
        {
            fprintf(stdout, "list-d-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-d-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else if (sFlag == 1)
        {
            fprintf(stdout, "list-d-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-d-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else
        {
            fprintf(stdout, "list-d-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-d-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
    }
    else if (yFlag != 0 && (opt_yield & INSERT_YIELD) == 0 && (opt_yield & DELETE_YIELD) == 0 && (opt_yield & LOOKUP_YIELD) != 0)
    {
        if (mFlag == 1)
        {
            fprintf(stdout, "list-l-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-l-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else if (sFlag == 1)
        {
            fprintf(stdout, "list-l-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-l-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else
        {
            fprintf(stdout, "list-l-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-l-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
    }
    else if (yFlag != 0 && (opt_yield & INSERT_YIELD) != 0 && (opt_yield & DELETE_YIELD) != 0 && (opt_yield & LOOKUP_YIELD) == 0)
    {
        if (mFlag == 1)
        {
            fprintf(stdout, "list-id-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-id-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else if (sFlag == 1)
        {
            fprintf(stdout, "list-id-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-id-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else
        {
            fprintf(stdout, "list-id-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-id-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
    }
     else if (yFlag != 0 && (opt_yield & INSERT_YIELD) == 0 && (opt_yield & DELETE_YIELD) != 0 && (opt_yield & LOOKUP_YIELD) != 0)
     {
        if (mFlag == 1)
        {
            fprintf(stdout, "list-dl-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-dl-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else if (sFlag == 1)
        {
            fprintf(stdout, "list-dl-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-dl-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else
        {
             fprintf(stdout, "list-dl-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-dl-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
     }
    else if(yFlag != 0 && (opt_yield & INSERT_YIELD) != 0 && (opt_yield & DELETE_YIELD) == 0 && (opt_yield & LOOKUP_YIELD) != 0)
    {
        if (mFlag == 1)
        {
            fprintf(stdout, "list-il-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-il-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else if (sFlag == 1)
        {
            fprintf(stdout, "list-il-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-il-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else
        {
            fprintf(stdout, "list-il-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-il-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
    }
    else if (yFlag != 0 && (opt_yield & INSERT_YIELD) != 0 && (opt_yield & DELETE_YIELD) != 0 && (opt_yield & LOOKUP_YIELD) != 0)
    {
        if (mFlag == 1)
        {
            fprintf(stdout, "list-idl-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-idl-m,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else if (sFlag == 1)
        {
            fprintf(stdout, "list-idl-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-idl-s,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
        else
        {
            fprintf(stdout, "list-idl-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fp = fopen("lab2_list.csv","a");
            fprintf(fp, "list-idl-none,%d,%d,%d,%ld,%ld,%ld\n", numThreads, numIterations, 1, totalOps, elapsedTimeNano, timePerOp);
            fclose(fp);
        }
    }
}
