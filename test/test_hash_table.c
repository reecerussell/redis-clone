#include "test.h"
#include "../include/hash_table.h"

static void test_create(void)
{
    printf("create\n");
    CHECK(ht_create(0) == NULL);

    HashTable *ht = ht_create(8);
    CHECK(ht != NULL);
    CHECK(ht->size == 8);
    CHECK(ht->count == 0);
    ht_free(ht);
}

static void test_set_get(void)
{
    printf("set/get\n");
    HashTable *ht = ht_create(8);

    CHECK(ht_set(ht, "foo", "bar") == 0);
    CHECK_STR(ht_get(ht, "foo"), "bar");
    CHECK(ht_get(ht, "missing") == NULL);

    // NULL key/value are rejected without touching the table.
    CHECK(ht_set(ht, NULL, "x") != 0);
    CHECK(ht_set(ht, "x", NULL) != 0);
    CHECK(ht_get(ht, NULL) == NULL);

    ht_free(ht);
}

static void test_overwrite(void)
{
    printf("overwrite\n");
    HashTable *ht = ht_create(8);

    ht_set(ht, "key", "first");
    ht_set(ht, "key", "second");
    CHECK_STR(ht_get(ht, "key"), "second");
    CHECK(ht->count == 1);

    ht_free(ht);
}

static void test_chaining(void)
{
    printf("chaining\n");
    // size 1 forces every key into the same bucket.
    HashTable *ht = ht_create(1);

    ht_set(ht, "a", "1");
    ht_set(ht, "b", "2");
    ht_set(ht, "c", "3");
    CHECK_STR(ht_get(ht, "a"), "1");
    CHECK_STR(ht_get(ht, "b"), "2");
    CHECK_STR(ht_get(ht, "c"), "3");
    CHECK(ht->count == 3);

    ht_free(ht);
}

static void test_del(void)
{
    printf("del\n");
    HashTable *ht = ht_create(1);

    ht_set(ht, "a", "1");
    ht_set(ht, "b", "2");
    ht_set(ht, "c", "3");

    ht_del(ht, "b"); // middle of chain
    CHECK(ht_get(ht, "b") == NULL);
    CHECK_STR(ht_get(ht, "a"), "1");
    CHECK_STR(ht_get(ht, "c"), "3");

    ht_del(ht, "a"); // head of chain
    CHECK(ht_get(ht, "a") == NULL);
    CHECK_STR(ht_get(ht, "c"), "3");

    ht_del(ht, "c"); // last one left
    CHECK(ht_get(ht, "c") == NULL);
    CHECK(ht->count == 0);

    ht_del(ht, "gone"); // no-ops must not crash
    ht_del(ht, NULL);

    ht_free(ht);
}

static void test_many(void)
{
    printf("many\n");
    HashTable *ht = ht_create(16);
    char key[16], value[16];

    for (int i = 0; i < 200; i++)
    {
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        ht_set(ht, key, value);
    }

    for (int i = 0; i < 200; i++)
    {
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        CHECK_STR(ht_get(ht, key), value);
    }

    // Delete the even keys, odd ones must survive.
    for (int i = 0; i < 200; i += 2)
    {
        snprintf(key, sizeof(key), "key%d", i);
        ht_del(ht, key);
    }
    for (int i = 0; i < 200; i++)
    {
        snprintf(key, sizeof(key), "key%d", i);
        if (i % 2 == 0)
        {
            CHECK(ht_get(ht, key) == NULL);
        }
        else
        {
            CHECK(ht_get(ht, key) != NULL);
        }
    }

    ht_free(ht);
}

static void test_resize(void)
{
    printf("resize\n");
    HashTable *ht = ht_create(4);
    char key[16], value[16];

    for (int i = 0; i < 50; i++)
    {
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        ht_set(ht, key, value);
    }
    CHECK(ht->count == 50);

    // Grow: size is updated and every pair is still reachable, which only
    // holds if the rehash used the new modulus.
    CHECK(ht_resize(ht, 128) == 0);
    CHECK(ht->size == 128);
    CHECK(ht->count == 50);
    for (int i = 0; i < 50; i++)
    {
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        CHECK_STR(ht_get(ht, key), value);
    }

    // Shrink to fewer buckets than entries: chaining has to absorb the rest.
    CHECK(ht_resize(ht, 8) == 0);
    CHECK(ht->size == 8);
    CHECK(ht->count == 50);
    for (int i = 0; i < 50; i++)
    {
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        CHECK_STR(ht_get(ht, key), value);
    }

    // Resizing to the size it already has must not drop or duplicate anything.
    CHECK(ht_resize(ht, 8) == 0);
    CHECK(ht->count == 50);
    for (int i = 0; i < 50; i++)
    {
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        CHECK_STR(ht_get(ht, key), value);
    }

    // The table is still writable afterwards, not just readable.
    CHECK(ht_set(ht, "after", "resize") == 0);
    CHECK_STR(ht_get(ht, "after"), "resize");
    CHECK(ht->count == 51);
    ht_set(ht, "key0", "overwritten"); // overwrite a rehashed key
    CHECK_STR(ht_get(ht, "key0"), "overwritten");
    CHECK(ht->count == 51);
    ht_del(ht, "key0");
    CHECK(ht_get(ht, "key0") == NULL);
    CHECK(ht->count == 50);

    ht_free(ht);

    // An empty table resizes cleanly and is usable after.
    HashTable *empty = ht_create(4);
    CHECK(ht_resize(empty, 16) == 0);
    CHECK(empty->size == 16);
    CHECK(empty->count == 0);
    CHECK(ht_set(empty, "k", "v") == 0);
    CHECK_STR(ht_get(empty, "k"), "v");
    ht_free(empty);

    // ht_create(0) is refused, so a table can never legally have 0 buckets and
    // a resize to 0 must be refused too. Checked on an empty table on purpose:
    // with entries to rehash this would divide by zero inside hash().
    HashTable *zero = ht_create(4);
    CHECK(ht_resize(zero, 0) != 0);
    CHECK(zero->size >= 1);
    ht_free(zero);
}

int main(void)
{
    test_create();
    test_set_get();
    test_overwrite();
    test_chaining();
    test_del();
    test_many();
    test_resize();

    return TEST_SUMMARY();
}
