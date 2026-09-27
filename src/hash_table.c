#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../include/hash_table.h"

// Fowler-Noll-Vo (FNV-1a)
static unsigned long hash(const char *key, size_t table_size)
{
    unsigned long h = 2166136261UL;
    while (*key)
    {
        h ^= (unsigned char)(*key++);
        h *= 16777619UL;
    }
    return h % table_size;
}

HashTable *ht_create(size_t initial_size)
{
    if (initial_size < 1)
    {
        return NULL;
    }

    HashTable *ht = malloc(sizeof(HashTable));
    if (!ht)
    {
        return NULL;
    }

    ht->buckets = calloc(initial_size, sizeof(Entry *));
    if (!ht->buckets)
    {
        free(ht);
        return NULL;
    }

    ht->count = 0;
    ht->size = initial_size;
    return ht;
}

void ht_free(HashTable *ht)
{
    for (size_t i = 0; i < ht->size; i++)
    {
        Entry *entry = ht->buckets[i];
        if (!entry)
        {
            continue;
        }
        while (entry)
        {
            Entry *next = entry->next;
            free(entry->key);
            free(entry->value);
            free(entry);
            entry = next;
        }
    }

    free(ht->buckets);
    free(ht);
}

int ht_set(HashTable *ht, const char *key, const char *value)
{
    if (!key)
    {
        return 1;
    }

    if (!value)
    {
        return 1;
    }

    // Resize the table when we hit 75% capacity.
    if ((double)ht->count / ht->size > 0.75)
    {
        ht_resize(ht, ht->size * 2);
    }

    Entry *e = malloc(sizeof(Entry));
    if (!e)
    {
        return 1; // Failed to allocate
    }

    e->key = strdup(key);
    if (!e->key)
    {
        free(e);
        return 1;
    }
    e->value = strdup(value);
    if (!e->value)
    {
        free(e->key);
        free(e);
        return 1;
    }
    e->next = NULL;

    unsigned long h = hash(key, ht->size);
    Entry *current = ht->buckets[h];

    // Set for first time
    if (!current)
    {
        ht->buckets[h] = e;
        ht->count++;
        return 0;
    }

    while (current)
    {
        // If the current Entry is matched on key, overwrite value.
        if (strcmp(e->key, current->key) == 0)
        {
            free(current->value);
            current->value = e->value;
            free(e->key);
            free(e);
            return 0; // Return out
        }

        if (current->next)
        {
            current = current->next;
        }
        else
        {
            current->next = e;
            ht->count++;
            break;
        }
    }

    return 0;
}

char *ht_get(HashTable *ht, const char *key)
{
    if (!key)
    {
        return NULL;
    }

    unsigned long h = hash(key, ht->size);
    Entry *current = ht->buckets[h];
    while (current)
    {
        if (strcmp(key, current->key) == 0)
        {
            // One to decide later: we are returning our copy of the value.
            // If we or the caller free the value, it could cause unknown outcomes.
            return current->value;
        }

        current = current->next;
    }

    return NULL;
}

void ht_del(HashTable *ht, const char *key)
{
    if (!key)
    {
        return;
    }

    unsigned long h = hash(key, ht->size);
    Entry *current = ht->buckets[h];
    Entry *head = current;
    Entry *parent = NULL;
    while (current)
    {
        if (strcmp(key, current->key) != 0)
        {
            parent = current;
            current = current->next;
            continue;
        }

        bool is_head = current == head;
        Entry *next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        ht->count--;

        if (parent)
        {
            parent->next = next;
        }

        if (is_head)
        {
            ht->buckets[h] = next;
        }

        return;
    }
}

int ht_resize(HashTable *ht, size_t new_size)
{
    if (new_size < 1)
    {
        return 1;
    }

    Entry **buckets = calloc(new_size, sizeof(Entry *));
    if (!buckets)
    {
        return 1;
    }

    for (size_t i = 0; i < ht->size; i++)
    {
        Entry *entry = ht->buckets[i];
        while (entry)
        {
            unsigned long h = hash(entry->key, new_size);

            Entry *next = entry->next;
            entry->next = buckets[h];
            buckets[h] = entry;
            entry = next;
        }
    }

    free(ht->buckets);
    ht->buckets = buckets;
    ht->size = new_size;
    return 0;
}