/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"
#define THREADNUM 10
#define SBUF_SIZE 1024
#define MAX 100


typedef struct item {
    int ID;  // stock ID
    int left_stock; // number of left stock
    int price; // stock price
    int readcnt;
    sem_t mutex;
}item;

typedef struct stockitem {
    item* root[MAX];
    int num_item;
}stockitem;


stockitem stock;

typedef struct {
    int* buf; /* Buffer array */
    int n; /* Maximum number of slots */
    int front; /* buf[(front+1)%n] is the first item */
    int rear; /* buf[rear%n] is the last item */
    sem_t mutex; /* Protects accesses to buf */
    sem_t slots; /* Counts available slots */
    sem_t items; /* Counts available items */
} sbuf_t;

sbuf_t sbuf;

static sem_t w;
static sem_t mutex;
static sem_t fd_mutex;
static int bytecnt;
int connfdcnt = 0;


int request_buy(int stockID, int buycnt);
void request_show(char buf[]);
void request_sell(int stockID, int sellcnt);
void echo(int connfd);
void sbuf_init(sbuf_t* sp, int n);
void sbuf_deinit(sbuf_t* sp);
void sbuf_insert(sbuf_t* sp, int item);
int sbuf_remove(sbuf_t* sp);
void savestock();
void echo_cnt(int connfd);
static void init_echo_cnt(void);
void load_tree(FILE* fp, stockitem* items);
void* thread(void* vargp);
void handle_show_cmd(char* ret_buf);
void handle_buy_cmd(int id, int cnt, char* ret_buf);
void handle_sell_cmd(int id, int cnt, char* ret_buf);
void handle_invalid_cmd(char* ret_buf);

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t* sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n; /* Buffer holds max of n items */
    sp->front = sp->rear = 0; /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1); /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n); /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0); /* Initially, buf has 0 items */
}
/* Clean up buffer sp */
void sbuf_deinit(sbuf_t* sp)
{
    Free(sp->buf);
}

/* Insert an item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t* sp, int item)
{
    P(&sp->slots); /* Wait for available slot */
    P(&sp->mutex); /* Lock the buffer */
    sp->buf[(++sp->rear) % (sp->n)] = item; /* Insert the item */
    V(&sp->mutex); /* Unlock the buffer */
    V(&sp->items); /* Announce available item */
}

/* Remove and return the first item from buffer sp */
int sbuf_remove(sbuf_t* sp)
{
    int item;
    P(&sp->items); /* Wait for available item */
    P(&sp->mutex); /* Lock the buffer */
    item = sp->buf[(++sp->front) % (sp->n)]; /* Remove the item */
    V(&sp->mutex); /* Unlock the buffer */
    V(&sp->slots); /* Announce available slot */
    return item;
}

/* Save the current state of the stock to the file "stock.txt" */
void savestock() {
    FILE* fp = fopen("stock.txt", "w");
    if (fp == NULL) {
        printf("Failed to open file.\n");
        return;
    }
    item* tmp = NULL;
    for (int i = 1; i <= stock.num_item; i++) {
        tmp = stock.root[i];
        fprintf(fp, "%d %d %d\n", tmp->ID, tmp->left_stock, tmp->price);
    }
    fclose(fp);
}

/* Write the current state of the stock to a provided buffer */
void request_show(char buf[]) {
    //int cnt = stock.num_item;

    char* ptr = buf; // pointer to the current position in the string
    int length = 0;  // length of the added string

    length = sprintf(ptr, "show\n");
    ptr += length; // move the pointer to the end of the newly added string

    // stockserver에서 show를 통해 목록을 출력하고 싶을 경우 사용
    /*for (int i = 1; i <= cnt; i++) {
        length = sprintf(ptr, "%d %d %d\n", stock.root[i]->ID, stock.root[i]->left_stock, stock.root[i]->price);
        ptr += length;
    } */

}
/* request_buy function */
int request_buy(int stockID, int buycnt)
{
    int cnt = stock.num_item;
    int i;
    int flag = 0;
    for (i = 1; i <= cnt; i++)
    {
        // Lock the current stock item before checking/modifying it.
        P(&stock.root[i]->mutex);
        if (stock.root[i]->ID == stockID && stock.root[i]->left_stock >= buycnt)
        {
            // If the stock item exists and has enough stock, reduce the stock
            // count, increment the read count, and set flag to 1.
            stock.root[i]->left_stock -= buycnt;
            stock.root[i]->readcnt++;
            flag = 1;
            // Unlock the stock item after modification.
            V(&stock.root[i]->mutex);
            break;
        }
        // Unlock the stock item if it does not need to be modified.
        V(&stock.root[i]->mutex);
    }
    // Return 1 if the buy request was successful, 0 otherwise.
    return flag;
}

