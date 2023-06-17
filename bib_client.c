#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef enum TipoRichiesta
{
    MSG_QUERY,
    MSG_LOAN
} Richiesta_tipo_t;

typedef char *String;

// tipo per tenere una data; i campi sono a 0 se non devono essere considerati
typedef struct
{
    int giorno;
    int mese;
    int anno;
} Data_t;

// union per i vari tipi di valori
typedef union
{
    Data_t data;
    String stringa;
} Proprieta_tipi_t;

// struct del nodo per la lista di proprietà di un libro
typedef struct Nodo
{
    String nomeProprieta;    // nome proprietà
    Proprieta_tipi_t valori; // valore per la proprietà
    struct Nodo *prev;       // nodo precendente
    struct Nodo *next;       // nodo successivo
} Proprieta_t;

// ogni libro è rappresentato come una lista di proprietà
typedef struct
{
    Proprieta_t testa; // elemento di testa della lista di proprietà
    int lunghezza;     // lunghezza della lista
} Libro_t;

// struct per tenere la richiesta da fare al server
typedef struct
{
    Richiesta_tipo_t tipo;
    size_t lunghezza;
    String dati;
} Richiesta_t;

// struct dei server a cui fare la richiesta
typedef struct
{
    String nome;      // nome del server
    String indirizzo; // indirizzo ip del server
    int porta;        // porta per accedere al server
    int fd_server;    // file descriptor della connect al server
} Server_t;

//* FUNZIONI CLASSICHE CON CONTROLLO ERRORI

void Perror(String messaggio)
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
// funzione per rimuovere un carattere c dalla stringa s
void rimuoviChar(String s, char c)
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

// restituisce un puntatore ad una struct di tipo Richiesta_t contenente i dati ottenuti dal parse
Richiesta_t *parseDati(int argc, char **argv)
{
    Richiesta_t *richiesta = (Richiesta_t *)malloc(sizeof(Richiesta_t));
    String proprieta;

    richiesta->dati = malloc(sizeof(char *));
    richiesta->tipo = MSG_QUERY;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0)
        {
            richiesta->tipo = MSG_LOAN;
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

// TODO restituisce un array di struct di tipo Server_t contenti i dati letti dal file .conf
Server_t *parseConfFile(FILE *confFile)
{
    return NULL;
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

    printf("%d, %ld, %s\n", richiesta->tipo, richiesta->lunghezza, richiesta->dati);

    //- legge bib.conf e si connette ai server
    FILE *confFile = Fopen("bib.conf", "r");
    Server_t *servers;

    servers = parseConfFile(confFile);

    //- invia richiesta al server

    //- aspetta per la risposta del server e stampa

    //! chiusura/liberazione della memoria usata
    free(richiesta->dati);
    fclose(confFile);

    return 0;
}