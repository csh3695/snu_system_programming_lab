#include "csapp.h"
#include "cache.h"
#include <stdbool.h>

#define MAX_OBJECT_SIZE 102400
#define MAX_HEADER_SIZE 16384
#define HOSTLEN 256
#define SERVLEN 8

void *init(void *vargp);
void doit(int connfd);
void forward(rio_t *rio, int connfd, char *server, int *port, char *filename);
int parse_uri(char *uri, char* server, char *filename);

cache *caches = NULL;
sem_t mutex;

int main(int argc, char** argv) {
    /* if port number not given */
    if(argc != 2) {
    	fprintf(stderr, "Port number required\n");
    	return 1;
    }

    /* initiate cache */
   	caches = init_cache();
    V(&mutex);

    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(struct sockaddr_in);
    pthread_t tid;

    Signal(SIGPIPE, SIG_IGN);
    int listenfd = Open_listenfd(argv[1]);

	while (1) {
        int *connfdp = malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Pthread_create(&tid, NULL, init, connfdp);
    }
    exit(0);
}

/*  Detach all threads & begin routine */
void *init(void *vargp) {
	int connfd = *((int*)vargp);    // copy and free fd to avoid race condition
	Pthread_detach(pthread_self()); // detach thread
    free(vargp);
    doit(connfd);   // operate
    Close(connfd);  // close
    return NULL;
}

/* Parse request and process */
void doit(int connfd){
	rio_t client_rio, server_rio;
	
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version;
    char header[MAX_HEADER_SIZE];

	/* Read header to buffer */
	Rio_readinitb(&client_rio, connfd);
	if (!Rio_readlineb(&client_rio, buf, MAXLINE)) return;
    sscanf(buf, "%s %s HTTP/1.%c", method, uri, &version);
    printf("%s", buf);
    /* Process only GET request */
    if (strcasecmp(method, "GET")) {
        fprintf(stderr, "501 Not Implemented : Does not implement this method");
        return;
    }

	while(Rio_readlineb(&client_rio, buf, MAXLINE) != 0){
        if(strcmp(buf, "\r\n") == 0) break;
        if(strstr(buf, "Connection:") == buf) sprintf(buf, "Connection: close\r\n");
        else if(strstr(buf, "Proxy-Connection:") == buf) sprintf(buf, "Proxy-Connection: close\r\n");
        strcat(header, buf);
    }
    strcat(header, "\r\n");

    char filename[MAXLINE], server[MAXLINE], port[MAXLINE];
    size_t size;
    int server_port = parse_uri(uri, server, filename);

    /* Check if the finding payload exist in cache */
    P(&mutex);
    char *payload = get_payload(caches, server_port, &size, server, filename);
    V(&mutex);

    /* Hit : write to client and return */
    if(payload){
    	Rio_writen(connfd, payload, size);
        return;
    }

    /* Miss : get from server */
    sprintf(port, "%d", server_port);
    int srcfd = Open_clientfd(server, port);

    sprintf(buf, "GET /%s HTTP/1.0\r\n%s", filename, header);
    Rio_writen(srcfd, buf, strlen(buf));    // send header to server

    Rio_readinitb(&server_rio, srcfd);
    forward(&server_rio, connfd, server, &server_port, filename);   // get from server and forward to client

    Close(srcfd);
    return;
}

/* Read from server and forward(write) to client */
void forward(rio_t *rio, int connfd, char *server, int *port, char *filename){
	char buf[MAX_OBJECT_SIZE], payload[MAX_OBJECT_SIZE];
    size_t size, read=0;
	while ((size = Rio_readnb(rio, buf, MAXLINE))){
		Rio_writen(connfd, buf, size);
        memcpy(payload + read, buf, size);
        read += size;
	}
    /* Save the payload in cache */
    P(&mutex);
    insert(caches, *port, read, payload, server, filename);
    V(&mutex);
	return;
}

/* Parser request uri and return server's port */
int parse_uri(char* uri, char* server, char* filename) {
    if (strstr(uri, "http://") != uri) {
        fprintf(stderr, "Error: invalid uri!\n");
        exit(0);
    }
    uri += strlen("http://");   // remove http://
    int port;
    char *file;
    char *tmp;    
    strncpy(server, uri, strpbrk(uri, ":/")-uri);
    tmp = strchr(uri, ':');
    file = strchr(uri, '/');
    *file = '\0';
    if(!tmp) port = 80; // default port : 80
    else port = atoi(tmp+1);    // write speficied port
    strcpy(filename, file+1);
    return port;
}