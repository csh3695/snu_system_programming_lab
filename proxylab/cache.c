#include "csapp.h"
#include "cache.h"

/* Initialize cache */
cache *init_cache(){
    cache *c = (cache*)malloc(sizeof(cache));
    c -> size = 0;
    c -> start = (node*)malloc(sizeof(node));
    c -> end = (node*)malloc(sizeof(node));
    c -> start -> prev = NULL;
    c -> end -> next = NULL;
    c -> start -> next = c -> end;
    c -> end -> prev = c -> start;
    return c;
}

/* Insert new node into cache list(linked list) */
void insert(cache *c, int port, size_t size, char* payload, char* host, char* filename){
    node *new = (node*) malloc(sizeof(node));
    new -> port = port;
    new -> size = size;
    if(payload){
        new -> payload = (char*)malloc(size);
        memcpy(new -> payload, payload, size);
    }
    if(host){
        new -> host = (char*)malloc(strlen(host));
        strcpy(new -> host, host);
    }
    if(filename){
        new -> filename = (char*)malloc(strlen(filename));
        strcpy(new -> filename, filename);
    }

    if((c -> size) + size > MAX_CACHE_SIZE) evict(c);   // evict if cache is full

    front_append(c, new);
    c -> size += size;
	return;
}

/* LRU policy : evict if cache is full */
void evict(cache *c){
    if(c == NULL || c->size == 0) return;
    node *nd = c -> end -> prev;
    nd -> prev -> next = c -> end;
    c -> end -> prev = nd -> prev;
    c -> size -= (nd -> size);
    clear_node(nd);
    return;
}

/* Free a node and all the ptrs inside it */
void clear_node(node *nd){
    free(nd -> host);
    free(nd -> payload);
    free(nd -> filename);
    free(nd);
    return;
}

/* LRU policy : append a node to the very front which is recently used */
void front_append(cache *c, node *nd){
    if(c == NULL || nd == NULL) return;
    c -> start -> next -> prev = nd;
    nd -> prev = c -> start;
    nd -> next = c -> start -> next;
    c -> start -> next = nd;
    return;
}

/* LRU policy : move a node to the very front which is recently used */
void front_move(cache *c, node *nd){
    if(c == NULL || nd == NULL || nd -> next == NULL || nd -> prev == NULL) return;
    nd -> prev -> next = nd -> next;
    nd -> next -> prev = nd -> prev;
    front_append(c, nd);
    return;
}

/* Find a node that matches port, host, filename information */
node *find(cache *c, int port, char *host, char *filename){
    if(!c) return NULL;
    node* nd = c -> start -> next;
    while(nd != (c -> end)){
        if(!strcmp(nd -> host, host)
            && !strcmp(nd -> filename, filename)
            && (nd -> port == port)) return nd;
        nd = nd -> next;
    }
    return NULL;
}

/* Return payload of the node found to match */
char *get_payload(cache *c, int port, size_t* size, char* host, char* filename){
    node* nd = find(c, port, host, filename);
    if(nd == NULL) return NULL;
    char *res = (char*)malloc(nd -> size);
    memcpy(res, nd -> payload, nd -> size);
    (*size) = nd -> size;
    front_move(c, nd);
    return res;
}