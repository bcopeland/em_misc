#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "veb_small_height.h"

/*
 *  This implements the "Locality preserving dynamic dictionary" of
 *  Bender et al.  The workhorse is pma.c which uses a vEB layout
 *  binary tree for indexing a resizable array.
 */

void permute_array(key_t *array, int count)
{
    int i, j;
    key_t tmp;

    srand(100);
    for (i=0; i < count; i++)
    {
        j = i + (random() % (count - i));
        tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
}

void timespec_sub(struct timespec *a, struct timespec *b, struct timespec *res)
{
    res->tv_sec = a->tv_sec - b->tv_sec;
    res->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (res->tv_nsec < 0)
    {
        res->tv_sec--;
        res->tv_nsec += 1000000000;
    }
}

/* runs the profile loop and returns total # of us */
u64 runprof(struct veb *veb, int *keys, int nkeys, int ntrials)
{
    int i;
    struct timespec start_time;
    struct timespec end_time;
    struct timespec diff_time;

    fprintf(stderr, ".\n");

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (i=0; i < ntrials; i++)
    {
        struct tree_node *node;

        int which = i % nkeys;
        node = veb_tree_search(veb, keys[which]);
        if (node == NULL || node->key != keys[which])
        {
            printf("Could not recover %d (got %d)\n", keys[which],
                node->key);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    timespec_sub(&end_time, &start_time, &diff_time);
    return diff_time.tv_sec * 1000000 + (diff_time.tv_nsec / 1000);
}

void *empty_cache()
{
    /* try to kill the cache */
    char *buf = malloc(1024 * 1024 * 100);
    char *buf2 = malloc(1024 * 1024 * 100);
    memset(buf, 0, 1024 * 1024 * 100);
    memcpy(buf2, buf, 1024 * 1024 * 100);
    free(buf);
    return buf2;
}

#define MAX_KEYS (1 << 30)
#define NTRIALS 100000
int main(int argc, char *argv[])
{
    int i;
    int nkeys;
    struct veb *veb;
    key_t *values;

    srandom(10);
    for (nkeys=(1<<12); nkeys <= MAX_KEYS; nkeys <<= 1)
    {
        veb = veb_tree_new(4);
        values = malloc(nkeys * sizeof(key_t));

        for (i=0; i < nkeys; i++)
        {
            values[i] = random() % 1000000;
            veb_tree_insert(veb, values[i]);
        }

        fprintf(stderr, "%d keys\n", nkeys);

        permute_array(values, nkeys);

        u64 search_time = runprof(veb, values, nkeys, NTRIALS);

        printf("%d %g\n", nkeys,
                search_time / 1000000.);

        fflush(stdout);
        veb_tree_free(veb);
    }
    return 0;
}
