/*
 * echoserveri.c - An iterative echo server
 */
 /* $begin echoserverimain */
#include "csapp.h"
#define MAX 100

typedef struct { /* represents a pool of connected descriptors */
    int maxfd; /* largest descriptor in read_set */
    fd_set read_set; /* set of all active descriptors */
    fd_set ready_set; /* subset of descriptors ready for reading */
    int nready; /* number of ready descriptors from select */
    int maxi; /* high water index into client array */
    int clientfd[FD_SETSIZE]; /* set of active descriptors */
    rio_t clientrio[FD_SETSIZE]; /* set of active read buffers */
}pool;

int byte_cnt = 0; // number of total byte
char str[MAXLINE]; // input string

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
void echo(int connfd);
void load_tree(FILE* fp, stockitem* items);
void request_show(char buf[]);
int request_buy(int stockID, int buycnt);
void request_sell(int stockID, int sellcnt);
int check_connection(pool* p);
void add_clients(int connfd, pool* p);
void init_pool(int listenfd, pool* P);
void savestock(void);
void process_client_request(char* buf, char* ret_buf);
void close_connection(pool* p, int i, int connfd);

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

/* buy function */
int request_buy(int stockID, int buycnt) {
    int cnt = stock.num_item;
    int stockIndex = -1;

    // Find stock by stockID
    for (int i = 1; i <= cnt; i++)
    {
        if (stock.root[i]->ID == stockID)
        {
            stockIndex = i;
            break;
        }
    }

    // If stock is not found, return 0
    if (stockIndex == -1) return 0;

    // If not enough stock left, return 0
    if (stock.root[stockIndex]->left_stock < buycnt) return 0;

    // Reduce the stock count
    stock.root[stockIndex]->left_stock -= buycnt;

    return 1; // Success
}

/* sell function */
void request_sell(int stockID, int sellcnt)
{
    int stockIndex = -1;
    int cnt = stock.num_item;

    // Find stock by stockID
    for (int i = 1; i <= cnt; i++)
    {
        if (stock.root[i]->ID == stockID)
        {
            stockIndex = i;
            break;
        }
    }
    // If stock is not found, return without doing anything
    if (stockIndex == -1) return;

    // Increase the stock count
    stock.root[stockIndex]->left_stock += sellcnt;
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
            stock.root[++stock.num_item] = newnode;
        }
        else {
            printf("Failed to read item data. [Appropriate data format] id left price \n");
            break;
        }
    }
    fclose(fp);
}

/* pool에 client를 추가함 */
void add_client(int connfd, pool* p) { 
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)  /* available slot을 찾음 */
        if (p->clientfd[i] < 0) {
            /* connected dsecriptor을 pool에 추가함 */
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            /* descriptor을 descriptor set에 추가함 */
            FD_SET(connfd, &p->read_set);

            /* max descriptor을 업데이트하고, high water mark를 pool함 */
            if (connfd > p->maxfd) {
                p->maxfd = connfd;
            }
            if (i > p->maxi) { 
                p->maxi = i; 
            }
            break;
        }
    /* empty slot을 찾을 수 없는 경우 apperror 출력 */
    if (i == FD_SETSIZE)
        app_error("add_client error : Too many clients");
}

/* pool의 시작 */
void init_pool(int listenfd, pool* p) {
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++)
        p->clientfd[i] = -1;

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

/* Check if there is any connected client in the pool */
int check_connection(pool* p) {
    for (int i = p->maxi; i >= 0; i--) {
        if (p->clientfd[i] > 0)
            return 1;
    }
    return 0;
}

/* Close the connection and update the pool */
void close_connection(pool* p, int i, int connfd) {
    Close(connfd);
    FD_CLR(connfd, &p->read_set);
    p->clientfd[i] = -1;
    savestock();
    if (!check_connection(p)) {
        savestock();
    }
}

/* Process client request for buying or selling */
void process_client_request(char* buf, char* ret_buf) {
    char* ptr = strtok(buf, " ");
    if (!strcmp(ptr, "buy")) {
        int id, cnt;
        int flag;
        id = atoi(strtok(NULL, " "));
        cnt = atoi(strtok(NULL, " "));
        flag = request_buy(id, cnt);

        if (flag)
            strcpy(ret_buf, "[buy] success\n");
        else
            strcpy(ret_buf, "Not enough left stock\n");
    }
    if (!strcmp(ptr, "sell")) {
        int id, cnt;
        id = atoi(strtok(NULL, " "));
        cnt = atoi(strtok(NULL, " "));
        request_sell(id, cnt);
        strcpy(ret_buf, "[sell] success\n");
    }
}

/* Check all clients in the pool and process their requests */
void check_clients(pool* p) {
    rio_t rio;
    char buf[MAXLINE];
    char ret_buf[MAXLINE];
    int connfd, n;
    
    for (int i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
                printf("Server received %d bytes\n", n);

                if (!strcmp(buf, "show\n")) {
                    request_show(ret_buf);
                }
                else if (!strcmp(buf, "exit\n")) {
                    close_connection(p, i, connfd);
                    break;
                }
                else {
                    process_client_request(buf, ret_buf);
                }

                Rio_writen(connfd, ret_buf, MAXLINE);
                ret_buf[0] = '\0';
            }
            else {
                close_connection(p, i, connfd);
            }
        }
    }
}

int main(int argc, char** argv)
{
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    // char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;
    int listenfd, connfd;
    socklen_t clientlen;
    FILE* fp = NULL;
    stockitem* bt = NULL;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    load_tree(fp, bt);
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);
        if (FD_ISSET(listenfd, &pool.ready_set)) { // add client to the pool
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            // Getnameinfo((SA*)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            // printf("Connected to (%s, %s)\n", client_hostname, client_port);
            add_client(connfd, &pool);
        }
        check_clients(&pool);
    }
    exit(0);
}
/* $end echoserverimain */
