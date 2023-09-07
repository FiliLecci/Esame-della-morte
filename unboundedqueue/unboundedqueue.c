#include <stdlib.h>
#include <semaphore.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#include "unboundedqueue.h"

pthread_mutex_t mutex; // mutex
Client_req *testa, *coda;
int numeroRichieste;

void initCoda()
{
    numeroRichieste = 0;
    pthread_mutex_init(&mutex, NULL);
    // inizializzo testa e coda
    testa = malloc(sizeof(Client_req));
    coda = malloc(sizeof(Client_req));
}

void destroyCoda()
{
    printf("Distruzione della coda...\n");
    pthread_mutex_destroy(&mutex);
    free(testa);
    free(coda);
}

void push(Client_req *req)
{
    pthread_mutex_lock(&mutex);

    if (numeroRichieste <= 0)
    {
        testa = req;
        coda = req;
    }
    else
    {
        coda->next = req;
        coda = req;
    }
    numeroRichieste++;

    pthread_mutex_unlock(&mutex);
}

Client_req *pop()
{
    pthread_mutex_lock(&mutex);
    if (numeroRichieste <= 0)
    {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    Client_req *retReq;

    retReq = testa;
    testa = testa->next;

    retReq->next = NULL;
    numeroRichieste--;

    pthread_mutex_unlock(&mutex);

    return retReq;
}