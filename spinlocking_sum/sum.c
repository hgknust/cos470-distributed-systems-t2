#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


void sum(int start) {
    printf("%d - thread\n", start);
};

int main(void) {

    int threads = 10;
    printf("Iniciando a execução com %d threads.\n", threads);

    // Dynamic memory allocation for n-threads
    pthread_t * thread = malloc(sizeof(pthread_t)*threads);

    for (int i = 0; i < threads; i++) {

        // Create threads
        if(pthread_create(&thread[i], NULL, &sum, i)) {
            printf ("Falha ao criar thread.\n");
            exit (1);
        }
    }

    // Join all live threads
    for (int i = 0; i < threads; i++) {
        pthread_join(thread[i], NULL);
    }

    return 0;
}
