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
#define _GNU_SOURCE
#define __USE_GNU

#define MAX_PROP_NAME_LEN 256
#define MAX_PROP_VALUE_LEN 256
#define MAX_LINE_LENGTH 1024
#define PORT 15792

typedef struct
{
    int clientSocket; // socket del client che ha fatto la richiesta
    char tipo;        // tipo di richiesta: 'Q' o 'L'
    int lunghezza;    // lunghezza della richiesta
    char *req;        // rihiesta nel formato proprietà:valore
} Client_req;

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

Book **all_books;
size_t all_books_len;
Property **all_props;
size_t all_props_len;
int *tid, *clientSocket;
int connessioniAttive;
int numeroWorker;
int stopSignal = 0;

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

char *noSpace(char *str)
{
    // Ignora gli spazi all'inizio della stringa
    while (isspace((unsigned char)*str))
    {
        str++;
    }

    // Ignora gli spazi alla fine della stringa
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
    {
        end--;
    }
    end[1] = '\0'; // Imposta il terminatore nullo dopo l'ultimo carattere valido

    // Crea una nuova stringa e copia la parte senza spazi
    char *result = malloc(strlen(str) + 1);
    strcpy(result, str);

    return result;
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

void printProperties(Book *book)
{
    if (book->size == 0)
        return;

    Property *follower = book->head;

    while (follower != NULL)
    {
        printf("%s : ", follower->name);

        if (follower->valType)
            printf("%d-%d-%d\n", follower->value.date.day, follower->value.date.month, follower->value.date.year);
        else
            printf("%s\n", follower->value.string);

        follower = follower->next;
    }
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

        // Tokenizzo le proprietà
        for (property_token = strtok_r(buffer, ";", &lastb); property_token; property_token = strtok_r(NULL, ";", &lastb))
        {
            //* Ho trovato una nuova proprietà
            all_props_len++;
            all_props = (Property **)realloc(all_props, sizeof(Property *) * all_props_len);
            all_props[all_props_len - 1] = (Property *)malloc(sizeof(Property));

            Property *prop_ptr = all_props[all_props_len - 1];

            // Inizializzo la nuova proprietà
            memset(prop_ptr->name, 0, MAX_PROP_NAME_LEN);
            prop_ptr->valType = 0;
            memset(&prop_ptr->value, 0, sizeof(Value));
            prop_ptr->next = NULL;

            // Inserisco il nome della proprietà
            prop_name = strtok_r(property_token, ":", &lastp);
            prop_name = noSpace(prop_name);

            if (strlen(prop_name) == 0)
                continue;

            strncpy(prop_ptr->name, prop_name, MAX_PROP_NAME_LEN);

            // Distinguo tra prestito e non
            if (strcmp(prop_ptr->name, "prestito") == 0)
            {
                prop_ptr->valType = 1;

                prop_value = strtok_r(NULL, ":", &lastp);
                prop_value = strtok_r(prop_value, "-", &lastp);
                prop_ptr->value.date.day = (unsigned char)atoi(prop_value);
                prop_value = strtok_r(prop_value, "-", &lastp);
                prop_ptr->value.date.month = (unsigned char)atoi(prop_value);
                prop_value = strtok_r(prop_value, "-", &lastp);
                prop_ptr->value.date.year = (unsigned short)atoi(prop_value);
            }
            else
            {
                prop_ptr->valType = 0;

                prop_value = strtok_r(NULL, ":", &lastp);
                prop_value = noSpace(prop_value);
                strncpy(prop_ptr->value.string, prop_value, MAX_PROP_VALUE_LEN);
            }

            // Aggiungo la proprietà al libro
            addProperty(book_ptr, prop_ptr);
        }
    }
    free(prop_name);
}

// TODO gestione segnali
static void gestore(int signum)
{
    stopSignal = 1;
}

