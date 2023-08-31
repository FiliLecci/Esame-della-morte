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
#include <ctype.h>

#define MAX_PROP_NAME_LEN 256
#define MAX_PROP_VALUE_LEN 256
#define MAX_LINE_LENGTH 1024

#define SERVER_CONNESSO 1
#define SERVER_NON_CONNESSO 0

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

typedef struct
{
    char *nome;      // nome del server
    char *indirizzo; // indirizzo ip del server
    int porta;       // porta per accedere al server
    int fd_server;   // file descriptor della connect al server
    int connesso;    // 0 connessione non riuscita, 1 altrimenti
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

/*
 * Rimuove un carattere da una stringa
 *
 * @param s
 * la stringa dalla quale rimuovere il carattere
 * @param c
 * il carattere da rimuovere
 */
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

/*
 * Elimina eventuali spazi consecutivi
 *
 * @param stringa
 * la stringa alla quale si vogliono eliminare gli spazi
 */
void rimuoviSpaziConsecutivi(char *stringa)
{
    if (stringa == NULL)
        return;

    if (strlen(stringa) <= 0)
        return;

    size_t lunghezzaStringa = strlen(stringa);
    int spazioConsecutivoFlag = 1;
    int ultimoCarattere = 0;

    for (int i = 0; i < lunghezzaStringa; i++)
    {
        if (isspace(stringa[i]) == 0)
        {
            stringa[ultimoCarattere++] = stringa[i];
            spazioConsecutivoFlag = 0;
            continue;
        }

        if (spazioConsecutivoFlag == 0)
        {
            stringa[ultimoCarattere++] = stringa[i];
            spazioConsecutivoFlag = 1;
        }
    }

    stringa[ultimoCarattere] = '\0';
}

/*
 * Connette il client ad un server
 *
 * @param indirizzo
 * l'indirizzo del server a cui connettersi
 * @param porta
 * la porta del server a cui connettersi
 */
int connettiClient(char *indirizzo, int porta)
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

    status = connect(fdClient, (struct sockaddr *)&indirizzoServer, sizeof(struct sockaddr_in));

    return (status == -1 ? -1 : fdClient);
}

/*
 * Effettua il parsing delle opzioni specificate all'esecuzione del programma
 *
 * @param argc
 * numero di argomenti passati
 * @param argv
 * array di stringhe contententi le coppie nome:valore dei parametri
 * @param richiesta
 * la struct che viene creata contenente i dati della richiesta
 */
void parseDati(int argc, char **argv, Richiesta_t *richiesta)
{
    char *token;
    char **propArr;  // array di tutte le proprietà
    int propNum = 0; // numero di proprietà

    propArr = malloc(sizeof(char *));
    richiesta->dati = malloc(sizeof(char));
    strcpy(richiesta->dati, "\0");
    richiesta->tipo = 'Q';

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            richiesta->tipo = 'L';
            continue;
        }

        token = strtok(argv[i], "=");

        // rimuovo i -
        rimuoviChar(token, '-');

        // controllo se la proprietà non è già stata inserita
        for (int i = 0; i < propNum; i++)
        {
            if (strcmp(propArr[i], token) == 0)
            {
                printf("saltata proprietà %s\n", token);
                return;
            }
        }

        // il primo valore è il --proprieta
        richiesta->dati = realloc(richiesta->dati, strlen(richiesta->dati) + strlen(token) + 2);
        propNum++;
        propArr = realloc(propArr, sizeof(char *) * propNum);
        propArr[propNum - 1] = malloc(sizeof(token));

        strcpy(propArr[propNum - 1], token);
        strcat(richiesta->dati, token);
        strcat(richiesta->dati, ":");

        // il secondo valore è il valore della proprietà
        token = strtok(NULL, "=");

        richiesta->dati = realloc(richiesta->dati, strlen(richiesta->dati) + strlen(token) + 2);
        strcat(richiesta->dati, token);
        strcat(richiesta->dati, ";");
        rimuoviSpaziConsecutivi(richiesta->dati);
    }

    richiesta->lunghezza = strlen(richiesta->dati);

    for (int i = 0; i < propNum; i++)
        free(propArr[i]);
    free(propArr);
}

