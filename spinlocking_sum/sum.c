#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <stdatomic.h>
#include <sys/time.h>


long sharedSumResult = 0;

typedef struct lockType {
    volatile atomic_flag flag;
} lockType;

typedef struct sumArgs {
    long start;
    long end;
    signed char *nArray;
    volatile atomic_flag *lock;
} sumArgs;

long getMicrotime(){
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

// Aquire exclusive access to critical zone
void aquire(volatile atomic_flag *lock) {
    while (atomic_flag_test_and_set(lock)==1) {
        // Busy-wait loop
    } 
};

// Release critical-zone access lock
void release(volatile atomic_flag *lock) {
    atomic_flag_clear(lock);

};

/*
    Perform sum on part of the array, with critical zone control.
*/
void sum(sumArgs *arguments) {
    
    printf("thread spawned with range [%ld, %ld]\n", arguments -> start, arguments -> end);
    long start = arguments -> start;
    long partSum = 0;
    for (;start < arguments->end; start++) {
        partSum = partSum + arguments -> nArray[start];
    }

        // Control critical zone access
        aquire(arguments -> lock);
        sharedSumResult = sharedSumResult + partSum;
        release(arguments -> lock);
};

/*
    Dynamically allocate the random vector
*/
signed char* generateRandomVector(long size) {
    signed char* vector = (signed char*)malloc(size * sizeof(signed char));
    if (vector == NULL) {
        printf("Memory allocation failed.\n");
        exit(1);
    }

    long i;
    for (i = 0; i < size; i++) {
        vector[i] = rand() % 201 - 100; // Range: -100 to 100
    }

    return vector;
}


int main(int argc, char *argv[]) {

    // Parse N
    char *nArg;
    long sizeN = strtol(argv[1], &nArg, 10);
    printf("Starting execution with N=%ld\n", sizeN);

    // Parse K
    char *kArg;
    int threads = (int) strtol(argv[2], &kArg, 10);
    printf("Using %d threads.\n", threads);

    // Initialize random number generator
    srand(time(NULL));
    int binSize =  floor(sizeN/threads);
    printf("Bin size: %d\n", binSize);

    // Initialize lock
    volatile atomic_flag lock;
    atomic_flag_clear(&lock);

    // Create the sizeN
    signed char* arrayN = generateRandomVector(sizeN);

    // Sum with one thread to compare results
    printf("Single-threaded sum result: ");
    long baselineSum = 0;

    for (long i = 0; i < sizeN; i++) {
        baselineSum = baselineSum + arrayN[i];
    }
    printf("%ld \n", baselineSum);

    // Time OP
    long start, end; 
    start = getMicrotime();
     
    // Dynamic memory allocation for n-threads
    pthread_t * thread = malloc(sizeof(pthread_t)*threads);
    sumArgs *args;

    for (int i = 0; i < threads; i++) {

        // Dynamically create args distributing sum
        args = (sumArgs*)malloc(sizeof(sumArgs));
        args -> start = i * binSize;
        args -> end = (i + 1) * binSize;
        args -> nArray = arrayN;
        args -> lock = &lock;

        // On the last thread, assign end to sizeN to prevent segfault
        if (i == threads - 1) {
            args -> end = sizeN;
        }

        // Create threads
        if(pthread_create(&thread[i], NULL, &sum, (sumArgs *) args)) {
            printf ("Failed to spawn thread.\n");
            exit (1);
        }
    }

    // Join all live threads
    for (int i = 0; i < threads; i++) {
        pthread_join(thread[i], NULL);
    }

    // Compute and print time spent
    end= getMicrotime();

    // Check sums
    if (sharedSumResult != baselineSum) {
        printf("Error!\nBaseline: %ld\nThreaded: %ld\n", baselineSum, sharedSumResult);
        exit(1);
    }

    double diff = (end-start) * 1e-6;
    printf("CPU time spent: %lf s\n", diff);
    printf("Result %ld matches with single-threaded sum\n", sharedSumResult);
    free(arrayN);

    return 0;
}
