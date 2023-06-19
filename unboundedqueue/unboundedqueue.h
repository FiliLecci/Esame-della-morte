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
// toglie l'elemento di testa
Client_req *pop();