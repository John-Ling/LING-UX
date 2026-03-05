#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hash_table.h"
#include "utils.h"

#define VIS_DELAY_US  500000   /* 0.5 s between major steps  */
#define VIS_FAST_US   180000   /* 0.18 s for bulk operations */
#define NODE_KEY_W    8        /* key field display width inside a node box */

/* ------------------------------------------------------------------ */
/*  Visualisation                                                       */
/* ------------------------------------------------------------------ */

static void _vis_format_key(const KeyValue* pair, HashType type,
                             char* buf, size_t size)
{
    switch (type)
    {
        case STRING: snprintf(buf, size, "\"%s\"",  (char*)pair->key);  break;
        case INT:    snprintf(buf, size, "%d",      *(int*)pair->key);  break;
        case CHAR:   snprintf(buf, size, "'%c'",    *(char*)pair->key); break;
        default:     snprintf(buf, size, "?");                          break;
    }
}

/*
 * Renders the hash table to stdout.
 *
 * Layout (vertical bucket array, horizontal chains):
 *
 *   buckets : 10    stored : 4    load : 0.40
 *   op      : insert 3
 *
 *   +---------+
 *   |  [  0]  |---->[ 'a'     ]---->[ 'b'     ]---->NULL
 *   +---------+
 *   |  [  1]  |
 *   +---------+
 *   |  [  2]  |---->[ 'c'     ]---->NULL
 *   +---------+
 *   ...
 */
void ht_visualize(HashTable* table, const char* op)
{
    /* clear screen, cursor to top-left */
    printf("\033[2J\033[H");

    /* ---- stats header ------------------------------------------- */
    printf("  buckets : %zu\n",  table->bucketCount);
    printf("  stored  : %d\n",   table->storedKeyCount);
    printf("  load    : %.2f\n",
           (double)table->storedKeyCount / (double)table->bucketCount);
    if (op != NULL)
        printf("  op      : %s\n", op);
    printf("\n");

    /* ---- bucket array (shared-border style) ---------------------- */
    printf("  +---------+\n");

    for (int i = 0; i < (int)table->bucketCount; i++)
    {
        LinkedList* bucket = table->buckets[i];

        printf("  |  [%3d]  |", i);

        if (bucket != NULL && bucket->head != NULL)
        {
            ListNode* node = bucket->head;
            while (node != NULL)
            {
                char buf[32] = {0};
                _vis_format_key((KeyValue*)node->value,
                                table->type, buf, sizeof(buf));
                printf("---->[ %-*s ]", NODE_KEY_W, buf);
                node = node->next;
            }
            printf("---->NULL");
        }

        printf("\n");
        printf("  +---------+\n");
    }

    printf("\n");
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
/*  Key printers (kept for ht_print_keys compatibility)                */
/* ------------------------------------------------------------------ */

static void print_key_chr(const void* data)
{
    if (data == NULL) { puts("NULL"); return; }
    printf("%c ", *(char*)((KeyValue*)data)->key);
}

static void print_key_int(const void* data)
{
    if (data == NULL) return;
    printf("%d ", *(int*)((KeyValue*)data)->key);
}

/* ------------------------------------------------------------------ */
/*  Tests                                                               */
/* ------------------------------------------------------------------ */

int string_test(void);
int int_test(void);

int main(void)
{
    puts("== CHAR KEY TEST ==");
    string_test();

    puts("== INT KEY TEST ==");
    int_test();

    return EXIT_SUCCESS;
}

int int_test(void)
{
    HashTable* table = LibHashTable.create(INT, 10, sizeof(int), free);
    if (table == NULL) { puts("create failed"); return EXIT_FAILURE; }

    int val = 234;
    char op[64];

    int keys[] = { 1, 2323, 2, 3 };
    for (int i = 0; i < 4; i++)
    {
        snprintf(op, sizeof(op), "insert %d", keys[i]);
        LibHashTable.insert_int(table, keys[i], &val);
        ht_visualize(table, op);
        usleep(VIS_DELAY_US);
    }

    LibHashTable.delete_int(table, 2, free);
    ht_visualize(table, "delete 2");
    usleep(VIS_DELAY_US);

    int del_res = LibHashTable.delete_int(table, 2, free);
    ht_visualize(table,
                 del_res == EXIT_FAILURE
                     ? "delete 2 again  ->  not found (correct)"
                     : "delete 2 again  ->  ERROR: should have failed");
    usleep(VIS_DELAY_US);

    KeyValue* kv = LibHashTable.get_int(table, 3);
    if (kv != NULL)
    {
        snprintf(op, sizeof(op), "get 3  ->  found, data = %d",
                 *(int*)kv->data);
        ht_visualize(table, op);
    }
    else
    {
        ht_visualize(table, "get 3  ->  not found");
    }
    usleep(VIS_DELAY_US);

    LibHashTable.free(table, free);
    printf("\033[2J\033[H  int_test: table freed.\n\n");
    usleep(VIS_DELAY_US / 2);
    return EXIT_SUCCESS;
}

int string_test(void)
{
    HashTable* table = LibHashTable.create(CHAR, 30, sizeof(int), free);
    if (table == NULL) { puts("create failed"); return EXIT_FAILURE; }

    int a = 5;
    char op[32];

    /* bulk insert 'a'-'z' */
    for (char c = 'a'; c <= 'z'; c++)
    {
        snprintf(op, sizeof(op), "insert '%c'", c);
        LibHashTable.insert_chr(table, c, &a);
        ht_visualize(table, op);
        usleep(VIS_FAST_US);
    }

    /* verify all present */
    int all_found = 1;
    for (char c = 'a'; c <= 'z'; c++)
    {
        if (LibHashTable.get_chr(table, c) == NULL) { all_found = 0; break; }
    }
    ht_visualize(table,
                 all_found ? "get 'a'-'z'  ->  all found"
                           : "get 'a'-'z'  ->  ERROR: missing keys");
    usleep(VIS_DELAY_US);

    /* bulk delete 'a'-'z' */
    for (char c = 'a'; c <= 'z'; c++)
    {
        snprintf(op, sizeof(op), "delete '%c'", c);
        LibHashTable.delete_chr(table, c, free);
        ht_visualize(table, op);
        usleep(VIS_FAST_US);
    }

    LibHashTable.free(table, free);
    printf("\033[2J\033[H  string_test: table freed.\n\n");
    usleep(VIS_DELAY_US / 2);
    return EXIT_SUCCESS;
}