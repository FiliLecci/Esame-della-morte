#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_PROP_NAME_LEN 256
#define MAX_PROP_VALUE_LEN 256
#define MAX_LINE_LENGTH 1024

typedef struct
{
    unsigned char day;
    unsigned char month;
    unsigned short year;
} Date;

typedef union
{
    char *string[MAX_PROP_VALUE_LEN];
    Date date;
} Value;

typedef struct node
{
    char name[MAX_PROP_NAME_LEN];

    // valType = 0 -> char *
    // valType = 1 -> date
    unsigned char valType;
    Value value;

    struct node *next;
} Property;

typedef struct
{
    size_t size;
    Property *head;
    Property *tail;
} Book;

typedef struct
{
    char tipo; // Query 'Q' - Loan 'L'
    size_t lunghezza;
    char *dati;
} Richiesta_t;

// struct dei server a cui fare la richiesta
typedef struct
{
    char *nome;      // nome del server
    char *indirizzo; // indirizzo ip del server
    int porta;       // porta per accedere al server
    int fd_server;   // file descriptor della connect al server
} Server_t;

//* FUNZIONI CLASSICHE CON CONTROLLO ERRORI

Server_t **servers;
int numeroServer = 0;

void Perror(char *messaggio)
{
    perror(messaggio);
    exit(EXIT_FAILURE);
}

FILE *Fopen(char *filePath, char *mod)
{
    FILE *f;
    if ((f = fopen(filePath, mod)) == NULL)
        Perror("fopen");

    return f;
}

//* FUNZIONI DI UTILITY
// funzione per rimuovere un carattere c dalla char *a s
void rimuoviChar(char *s, char c)
{
    int i, j;
    size_t len = strlen(s);

    for (i = j = 0; i < len; i++)
    {
        if (s[i] != c)
            s[j++] = s[i];
    }
    s[j] = '\0';
}

int startClient(char *indirizzo, int porta)
{
    int status = 0, fdClient;
    struct sockaddr_in indirizzoServer;

    if ((fdClient = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        Perror("Creazione socket");

    memset(&indirizzoServer, 0, sizeof(struct sockaddr_in));

    indirizzoServer.sin_family = AF_INET;
    indirizzoServer.sin_port = htons(porta);

    errno = 0;

    if (inet_pton(AF_INET, indirizzo, &indirizzoServer.sin_addr) <= 0)
        Perror("Indirizzo non valido");

    while ((status = connect(fdClient, (struct sockaddr *)&indirizzoServer,
                             sizeof(struct sockaddr_in))) == -1 &&
           errno == ECONNREFUSED)
        sleep(1);

    return fdClient;
}

// restituisce un puntatore ad una struct di tipo Richiesta_t contenente i dati ottenuti dal parse
Richiesta_t *parseDati(int argc, char **argv)
{
    Richiesta_t *richiesta = (Richiesta_t *)malloc(sizeof(Richiesta_t));
    char *proprieta;

    richiesta->dati = malloc(sizeof(char));
    richiesta->tipo = 'Q';

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            richiesta->tipo = 'L';
            continue;
        }

        proprieta = strtok(argv[i], "=");

        // il primo valore è il --proprieta
        richiesta->dati = realloc(richiesta->dati, sizeof(richiesta->dati) + sizeof(proprieta) + 1);

        // rimuovo i -
        rimuoviChar(proprieta, '-');

        strcat(richiesta->dati, proprieta);
        strcat(richiesta->dati, ":");

        // il secondo valore è il valore della proprietà
        proprieta = strtok(NULL, "=");

        richiesta->dati = realloc(richiesta->dati, sizeof(richiesta->dati) + sizeof(proprieta));
        strcat(richiesta->dati, proprieta);

        strcat(richiesta->dati, ";");
    }

    richiesta->lunghezza = strlen(richiesta->dati);

    return richiesta;
}

