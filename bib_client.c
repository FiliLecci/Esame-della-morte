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
size_t servers_len = 0;

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

void rimuoviSpaziConsecutivi(char *stringa)
{
    size_t lunghezzaStringa = strlen(stringa);
    int spazioConsecutivoFlag = 0;
    int ultimoCarattere = 0;

    for (int i = 0; i < lunghezzaStringa; i++)
    {
        if (stringa[i] != ' ' && i == 0)
        {
            spazioConsecutivoFlag = 0;
            stringa[ultimoCarattere] = stringa[ultimoCarattere];
            ultimoCarattere++;
            continue;
        }

        // serve per mantenere il primo spazio
        if (spazioConsecutivoFlag == 0)
            spazioConsecutivoFlag = 1;
    }
}

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

    while ((status = connect(fdClient, (struct sockaddr *)&indirizzoServer,
                             sizeof(struct sockaddr_in))) == -1 &&
           errno == ECONNREFUSED)
        sleep(1);

    if (status == -1)
        Perror("connect");

    return fdClient;
}

/**
 * @brief Fa il parsing di un array di stringhe e salva le informazioni in una Richiesta
 *
 * @param buffer Puntatore alla richiesta in cui inserire i dati
 * @param data Array di stringhe
 * @param len Lunghezza dell'array
 *
 * @returns Numero di proprietà lette
 */
size_t parseData(Richiesta_t *buffer, char **data, size_t len)
{
    if (buffer == NULL)
        return 0;

    // default
    buffer->tipo = 'Q';

    char *token;
    char **props; // proprietà già inserite
    size_t props_len = 0;

    size_t data_len;

    // itero gli argomenti
    for (size_t i = 1; i < len; i++)
    {
        // Loan (prestito)
        if (strcmp(data[i], "-p") == 0)
        {
            buffer->tipo = 'L';
            continue;
        }

        token = strtok(data[i], "=");
        rimuoviChar(token, '-');

        // se la proprietà è già stata inserita, la salto
        for (size_t j = 0; j < props_len; j++)
            if (strcmp(token, props[j]) == 0)
            {
                token = strtok(NULL, "=");
                goto SKIP_PROP;
            }

        // aggiungo la proprietà
        if (props_len == 0)
        {
            // prima proprietà inserita
            data_len = snprintf(NULL, 0, "%s", token) + 1;
            buffer->dati = (char *)malloc(data_len);
            snprintf(buffer->dati, data_len, "%s", token);
        }
        else
        {
            data_len = snprintf(NULL, 0, "%s;%s", buffer->dati, token) + 1;
            buffer->dati = (char *)realloc(buffer->dati, data_len);
            snprintf(buffer->dati, data_len, "%s;%s", buffer->dati, token);
        }

        // salvo la proprietà tra quelle già inserite
        props_len++;
        props = (char **)realloc(props, sizeof(char *) * props_len);
        props[props_len - 1] = (char *)malloc(sizeof(char) * strlen(token) + 1);
        strcpy(props[props_len - 1], token);

        // aggiungo il valore della proprietà
        token = strtok(NULL, "=");

        data_len = snprintf(NULL, 0, "%s:%s", buffer->dati, token) + 1;
        buffer->dati = (char *)realloc(buffer->dati, data_len);
        snprintf(buffer->dati, data_len, "%s:%s", buffer->dati, token);

    SKIP_PROP:;
    }

    buffer->lunghezza = strlen(buffer->dati);

    for (size_t i = 0; i < props_len; i++)
        free(props[i]);
    free(props);

    return props_len;
}

/**
 * @brief Fa il parsing del file di configurazione e salva le informazioni in un array di Server_t
 *
 * @param servers Array di Server
 * @param config_file Puntatore al file di configurazione
 * @param len Puntatore alla lunghezza dell'array (NULL se non è importante)
 */
