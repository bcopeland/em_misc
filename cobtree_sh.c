#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <perfmon/pfmlib_perf_event.h>
#include "veb_small_height.h"
#include "bitlib.h"

/*
 *  This implements the "Locality preserving dynamic dictionary" of
 *  Bender et al.  The workhorse is pma.c which uses a vEB layout
 *  binary tree for indexing a resizable array.
 */

void die(char *s)
{
    printf("%s\n", s);
    exit(-1);
}

void permute_array(btrfs_key_t *array, int count)
{
    int i, j;
    btrfs_key_t tmp;

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

struct timespec start_time;
struct timespec end_time;

int perf_fd;
u64 perf_values[3];

void perf_init()
{
    int ret;
    ret = pfm_initialize();
    if (ret != PFM_SUCCESS)
        die("couldn't init pfm");
}

void perf_start()
{
    int ret;
    struct perf_event_attr attr;

    memset(&attr, 0, sizeof(attr));

    ret = pfm_get_perf_event_encoding("LLC_MISSES", PFM_PLM3,
        &attr, NULL, NULL);

    if (ret != PFM_SUCCESS)
        die("couldn't get encoding");

    attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
                       PERF_FORMAT_TOTAL_TIME_RUNNING;
    attr.disabled = 1;

    perf_fd = perf_event_open(&attr, getpid(), -1, -1, 0);
    if (perf_fd < 0)
        die("couldn't open fd");
    ret = ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, 0);
    if (ret)
        die("couldn't enable ioctl");
}

void perf_end()
{
    size_t ret;

    ret = ioctl(perf_fd, PERF_EVENT_IOC_DISABLE, 0);
    if (ret)
        die("couldn't disable ioctl");
    ret = read(perf_fd, perf_values, sizeof(perf_values));
    if (ret < sizeof(perf_values))
        die("couldn't read event\n");
}

void time_start()
{
    clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void time_end()
{
    clock_gettime(CLOCK_MONOTONIC, &end_time);
}

u64 time_elapsed()
{
    struct timespec diff_time;

    timespec_sub(&end_time, &start_time, &diff_time);
    return diff_time.tv_sec * 1000000 + (diff_time.tv_nsec / 1000);
}

/* runs the profile loop and returns total # of us */
u64 runprof(struct veb *veb, btrfs_key_t *keys, int nkeys, int ntrials)
{
    int i;

    fprintf(stderr, ".\n");

    perf_start();
    time_start();
    for (i=0; i < ntrials; i++)
    {
        struct tree_node *node;

        int which = i % nkeys;
        node = veb_tree_search(veb, &keys[which]);
        if (node == NULL ||
            node->key.objectid != keys[which].objectid)
        {
            printf("Could not recover %lld (got %lld)\n",
                (unsigned long long) keys[which].objectid,
                (node) ?  (unsigned long long) node->key.objectid : 0);
        }
    }
    time_end();
    perf_end();
    return time_elapsed();
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
//#define NTRIALS (100000000)
#define NTRIALS (1000000)
int main(int argc, char *argv[])
{
    int i;
    int nkeys = 1 << 8;
    int max_keys = MAX_KEYS;
    struct veb *veb;
    btrfs_key_t *values;
    u64 insert_time = 0;
    u64 search_time = 0;
    int opt;
    bool do_inserts = false, do_searches = false;
    bool clear = true;

    while ((opt = getopt(argc, argv, "isk:")) != -1)
    {
        switch(opt) {
        case 'i':
            do_inserts = true;
            break;
        case 's':
            do_searches = true;
            break;
        case 'k':
            nkeys = max_keys = atoi(optarg);
            break;
        default:
            die("unknown param");
        }
    }
    if (!do_inserts && !do_searches)
        do_inserts = do_searches = true;

    clear = do_inserts;

    perf_init();

    srandom(10);
    for (; nkeys <= max_keys; nkeys <<= 1)
    {
        veb = veb_tree_new(nkeys/8, clear);
        values = malloc(nkeys * sizeof(btrfs_key_t));

        time_start();
        for (i=0; i < nkeys; i++)
        {
            values[i].objectid = random();
            values[i].type = random();
            values[i].offset = random();
            if (do_inserts)
                veb_tree_insert(veb, &values[i]);
        }
        time_end();
        insert_time = time_elapsed();

        permute_array(values, nkeys);

        pointerize(veb);

        free(empty_cache());

        if (do_searches)
            search_time = runprof(veb, values, nkeys, NTRIALS);

        if (!perf_values[2])
            perf_values[2] = 1;

        u64 cycles = (u64) ((double)perf_values[0] * perf_values[1])/
            perf_values[2];

        printf("%d %g %g %lld\n", ilog2(nkeys), search_time / 1000000.,
               insert_time / 1000000.,
               (unsigned long long) cycles);

        fflush(stdout);
        veb_tree_free(veb);
    }
    return 0;
}
