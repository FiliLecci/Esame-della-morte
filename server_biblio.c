#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/mman.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <inttypes.h>

#include "./unboundedqueue/unboundedqueue.h"

#define _GNU_SOURCE
#define __USE_GNU

#define STRING_TYPE 0
#define DATE_TYPE 1

#define MAX_PROP_NAME_LEN 256
#define MAX_PROP_VALUE_LEN 256
#define MAX_LINE_LENGTH 1024
#define MAX_SERVER_CONF_LENGTH 256 // nome + 39 incluso \n quindi nome = 217
#define PORT 15792

typedef struct
{
    unsigned char day;
    unsigned char month;
    unsigned short year;
} Date;

typedef union
{
    char string[MAX_PROP_VALUE_LEN];
    Date date;
} Value;

typedef struct node
{
    char name[MAX_PROP_NAME_LEN];

    // valType = 0 -> string
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

pthread_mutex_t mutexConnessioni; // mutex per gestire le connessioni attive

Book **all_books;        // array di libri
size_t all_books_len;    // numero di libri
Property **all_props;    // array di proprietà
size_t all_props_len;    // numero di proprietà
Client_req **client_req; // array di tutte le richieste
int *tid;                // array di tid
int connessioniAttive;   // numero di connessioni attive
int numeroWorker;        // numero di worker
int stopSignal = 0;      // segnale di stop per segnali

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

void noSpace(char *stringa)
{
    size_t lunghezzaStringa = strlen(stringa);
    int spazioConsecutivoFlag = 0;
    int ultimoCarattere = 0;

    for (int i = 0; i < lunghezzaStringa; i++)
    {
        if (i == 0 && isspace(stringa[i]) != 0)
        {
            spazioConsecutivoFlag = 1;
            continue;
        }

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

// Inserimento in testa
void addProperty(Book *book, Property *property)
{
    if (book->size == 0)
        book->tail = property;
    else
        property->next = book->head;

    book->head = property;

    book->size++;
}

// scrive il libro 'book' come stringa in 'res'
void printProperties(Book *book, char *res)
{
    char *tempRes;
    Property *prop = book->head;
    size_t resultLen = 0;

    tempRes = malloc(sizeof(char));

    while (prop != NULL)
    {
        tempRes = realloc(tempRes, resultLen + strlen(prop->name) + 1);
        strcat(tempRes, prop->name);
        strcat(tempRes, "\n");

        resultLen = strlen(tempRes);

        prop = prop->next;
    }

    strcpy(res, tempRes);

    free(tempRes);
}

void parseDati(FILE *file)
{
    char buffer[MAX_LINE_LENGTH]; // buffer per riga letta

    char *property_token;         // token strtok
    char *lastb, *lastp;          // var per strtok_r
    char *prop_name, *prop_value; // nome e valore proprietà

    while (fgets(buffer, MAX_LINE_LENGTH, file) != NULL)
    {
        //* Ho trovato un nuovo libro
        all_books_len++;
        all_books = (Book **)realloc(all_books, sizeof(Book *) * all_books_len);
        all_books[all_books_len - 1] = (Book *)malloc(sizeof(Book));

        //- Inizializzo il nuovo libro
        Book *book_ptr = all_books[all_books_len - 1];

        book_ptr->size = 0;
        book_ptr->head = book_ptr->tail = NULL;

        // Separo le proprietà
        for (property_token = strtok_r(buffer, ";", &lastb); property_token; property_token = strtok_r(NULL, ";", &lastb))
        {
            //* Ho trovato una nuova proprietà
            all_props_len++;
            all_props = (Property **)realloc(all_props, sizeof(Property *) * all_props_len);
            all_props[all_props_len - 1] = (Property *)malloc(sizeof(Property));

            Property *prop_ptr = all_props[all_props_len - 1];

            //* Inizializzo la nuova proprietà
            memset(prop_ptr->name, 0, MAX_PROP_NAME_LEN);
            prop_ptr->valType = 0;
            memset(&prop_ptr->value, 0, sizeof(Value));
            prop_ptr->next = NULL;

            //* Inserisco il nome della proprietà
            prop_name = strtok_r(property_token, ":", &lastp);
            noSpace(prop_name);

            if (strlen(prop_name) == 0)
                continue;

            strncpy(prop_ptr->name, prop_name, MAX_PROP_NAME_LEN);

            //* Distinguo tra prestito e non
            if (strcmp(prop_ptr->name, "prestito") == 0)
            {
                prop_ptr->valType = 1;

                prop_value = strtok_r(NULL, ":", &lastp);
                prop_value = strtok_r(prop_value, "-", &lastp);
                prop_ptr->value.date.day = (unsigned char)atoi(prop_value);

                prop_value = strtok_r(NULL, "-", &lastp);
                prop_ptr->value.date.month = (unsigned char)atoi(prop_value);

                prop_value = strtok_r(NULL, "-", &lastp);
                prop_ptr->value.date.year = (unsigned short)atoi(prop_value);
            }
            else
            {
                prop_ptr->valType = 0;

                prop_value = strtok_r(NULL, ":", &lastp);
                noSpace(prop_value);
                strncpy(prop_ptr->value.string, prop_value, MAX_PROP_VALUE_LEN);
            }

            // Aggiungo la proprietà al libro
            addProperty(book_ptr, prop_ptr);
        }
    }
}

// gestione segnali
static void gestore(int signum)
{
    // segnale di stop per i worker thread e main thread
    stopSignal = 1;
    // segnale per fermare la coda
    closeConn();
}

int checkProp(Book *libro, char **req, int numeroReq)
{
    Property *prop = libro->head;
    int reqIndex = 0;
    char *token;

    // scorre le prop
    while (prop != NULL)
    {
        if (reqIndex >= numeroReq)
            return 1;

        if (strcmp(prop->name, req[reqIndex]) != 0)
        {
            prop = prop->next;
            continue;
        }

        if (prop->valType == STRING_TYPE)
        {
            if (strcmp(prop->value.string, req[reqIndex + 1]) == 0)
            {
                prop = libro->head;
                reqIndex++;
                continue;
            }
            prop = prop->next;
            continue;
        }

        token = strtok(req[reqIndex + 1], "-");
        if (memcmp(token, &prop->value.date.day, sizeof(char)) != 0)
            break;

        token = strtok(NULL, "-");
        if (memcmp(token, &prop->value.date.month, sizeof(char)) != 0)
            break;

        token = strtok(NULL, "-");
        if (atoi(token) == prop->value.date.year)
        {
            prop = libro->head;
            reqIndex++;
            break;
        }
    }
    return 0;
}

// cerca un libro che abbia
void cercaLibri(char *req, char *res)
{
    char **props;
    char *coppia, *token;
    char *tokPtr1, *tokPtr2;
    int numeroProp = 0, numeroLibriAccettati = 0;

    props = malloc(sizeof(char *) * 2);
    coppia = strtok_r(req, ";", &tokPtr1);

    do
    {
        // la richiesta arriva come <proprieta:valore>;
        token = strtok_r(coppia, ":", &tokPtr2);
        props[numeroProp] = malloc(sizeof(char) * strlen(token) + 1);
        strcpy(props[numeroProp], token);

        token = strtok_r(NULL, ":", &tokPtr2);
        props[numeroProp + 1] = malloc(sizeof(char) * strlen(token) + 1);
        strcpy(props[numeroProp + 1], token);

        numeroProp++;
        props = realloc(props, sizeof(char *) * (numeroProp * 2));
    } while ((coppia = strtok_r(NULL, ";", &tokPtr1)) != NULL);

    for (int i = 0; i < all_books_len; i++)
    {
        if (checkProp(all_books[i], props, numeroProp) != 0)
        {
            numeroLibriAccettati++;
            printProperties(all_books[i], res);
            printf("Libro~~~%s~~~\n", res);
        }
    }

    free(props);
}

// thread worker prende in carico la prima richiesta nella coda
void *workerThread(void *args)
{
    while (!stopSignal)
    {
        //- prende la prima richiesta dalla coda
        Client_req *req = pop();

        // se req è NULL non ci sono elementi nella coda e la connessione è chiusa
        if (req == NULL)
        {
            sleep(1);
            continue;
        }

        printf("presa richiesta %c, %d, %s\n", req->tipo, req->lunghezza, req->req);
        fflush(stdout);

        //- cerca i libri che corrispondono alla query
        char *result = malloc(sizeof(char *));
        cercaLibri(req->req, result);

        //- invia risposta al client
        ssize_t bitSignificativi = strlen(result);
        char *resultConDimensione = (char *)malloc(bitSignificativi + 5);

        sprintf(resultConDimensione, "%0*ld;%s", 4, bitSignificativi, result);

        printf("Inviata risposta %s\n", resultConDimensione);
        send(req->clientSocket, resultConDimensione, strlen(resultConDimensione), 0);

        //- chiude la connessione con il client
        pthread_mutex_lock(&mutexConnessioni);
        connessioniAttive--;
        pthread_mutex_unlock(&mutexConnessioni);

        shutdown(req->clientSocket, SHUT_RDWR);

        free(result);
        free(resultConDimensione);
    }
    return NULL;
}

int inserisciConfigurazioneServer(FILE *confFile, char *nome)
{
    char *buffer = malloc(MAX_SERVER_CONF_LENGTH);
    char *configurazioneServerCorrente = malloc(MAX_SERVER_CONF_LENGTH);

    sprintf(configurazioneServerCorrente, "nome:%s;indirizzo:%s;porta:%d;", nome, "127.0.0.1", PORT);

    while (fgets(buffer, MAX_SERVER_CONF_LENGTH, confFile))
    {
        if (strcmp(buffer, configurazioneServerCorrente) == 0 || strcmp(buffer, strcat(configurazioneServerCorrente, "\n")) == 0)
        {
            free(buffer);
            free(configurazioneServerCorrente);
            return 1;
        }
    }

    fwrite(strcat(configurazioneServerCorrente, "\n"), 1, strlen(configurazioneServerCorrente) + 1, confFile);

    return 0;
}

/*
 ? INIZIALIZZAZIONE SERVER
 * Attivato da riga di comando sulla shell come segue:
 * $ server_biblio [nome_bib] [file_record] [W]
 *
 * "file_record" si riferisce al file contenente le informazioni relative ai libri contenuti nella biblioteca name_bib. Sono i file in bibData/.
 *
 * "nome_bib" si riferisce al nome della biblioteca (a scelta)
 *
 * "W" è il  numero di thread worker
 *
 * All'avio del server sono caricate tutte le info relative ai record dal file; In ogni biblioteca i record sono unici
 *
 ? FILE DEI RECORD
 * I file dei record seguono il seguente formato:
 * <proprieta: valore>; ...\n
 *
 * Le proprieta possono essere ripetutte in caso possano avere più valori
 *
 * La proprieta "prestito" è presente solo nel caso in cui il volume sia attualmente in prestito ed indica la data di scadenza del prestito
 *
 ? FILE LOG
 * Ogni richiesta processata si registra in un file di log nome_bib.log che viene svuotato ad ogni nuova accensione del server.
 *
 * Per ogni richiesta di tipo MSG_QUERY si rispetta il formato seguente:
 * <record 1>
 * <record 2>
 * ...
 * QUERY [#record]
 *
 * Per ogni richiesta di tipo MSG_LOAN si rispetta il formato seguente:
 * <record 1>
 * <record 2>
 * ...
 * LOAN [#record con prestito approvato]
 *
 ? RISPOSTA SERVER
 * Il server risponde sullo stesso socket su cui riceve le richieste
 *
 * Sia richiesta che risposta hanno il formato:
 * [tipo],[lunghezza],[dati]
 *
 * Il campo "tipo" è un char che contiene il tipo di messaggio inviato ed assume uno dei seguenti valori:
 * MSG_RECORD 'R' -> "dati" contiene i record di risposta
 * MSG_NO 'N' -> non ci sono record che verificano la query
 * MSG_ERROR 'E' -> errore nel processare la richiesta; "dati" spiega l'errore verificatosi
 *
 * Il campo "lunghezza" indica il numero di bit significativi all'interno di "dati"; vale 0 se non ci sono dati
 *
 * "dati" è una stringa che contiene il messaggio di risposta effettivo; per ogni send si inviano solo i byte significativi e non un buffer di lunghezza fissa
 *
 ? TEMINAZIONE
 * Il server termina al ricevimento di un segnale tra SIGINT o SIGTERM, se ne riceve uno si attende la terminazione dei thread worker, si termina la scrittura dei file di log e file_record e si chiude il socket
 *
 ? FUNZIONALITA
 //0 Parsing dei libri dal file passato come argomento in una struttura condivisa

 //1 Se il file passato come argomento è valido si creano un file di log e un nuovo socket

 //2 Una volta avviato il socket il server "comunica" la sua operatività scrivendo i suoi dati nel flie ./bib.conf con il formato seguente:
 *nome:[nome];indirizzo:[hostname];porta:[porta];

 //3 Il server si mette in ascolto per aspettare la connessione dei client (NON BLOCCANTE)

 //4 Le richieste dei client vengono messe in una coda condivisa tra gli worker

 //5 I worker una volta presa una richiesta in carico lavorano su una struttura condivisa contenente tutti le informazioni relative ai libri

 -6 Se viene richiesto un prestito (dalla durata di 30 giorni) si aggiorna LOCALMENTE nella struttura condivisa il campo "prestito" del libro

 -7 Il server registra un file di log ./nome_bib.log in cui viene registrato il numero di record inviati al client per ogni richiesta (vedi FILE LOG ^^^)

 -8 Alla chiusura viene sovrascritto il "file_record" con i nuovi dati. Se la data del prestito è scaduta, questo campo non viene stampato
 */

int main(int argc, char **argv)
{
    if (argc < 4)
        Perror("Parametri insufficienti");

    FILE *f = Fopen(argv[2], "r");

    //! gestione segnali
    signal(SIGINT, gestore);
    signal(SIGTERM, gestore);

    all_books = malloc(sizeof(Book *));
    all_props = malloc(sizeof(Property *));

    parseDati(f);

    //- creo il file di log
    FILE *logFile;
    char logName[strlen(argv[1]) + 5];
    //* il log si chiama nomebib.log
    snprintf(logName, strlen(argv[1]) + 5, "%s.log", argv[1]);
    logFile = Fopen(logName, "w+");

    //- inizializzazione socket
    int server_fd;
    struct sockaddr_in indirizzoServer;
    int opt = 1;
    numeroWorker = atoi(argv[3]);

    pthread_t tid[atoi(argv[3])];

    //* il socket è impostato su non bloccante per permettere il ricevimento di messaggi e la connessione di altri client da parte del solo main thread
    if ((server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
    {
        Perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET,
                   SO_REUSEADDR, &opt,
                   sizeof(opt)) == -1)
    {
        Perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&indirizzoServer, 0, sizeof(struct sockaddr_in));

    indirizzoServer.sin_family = AF_INET;
    indirizzoServer.sin_port = htons(PORT);
    indirizzoServer.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&indirizzoServer, sizeof(struct sockaddr_in)) < 0)
    {
        Perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0)
    {
        Perror("listen");
        exit(EXIT_FAILURE);
    }

    //- scrive nel file bib.conf i propri dati
    FILE *confFile;
    // apre in modalità append per non sovrascrivere dati di altri server e creare il file se non esiste
    confFile = Fopen("bib.conf", "a+");

    inserisciConfigurazioneServer(confFile, argv[1]);
    fclose(confFile);

    //- avvio thread per elaborazione richieste
    pthread_mutex_init(&mutexConnessioni, NULL);

    for (int i = 0; i < numeroWorker; i++)
    {
        if (pthread_create(&tid[i], NULL, workerThread, NULL) != 0)
            Perror("thread create");
    }

    client_req = (Client_req **)malloc(sizeof(Client_req *)); // array di richieste dei client
    int tempSock;                                             // socket temporaneo
    connessioniAttive = 0;                                    // connessioni attive
    char *buffer;                                             // buffer di ricezione

    buffer = malloc(7);

    //- inizializzo la coda
    initCoda();

    //* accettazione client (ciclo infinito fino a segnale SIGINT)
    while (!stopSignal)
    {
        sleep(1);
        //- controlla se ci sono client a cui accettare la richiesta di connessione
        printf("Accetto...\a\n");
        tempSock = accept(server_fd, NULL, NULL);

        //- se si è connesso un nuovo client aumento il contatore
        pthread_mutex_lock(&mutexConnessioni);
        if (tempSock != -1)
        {
            connessioniAttive++;
            client_req = (Client_req **)realloc(client_req, sizeof(Client_req *) * connessioniAttive);
            client_req[connessioniAttive - 1] = malloc(sizeof(Client_req));
            client_req[connessioniAttive - 1]->next = malloc(sizeof(Client_req *));

            client_req[connessioniAttive - 1]
                ->clientSocket = tempSock;
        }

        if (connessioniAttive <= 0)
        {
            pthread_mutex_unlock(&mutexConnessioni);
            continue;
        }

        //- controlla se sono arrivate richieste
        size_t dimReq, dim;
        char *token;

        for (int i = 0; i < connessioniAttive; i++)
        {

            if ((dim = recv(client_req[i]->clientSocket, buffer, 7, SOCK_NONBLOCK)) <= 0)
                continue;

            //  ottengo tipo richiesta
            token = strtok(buffer, ",");
            client_req[i]->tipo = *token;

            // ottengo lunghezza dati significativi
            token = strtok(NULL, ",");
            dimReq = atoi(token);
            client_req[i]->lunghezza = dimReq;

            // devo allocare la dimenzione dei bit significativi
            buffer = realloc(buffer, sizeof(char) * dimReq + 1);
            memset(buffer, '\0', sizeof(char) * dimReq + 1);

            // ottengo stringa dei dati
            dim = recv(client_req[i]->clientSocket, buffer, dimReq + 1, SOCK_NONBLOCK);
            client_req[i]->req = malloc(sizeof(char) * dim + 1);
            strcpy(client_req[i]->req, buffer);

            // inserisco la richiesta nella coda condivisa
            push(client_req[i]);
            memset(buffer, '\0', dimReq);
        }
        pthread_mutex_unlock(&mutexConnessioni);
    }

    //* esco dopo che l'handler dei segnali ha impostato la variabile a 1

    printf("Terminazione worker...\n");
    //- aspetta terminazione dei worker
    for (int i = 0; i < numeroWorker; i++)
        pthread_join(tid[i], NULL);

    //- chiude socket
    for (int i = 0; i < connessioniAttive; i++)
    {
        close(client_req[i]->clientSocket);
        free(client_req[i]);
    }

    //- termina la scrittura del log

    free(client_req);
    close(server_fd);

    free(buffer);

    for (int i = 0; i < all_books_len; i++)
        free(all_books[i]);
    free(all_books);

    for (int i = 0; i < all_props_len; i++)
        free(all_props[i]);
    free(all_props);

    fclose(f);
    fclose(logFile);

    destroyCoda();

    return 0;
}