/* request_sell function*/
void request_sell(int stockID, int sellcnt)
{
    int i = 1;
    while (i <= stock.num_item)
    {
        if (stock.root[i]->ID == stockID)
        {
            // Lock the current stock item before modifying it.
            P(&stock.root[i]->mutex);
            // If the stock item exists, increase the stock count and increment the read count.
            stock.root[i]->left_stock += sellcnt;
            stock.root[i]->readcnt++;
            // Unlock the stock item after modification.
            V(&stock.root[i]->mutex);
            break;
        }
        i++;
    }
    // Return after either modifying a stock item or checking all stock items without modification.
    return;
}

/* Load the state of the stock from the file "stock.txt" into the provided items array */
void load_tree(FILE* fp, stockitem* items) {

    item* newnode = NULL;
    int id, left, price;
    items = (stockitem*)malloc(sizeof(stockitem));
    items->num_item = 0;
    stock.num_item = 0;

    fp = fopen("stock.txt", "r");
    if (fp == NULL) {
        printf("Failed to open file.\n");
        return;
    }
    int ret;
    while ((ret = fscanf(fp, "%d %d %d", &id, &left, &price)) != EOF) {
        // Check if fscanf successfully read three values
        if (ret == 3) {
            newnode = (item*)malloc(sizeof(item));
            newnode->ID = id;
            newnode->left_stock = left;
            newnode->price = price;

            Sem_init(&newnode->mutex, 0, 1);
            stock.root[++stock.num_item] = newnode;
        }
        else {
            printf("Failed to read item data. [Appropriate data format] id left price \n");
            break;
        }
    }
    fclose(fp);
}

void* thread(void* vargp)
{
    Pthread_detach(pthread_self());
    while (1)
    {
        int connfd = sbuf_remove(&sbuf); /* Remove connfd from buf */
        echo_cnt(connfd); /* Service client */
        P(&fd_mutex);
        Close(connfd);
        connfdcnt--;
        V(&fd_mutex);
        if (connfdcnt == 0)
        {
            savestock();
            return NULL;
        }
    }
}

static void init_echo_cnt(void)
{
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0, 1);
    bytecnt = 0;
}

void handle_show_cmd(char* ret_buf) {
    request_show(ret_buf);
}

void handle_buy_cmd(int id, int cnt, char* ret_buf) {
    int flag = request_buy(id, cnt);
    if (flag)
        strcpy(ret_buf, "[buy] success\n");
    else
        strcpy(ret_buf, "Not enough left stock\n");
}

void handle_sell_cmd(int id, int cnt, char* ret_buf) {
    request_sell(id, cnt);
    strcpy(ret_buf, "[sell] success\n");
}

void handle_invalid_cmd(char* ret_buf) {
    strcpy(ret_buf, "Invalid command\n");
}

void echo_cnt(int connfd)
{
    int n;
    char buf[MAXLINE], cmd[MAXLINE], ret_buf[MAXLINE];
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    int id, cnt;

    Pthread_once(&once, init_echo_cnt);
    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("server received %d bytes\n", n);
        n = sscanf(buf, "%s %d %d", cmd, &id, &cnt);

        if (n == 1 && !strcmp(cmd, "show")) {
            handle_show_cmd(ret_buf);
        }
        else if (n == 1 && !strcmp(cmd, "exit")) {
            break;
        }
        else if (n == 3 && !strcmp(cmd, "buy")) {
            handle_buy_cmd(id, cnt, ret_buf);
        }
        else if (n == 3 && !strcmp(cmd, "sell")) {
            handle_sell_cmd(id, cnt, ret_buf);
        }
        else {
            handle_invalid_cmd(ret_buf);
        }
        Rio_writen(connfd, ret_buf, MAXLINE);
    }
}

int main(int argc, char **argv) 
{
    FILE* fp = NULL;
    stockitem* bt = NULL;
    pthread_t tid;
    int listenfd, connfd;
    int i;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    
    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(0);
    }

    load_tree(fp, bt);

    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUF_SIZE);
    for (i = 0; i < THREADNUM; i++) {
        Pthread_create(&tid, NULL, thread, NULL);
    }

    Sem_init(&fd_mutex, 0, 1);

    while (1) {
	    clientlen = sizeof(struct sockaddr_storage); 
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        //Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        //printf("Connected to (%s, %s)\n", client_hostname, client_port);
        P(&fd_mutex);
        sbuf_insert(&sbuf, connfd);
        connfdcnt++;
        V(&fd_mutex);
    }
    exit(0);
}
/* $end echoserverimain */
