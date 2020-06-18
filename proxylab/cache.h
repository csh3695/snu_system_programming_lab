#ifndef __CACHE_H__
#define __CACHE_H__

#define MAX_CACHE_SIZE 1048576

typedef struct node{
    int port;
    size_t size;
    char* payload;
    char* host;
    char* filename;
    struct node *prev;
    struct node *next;
} node;

typedef struct cache{
    size_t size;
    struct node *start;
    struct node *end;
} cache;


cache *init_cache();
void insert(cache *c, int port, size_t size, char* payload, char* host, char* filename);
void clear_node(node *nd);
void evict(cache *c);
void front_append(cache *c, node *nd);
void front_move(cache *c, node *nd);
node *find(cache *c, int port, char *host, char *filename);
char *get_payload(cache *c, int port, size_t *size, char* host, char* filename);

#endif