// TODO thread worker prende in carico la prima richiesta nella coda
void *workerThread(void *args)
{
    //- invia risposta al client

    //- chiude la connessione con il client e diminuisce la variabile

    return NULL;
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
 * <proprieta: valore, <valore>>; ...\n
 *
 * Le proprieta possono essere ripetutte in caso possano avere più valori
 *
 * La proprieta "prestito" è presente solo nel caso in cui il volume sia attualmente in prestito ed indica la data di scadenza del prestito
 *
 ? FILE LOG
 * Ogni richiesta processata si registra in un file di log nome_bib.log che viene svuotato ad ogni nuova accensione del server.
 *
 * Per ogni richiesta di tipo MSG_QUERY si rispetta il formato seguente:
 * <record cha soddisfano la query>
 * QUERY [#record]
 *
 * Per ogni richiesta di tipo MSG_LOAN si rispetta il formato seguente:
 * <record cha soddisfano la query>
 * LOAN [#record con prestito approvato]
 *
 ? RISPOSTA SERVER
 * Il server risponde sullo stesso socket su cui riceve le richieste
 *
 * Sia richiesta che risposta hanno il formato:
 * [tipo], [lunghezza], [dati]
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

 -3 Il server si mette in ascolto per aspettare la connessione dei client (NON BLOCCANTE)

 -4 Le richieste dei client vengono messe in una coda condivisa tra gli worker

 -5 I worker una volta presa una richiesta in carico lavorano su una struttura condivisa contenente tutti le informazioni relative ai libri

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

    for (int i = 0; i < all_books_len; i++)
    {
        printf("Libro [%d]\n", i);
        printProperties(all_books[i]);
    }

    printf("Fatto il parsing dei libri, creo il file di log...\n");

    //- creo il file di log
    FILE *logFile;
    char logName[strlen(argv[1]) + 5];
    //* il log si chiama nomebib.log
    snprintf(logName, strlen(argv[1]) + 5, "%s.log", argv[1]);
    logFile = Fopen(logName, "w+");

    printf("Creato file di log, avvio socket...\n");

    //- inizializzazione socket
    int server_fd;
    struct sockaddr_in indirizzoServer;
    int opt = 1;
    numeroWorker = atoi(argv[1]);

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

    printf("Avviato socket, scrivo bib.conf...\n");

    //- scrive nel file bib.conf i propri dati
    FILE *confFile;
    // apre in modalità append per non sovrascrivere dati di altri server e creare il file se non esiste
    confFile = Fopen("bib.conf", "a");

    fprintf(confFile, "nome:%s;indirizzo:%s;porta:%d;\n", argv[1], "127.0.0.1", PORT);

    //- avvio thread per elaborazione richieste
    for (int i = 0; i < numeroWorker; i++)
    {
        if (pthread_create(&tid[i], NULL, workerThread, NULL) != 0)
            Perror("thread create");
    }

    clientSocket = (int *)malloc(sizeof(int));
    int tempSock;
    connessioniAttive = 0;
    char buffer[1024];
    // TODO accettazione client (ciclo infinito fino a segnale SIGINT)
    while (!stopSignal)
    {
        //- controlla se ci sono client a cui accettare la richiesta di connessione
        printf("Accetto...\n");
        clientSocket[connessioniAttive] = accept(server_fd, NULL, NULL);

        //- se si è connesso un nuovo client aumento il contatore
        if (tempSock != -1)
            connessioniAttive++;

        printf("Ascolto richieste...\n");
        //- controlla se sono arrivate richieste
        recv(clientSocket, buffer, 1024, 0);

        sleep(1);
    }
    //* esco dopo che l'handler dei segnali ha impostato la variabile a 1

    printf("Terminazione worker...\n");
    //- aspetta terminazione dei worker
    for (int i = 0; i < numeroWorker; i++)
        pthread_join(tid[i], NULL);

    //- chiude socket
    for (int i = 0; i < connessioniAttive; i++)
        close(clientSocket[i]);

    //- termina la scrittura del log

    for (int i = 0; i < numeroWorker; i++)
        free(tid);
    free(clientSocket);
    close(server_fd);

    free(all_books);
    free(all_props);
    fclose(f);
    fclose(logFile);
    fclose(confFile);

    return 0;
}