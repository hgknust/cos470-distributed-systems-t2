#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


typedef struct sumArgs {
    int start;
    int end;
    char *nArray;
} sumArgs;

void sum(sumArgs *arguments) {
    
    printf("%d - thread\n", arguments -> start);
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

    // Create the sizeN
    signed char* arrayN = generateRandomVector(10000000);

    // Sum with one thread to compare results
    printf("Single-threaded sum result: ");
    long baselineSum = 0;

    for (long i = 0; i < sizeN; i++) {
        baselineSum = baselineSum + arrayN[i];
    }
    printf("%ld \n", baselineSum);


    int threads = 4;
    printf("Starting execution with %d threads.\n", threads);

    // Dynamic memory allocation for n-threads
    pthread_t * thread = malloc(sizeof(pthread_t)*threads);

    for (int i = 0; i < threads; i++) {

        sumArgs args;
        args.start = i;
        args.end = 7;
        args.nArray = arrayN;

        // Create threads
        if(pthread_create(&thread[i], NULL, &sum, (sumArgs *) &args)) {
            printf ("Falha ao criar thread.\n");
            exit (1);
        }
    }

    // Join all live threads
    for (int i = 0; i < threads; i++) {
        pthread_join(thread[i], NULL);
    }

    free(arrayN);

    return 0;
}
