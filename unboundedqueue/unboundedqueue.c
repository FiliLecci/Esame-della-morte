#include <stdlib.h>
#include <semaphore.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#include "unboundedqueue.h"

pthread_mutex_t mutex; // mutex
pthread_cond_t cv;
Client_req *testa, *coda;
int numeroRichieste;
int connClosed;

void initCoda()
{
    connClosed = 0;
    numeroRichieste = 0;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cv, NULL);
    // inizializzo testa e coda
    testa = malloc(sizeof(Client_req));
    coda = malloc(sizeof(Client_req));
}

void destroyCoda()
{
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cv);
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
    pthread_cond_broadcast(&cv);
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

void closeConn()
{
    connClosed = 1;
    pthread_cond_broadcast(&cv);
}