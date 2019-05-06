#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <getopt.h>


// Global counter that we get results from
long long counter;

// for measuring start/end times
struct timespec timeStructStart;
struct timespec timeStructEnd;

int numThreads = 1; // argument for --threads=
int numIterations = 1; // argument for --iterations=

// option flags
int opt_yield = 0; int mFlag = 0; int sFlag = 0;
int cFlag = 0; int opt_sync = 0; int syncFlag = 0;

FILE * fp; // for writing to CSV file
pthread_mutex_t lock;
int spinLock = 0; int cLock = 0;

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
    long long *counterPointer = (long long *) arg;
    long long sum;
    if (mFlag == 1)
    {
        pthread_mutex_lock(&lock);
        for (int i = 0; i < numIterations; i++)
        {
            sum = *counterPointer + 1;
            if (opt_yield)
            {
                sched_yield();
            }
            *counterPointer = sum;
        }
        pthread_mutex_unlock(&lock);
        pthread_mutex_lock(&lock);
        for (int i = 0; i < numIterations; i++)
        {
            sum = *counterPointer - 1;
            if (opt_yield)
            {
                sched_yield();
            }
            *counterPointer = sum;
        }
        pthread_mutex_unlock(&lock);
    }
    else if (sFlag == 1)
    {
        putSpinLock();
        for (int i = 0; i < numIterations; i++)
        {
            sum = *counterPointer + 1;
            if (opt_yield)
            {
                sched_yield();
            }
            *counterPointer = sum;
        }
        __sync_lock_release(&spinLock);
        putSpinLock();
        for (int i = 0; i < numIterations; i++)
        {
            sum = *counterPointer - 1;
            if (opt_yield)
            {
                sched_yield();
            }
            *counterPointer = sum;
        }
        __sync_lock_release(&spinLock);
    }
    else if (cFlag == 1)
    {
        long long temp;
        for (int i = 0; i < numIterations; i++)
        {
            do {
                 temp = *counterPointer;
                 sum = temp + 1;
                 if (opt_yield)
                 {
                      sched_yield();
                 }
                 
            } while (temp != __sync_val_compare_and_swap(counterPointer, temp, sum));
        }
        for (int i = 0; i < numIterations; i++)
        {
            do {
                 temp = *counterPointer;
                 sum = temp - 1;
                 if (opt_yield)
                 {
                      sched_yield();
                 }
                 
            } while (temp != __sync_val_compare_and_swap(counterPointer, temp, sum));
        }
    }
    else
    {
        for (int i = 0; i < numIterations; i++)
        {
            sum = *counterPointer + 1;
            if (opt_yield)
            {
                sched_yield();
            }
            *counterPointer = sum;
        }
        for (int i = 0; i < numIterations; i++)
        {
            sum = *counterPointer - 1;
            if (opt_yield)
            {
                sched_yield();
            }
            *counterPointer = sum;
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
    counter = 0;
    
    struct option options[] = {
        {"threads", 1, NULL, 't'},
        {"iterations", 1, NULL, 'i'},
        {"yield", 0, NULL, 'y'},
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
            opt_yield = 1;
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
            if (*optarg == 'c')
            {
                cFlag = 1;
            }
        }
        else
        {
            fprintf(stderr, "wrong and/or unknown option used, try again. Use --threads=# or --iterations=#\n or --sync=arg or --yield");
            exit(1); //unrecognized argument
        }
    }

    // Create the threads
    int i;
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
        threadSuccess = pthread_create(&thread[i], NULL, add, &counter);
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
    
    // Calculate values for CSV record
    long totalOps = 2*numThreads*numIterations;
    long elapsedTimeNano = timeStructEnd.tv_nsec - timeStructStart.tv_nsec;
    long timePerOp = elapsedTimeNano / totalOps;
    // Print CSV record
    if (opt_yield == 0 && syncFlag == 0)
    {
        fprintf(stdout, "add-none,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fp = fopen("lab2_add.csv","a");
        fprintf(fp, "add-none,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fclose(fp);
    }
    else if (opt_yield == 0 && syncFlag == 1 && mFlag == 1)
    {
        fprintf(stdout, "add-m,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fp = fopen("lab2_add.csv","a");
        fprintf(fp, "add-m,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fclose(fp);
    }
    else if (opt_yield == 0 && syncFlag == 1 && sFlag == 1)
    {
        fprintf(stdout, "add-s,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fp = fopen("lab2_add.csv","a");
        fprintf(fp, "add-s,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fclose(fp);
    }
    else if (opt_yield == 0 && syncFlag == 1 && cFlag == 1)
    {
        fprintf(stdout, "add-c,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fp = fopen("lab2_add.csv","a");
        fprintf(fp, "add-c,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fclose(fp);
    }
    else if (opt_yield == 1 && syncFlag == 0)
    {
        fprintf(stdout, "add-yield-none,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fp = fopen("lab2_add.csv","a");
        fprintf(fp, "add-yield-none,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fclose(fp);
    }
    else if (opt_yield == 1 && syncFlag == 1 && mFlag == 1)
    {
        fprintf(stdout, "add-yield-m,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fp = fopen("lab2_add.csv","a");
        fprintf(fp, "add-yield-m,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fclose(fp);
    }
    else if (opt_yield == 1 && syncFlag == 1 && sFlag == 1)
    {
        fprintf(stdout, "add-yield-s,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fp = fopen("lab2_add.csv","a");
        fprintf(fp, "add-yield-s,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fclose(fp);
    }
    else if (opt_yield == 1 && syncFlag == 1 && cFlag == 1)
    {
        fprintf(stdout, "add-yield-c,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fp = fopen("lab2_add.csv","a");
        fprintf(fp, "add-yield-c,%d,%d,%ld,%ld,%ld,%lld\n", numThreads, numIterations, totalOps, elapsedTimeNano, timePerOp, counter);
        fclose(fp);
    }
    
    if (mFlag == 1)
    {
        pthread_mutex_destroy(&lock);
    }
    
    exit(0);
}