void parseConfig(Server_t **servers, FILE *config_file, size_t *len)
{
    if (servers == NULL || config_file == NULL)
        return;

    size_t found = 0;
    char buffer[MAX_LINE_LENGTH];

    char *token;
    char *outer_save = NULL;
    char *inner_save = NULL;

    while (fgets(buffer, MAX_LINE_LENGTH, config_file) != NULL)
    {
        // trovata una nuova riga nel file di configurazione
        found++;
        servers = (Server_t **)realloc(servers, sizeof(Server_t *) * found);
        servers[found - 1] = (Server_t *)malloc(sizeof(Server_t));
        Server_t *server = servers[found - 1];

        rimuoviChar(buffer, '\n');

        // tokenize buffer
        for (token = strtok_r(buffer, ";", &outer_save); token; token = strtok_r(NULL, ";", &outer_save))
        {
            char *inner_token = strtok_r(token, ":", &inner_save);

            if (strcmp(inner_token, "nome") == 0)
            {
                inner_token = strtok_r(NULL, ":", &inner_save);
                server->nome = (char *)malloc(strlen(inner_token) + 1);
                strcpy(server->nome, inner_token);
            }
            else if (strcmp(inner_token, "indirizzo") == 0)
            {
                inner_token = strtok_r(NULL, ":", &inner_save);
                server->indirizzo = (char *)malloc(strlen(inner_token) + 1);
                strcpy(server->indirizzo, inner_token);
            }
            else if (strcmp(inner_token, "porta") == 0)
            {
                inner_token = strtok_r(NULL, ":", &inner_save);
                server->porta = atoi(inner_token);
            }
            else
            {
                Perror("File di configurazione non valido");
            }
        }
    }

    if (len != NULL)
        *len = found;
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

 //2 Se il parsing va a buon fine legge il file bib.conf per ottenere le informazioni riguardo a tutti i server disponibili

 //3 effettua la richiesta a tutti i server contenuti in bib.conf

 //4 I record ricevuti dal server sono stampati su stdout
 */

int main(int argc, char **argv)
{
    if (argc < 2)
        Perror("Necessario almeno un argomento per effettuare la richiesta");

    Richiesta_t richiesta;

    //- parse dei parametri nella struct della richiesta al server
    if (parseData(&richiesta, argv, argc) == 0)
        Perror("Si deve specificare almeno una proprietà da filtrare");

    FILE *config_file = Fopen("bib.conf", "r");

    servers = (Server_t **)malloc(sizeof(Server_t *));

    // parse del file di configurazione
    parseConfig(servers, config_file, &servers_len);

    // nessun server trovato
    if (servers_len <= 0)
    {
        printf("Nessun server disponibile... terminazione.\n");
        free(servers);
        free(richiesta.dati);
        fclose(config_file);
        return 0;
    }

    char *buffer;
    size_t buffer_len;

    for (int i = 0; i < servers_len; i++)
    {
        //- connessione al server
        printf("connessione al server %s...\n", servers[i]->nome);
        servers[i]->fd_server = connettiClient(servers[i]->indirizzo, servers[i]->porta);
        printf("connesso a %s:%d\n", servers[i]->indirizzo, servers[i]->porta);

        //-  creazione richiesta
        buffer_len = snprintf(NULL, 0, "%c;%zu;%s", richiesta.tipo, richiesta.lunghezza, richiesta.dati) + 1;
        buffer = (char *)malloc(buffer_len);
        snprintf(buffer, buffer_len, "%c;%zu;%s", richiesta.tipo, richiesta.lunghezza, richiesta.dati);

        //- invio richiesta
        send(servers[i]->fd_server, buffer, buffer_len, 0);

        printf("Richiesta: %s [%zu]\n", buffer, buffer_len);
    }

    //- aspetta per le risposte dei server e stampa
    char recBuff[1024];
    ssize_t status;

    for (int i = 0; i < servers_len; i++)
    {
        printf("Aspettando risposta dal server %s\n", servers[i]->nome);

        while (1)
        {
            status = recv(servers[i]->fd_server, recBuff, 1024, 0);

            if (status == -1)
                Perror("recv");

            if (status)
                printf("Risposta: %s\n", recBuff);
        }
    }

    //! chiusura de file e liberazione della memoria usata
    free(richiesta.dati);
    free(buffer);
    fclose(config_file);

    return 0;
}