// restituisce un array di struct di tipo Server_t contenti i dati letti dal file .conf
void parseConfFile(FILE *confFile)
{
    char riga[MAX_LINE_LENGTH]; // riga letta dal file
    char *token;
    char *nomeServer, *indirizzoServer; // nome e indirizzo del server
    char *tokPtr1, *tokPtr2;            // puntatori per strtok_r
    int portaServer;                    // porta del server

    // inizializzo l'array di server
    servers = malloc(sizeof(Server_t *));

    // legge riga dal file
    while (fgets(riga, MAX_LINE_LENGTH, confFile) != NULL)
    {
        rimuoviChar(riga, '\n');
        // parse nel nome del server
        token = strtok_r(riga, ";", &tokPtr1);
        strtok_r(token, ":", &tokPtr2);
        nomeServer = strtok_r(NULL, ":", &tokPtr2);

        // parse dell'inidirizzo del server
        token = strtok_r(NULL, ";", &tokPtr1);
        strtok_r(token, ":", &tokPtr2);
        indirizzoServer = strtok_r(NULL, ":", &tokPtr2);

        // parse della porta del server
        token = strtok_r(NULL, ";", &tokPtr1);
        strtok_r(token, ":", &tokPtr2);
        portaServer = atoi(strtok_r(NULL, ":", &tokPtr2));

        // inizializzo il nuovo server
        numeroServer++;

        servers = realloc(servers, sizeof(Server_t *) * numeroServer);
        servers[numeroServer - 1] = malloc(sizeof(Server_t));
        servers[numeroServer - 1]->indirizzo = malloc(sizeof(indirizzoServer));
        servers[numeroServer - 1]->nome = malloc(sizeof(nomeServer));

        strcpy(servers[numeroServer - 1]->nome, nomeServer);
        strcpy(servers[numeroServer - 1]->indirizzo, indirizzoServer);
        servers[numeroServer - 1]->porta = portaServer;
    }
}

/*
 ? INIZIALIZZAZIONE
 * Si avvia da riga di comando con il seguente formato:
 * ./bib_client <--proprieta="valore"> ... <-p>
 *
 * Per ogni chiamata può essere invocata una sola specifica per proprieta
 *
 * La proprietà -p server per richiedere il prestito al server di tutti i libri che soddisfano le proprietà specificate
 *
 ? RICHIESTA AL SERVER
 * I messaggi seguono la forma seguente:
 * [tipo], [lunghezza], [dati]
 *
 * "tipo" è il tipo della richiesta e può assumere i seguenti valori:
 * MSG_QUERY 'Q' -> richiede i record che hanno proprieta specifiche definite in "dati"
 * MSG_LOAN 'L' -> richiede il prestito di tutti i record che hanno proprieta specifiche definite in "dati"
 *
 * "lunghezza" è il numero di bit significativi in "dati"; vale 0 se non ci sono dati
 *
 * "dati" contiene le proprietà che i record devono rispettare, segue il seguente formato:
 * <proprieta: valore>;...
 *
 ? FUNZIONALITA
 // 1 esegue il parsing delle opzioni

 -2 Se il parsing va a buon fine legge il file bib.conf per ottenere le informazioni riguardo a tutti i server disponibili

 -3 effettua la richiesta a tutti i server contenuti in bib.conf

 -4 I record ricevuti dal server sono stampati su stdout
 */

int main(int argc, char **argv)
{
    if (argc < 2)
        Perror("Necessario almeno un argomento per efffettuare la richiesta");
    else if (argc == 2 && strcmp(argv[1], "-p") == 0)
        Perror("Si deve specificare almeno una proprietà da filtrare");

    //- parse dei parametri nella struct della richiesta al server
    Richiesta_t *richiesta = parseDati(argc, argv);

    printf("%c, %ld, %s\n", richiesta->tipo, richiesta->lunghezza, richiesta->dati);

    //- legge bib.conf e si connette ai server
    FILE *confFile = Fopen("bib.conf", "r");

    parseConfFile(confFile);

    for (int i = 0; i < numeroServer; i++)
        printf("letto: %s, %s, %d\n", servers[i]->nome, servers[i]->indirizzo, servers[i]->porta);

    //- invia richiesta al server

    //- aspetta per la risposta del server e stampa

    //! chiusura/liberazione della memoria usata
    free(richiesta->dati);
    fclose(confFile);

    return 0;
}