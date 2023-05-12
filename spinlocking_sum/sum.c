#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <time.h>


typedef struct sumArgs {
    int start;
    int end;
    char *nArray;
} sumArgs;

void sum(sumArgs *arguments) {
    
    printf("%d - %d thread\n", arguments -> start, arguments -> end);
    printf("N start %d\n", arguments -> nArray[arguments -> start]);
    long start = arguments -> start;
    long partSum = 0;
    for (start; start < arguments->end; start++) {
        arguments -> nArray[start] + 1;
    }
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

int main(void) {

    // Initialize random number generator
    srand(time(NULL));
    long sizeN = 10000000;
    printf("Starting execution with N=%ld\n", sizeN);
    int threads = 3;
    int binSize =  floor(sizeN/threads);
    printf("Bin size: %d\n", binSize);

    // Create the sizeN
    signed char* arrayN = generateRandomVector(10000000);

    // Sum with one thread to compare results
    printf("Single-threaded sum result: ");
    long baselineSum = 0;

    for (long i = 0; i < sizeN; i++) {
        baselineSum = baselineSum + arrayN[i];
    }
    printf("%ld \n", baselineSum);


    printf("Starting execution with %d threads.\n", threads);
    #include <time.h>
     
    // Time OP
    clock_t start, end; 
    start = clock();
     
    // Dynamic memory allocation for n-threads
    pthread_t * thread = malloc(sizeof(pthread_t)*threads);
    sumArgs *args;

    for (int i = 0; i < threads; i++) {

        // Dynamically create args distributing sum
        args = (sumArgs*)malloc(sizeof(sumArgs));
        args -> start = i * binSize;
        args -> end = (i + 1) * binSize;
        args -> nArray = arrayN;

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
    end = clock();
    printf("CPU time spent: %f s\n", ((double) (end - start)) / CLOCKS_PER_SEC);
    free(arrayN);

    return 0;
}
