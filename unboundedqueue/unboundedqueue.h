typedef struct richiesta
{
    int clientSocket; // socket del client che ha fatto la richiesta
    char tipo;        // tipo di richiesta: 'Q' o 'L'
    int lunghezza;    // lunghezza della richiesta
    char *req;        // rihiesta nel formato proprietà:valore;
    struct richiesta *next;
} Client_req;

// inizializza la coda
void initCoda();
// distrugge la coda
void destroyCoda();
// inserisce un elemento in coda
void push(Client_req *req);
/*
 * Toglie l'elemento di testa della coda
 *
 * @retval
 * Client_req * se esistono elementi nella coda
 * @retval
 * NULL se non ci sono elementi nella coda e la connessione è stata chiusa
 */
Client_req *pop();
// segnala che la connesione è stata chiusa
void closeConn();