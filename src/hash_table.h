#include <stdlib.h>

typedef struct Entry
{
    char *key;
    char *value;
    struct Entry *next;
} Entry;

typedef struct
{
    Entry **buckets;
    size_t size;
    size_t count;
} HashTable;

HashTable *ht_create(size_t initial_size);
void ht_free(HashTable *ht);

int ht_set(HashTable *ht, const char *key, const char *value);
char *ht_get(HashTable *ht, const char *key); // NULL if not found
void ht_del(HashTable *ht, const char *key);

int ht_resize(HashTable *ht, size_t new_size);