/*
 * Effettua il parsing dei server e li aggiunge all'array di server.
 *
 * @param confFile
 * il file di configurazione dal quale vengono prese le informazioni dei server
 */
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
        servers[numeroServer - 1]->indirizzo = malloc(strlen(indirizzoServer) + 1);
        servers[numeroServer - 1]->nome = malloc(strlen(nomeServer) + 1);

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
 * <proprieta:valore>;...
 *
 ? FUNZIONALITA
 // 1 esegue il parsing delle opzioni

 //2 Se il parsing va a buon fine legge il file bib.conf per ottenere le informazioni riguardo a tutti i server disponibili

 //3 effettua la richiesta a tutti i server contenuti in bib.conf

 //4 I record ricevuti dal server sono stampati su stdout
 */

int main(int argc, char **argv)
{
    if (argc < 2)
        Perror("Necessario almeno un argomento per efffettuare la richiesta");
    else if (argc == 2 && strcmp(argv[1], "-p") == 0)
        Perror("Si deve specificare almeno una proprietà da filtrare");

    //- parse dei parametri nella struct della richiesta al server
    Richiesta_t *richiesta;

    richiesta = malloc(sizeof(Richiesta_t));
    parseDati(argc, argv, richiesta);

    printf("%c, %ld, %s\n", richiesta->tipo, richiesta->lunghezza, richiesta->dati);

    //- legge bib.conf
    FILE *confFile = Fopen("bib.conf", "r");

    parseConfFile(confFile);

    if (numeroServer <= 0)
    {
        printf("Nessun server disponibile... terminazione.\n");
        free(servers);
        free(richiesta->dati);
        free(richiesta);
        fclose(confFile);
        return 0;
    }

    //- connessione ai server e memorizzazione dei file descriptor
    char *tempBuffer = NULL;
    int statusConnessione;
    ssize_t bufferDim;

    for (int i = 0; i < numeroServer; i++)
    {
        printf("connessione al server %s...\n", servers[i]->nome);
        statusConnessione = connettiClient(servers[i]->indirizzo, servers[i]->porta);
        if (statusConnessione == -1)
        {
            printf("connessione non riuscita.\n");
            servers[i]->connesso = SERVER_NON_CONNESSO;
            continue;
        }

        servers[i]->fd_server = statusConnessione;
        servers[i]->connesso = SERVER_CONNESSO;
        printf("connesso, invio richiesta...\n");

        //- invia richiesta al server
        tempBuffer = malloc(sizeof(char) + sizeof(unsigned long) + strlen(richiesta->dati) + 3);
        bufferDim = sprintf(tempBuffer, "%c,%0*ld,%s", richiesta->tipo, 4, richiesta->lunghezza, richiesta->dati);

        send(servers[i]->fd_server, tempBuffer, bufferDim, 0);
    }

    //- aspetta per le risposte dei server e stampa
    char *recBuff = (char *)malloc(5);
    char *token;
    ssize_t dimensioneRisposta;

    for (int i = 0; i < numeroServer; i++)
    {
        if (servers[i]->connesso == SERVER_NON_CONNESSO)
            continue;

        printf("Aspettando risposta dal server %s...\n", servers[i]->nome);
        // i primi 5 caratteri sono il numero dei bit significativi
        recv(servers[i]->fd_server, recBuff, 5, 0);

        token = strtok(recBuff, ";");
        dimensioneRisposta = atoi(token) + 1;

        recBuff = realloc(recBuff, dimensioneRisposta);
        memset(recBuff, '\0', dimensioneRisposta);

        recv(servers[i]->fd_server, recBuff, dimensioneRisposta, 0);

        printf("%s\n", recBuff);

        shutdown(servers[i]->fd_server, SHUT_RDWR);
    }

    //! chiusura/liberazione della memoria usata
    for (int i = 0; i < numeroServer; i++)
    {
        free(servers[i]->nome);
        free(servers[i]->indirizzo);
        free(servers[i]);
    }
    free(servers);

    free(recBuff);
    free(richiesta->dati);
    free(richiesta);
    free(tempBuffer);
    fclose(confFile);

    return 0;
}