#include <stdlib.h>
#include <semaphore.h>
#include <string.h>

typedef struct richiesta
{
    int clientSocket; // socket del client che ha fatto la richiesta
    char tipo;        // tipo di richiesta: 'Q' o 'L'
    int lunghezza;    // lunghezza della richiesta
    char *req;        // rihiesta nel formato proprietà:valore
    struct richiesta *next;
} Client_req;

pthread_mutex_t *mutex; // mutex
pthread_cond_t *cv;
pthread_condattr_t *empty; // variabile di condizione coda vuota
Client_req *testa;

void initCoda()
{
    sem_init(empty, 0, 0);
    testa = malloc(sizeof(Client_req));
    testa->next = malloc(sizeof(Client_req *));
}

void destroyCoda()
{
}

// inerimento in testa
void push(Client_req *req)
{
    testa->next = req;
    testa = req;

    sem_post(empty);
}

Client_req *pop()
{
    sem_wait(empty);

    Client_req *ret = testa;
    ret->next = NULL;

    testa = testa->next;
}