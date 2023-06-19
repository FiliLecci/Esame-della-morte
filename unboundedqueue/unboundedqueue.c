#include <stdlib.h>
#include <semaphore.h>
#include <string.h>
#include <pthread.h>

#include "unboundedqueue.h"

pthread_mutex_t *mutex; // mutex
pthread_cond_t *cv;
Client_req *testa, *coda;
int numeroRichieste;

void initCoda()
{
    numeroRichieste = 0;
    pthread_mutex_init(mutex, NULL);
    pthread_cond_init(cv, NULL);
}

void destroyCoda()
{
    pthread_mutex_destroy(mutex);
    pthread_cond_destroy(cv);
}

void push(Client_req *req)
{
    pthread_mutex_lock(mutex);

    if (numeroRichieste <= 0)
        testa = req;

    coda->next = req;
    coda = req;
    numeroRichieste++;

    pthread_mutex_unlock(mutex);
}

Client_req *pop()
{
    pthread_mutex_lock(mutex);
    while (numeroRichieste <= 0)
        pthread_cond_wait(cv, mutex);

    Client_req *retReq;

    retReq = testa;
    testa = testa->next;

    retReq->next = NULL;

    pthread_mutex_unlock(mutex);

    return retReq;
}