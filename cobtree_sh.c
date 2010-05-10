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

int perf_fds[2];
u64 perf_values[2][3];

void perf_init()
{
    int ret;
    ret = pfm_initialize();
    if (ret != PFM_SUCCESS)
        die("couldn't init pfm");
}

void perf_start()
{
    unsigned int i;
    int ret;
    struct perf_event_attr attr[2];

    for (i=0; i < ARRAY_SIZE(attr); i++)
        memset(&attr[i], 0, sizeof(attr[0]));

    ret = pfm_get_perf_event_encoding("LLC_MISSES", PFM_PLM3,
        &attr[0], NULL, NULL);

    if (ret != PFM_SUCCESS)
        die("couldn't get encoding");

    ret = pfm_get_perf_event_encoding("INSTRUCTION_RETIRED", PFM_PLM3,
        &attr[1], NULL, NULL);

    if (ret != PFM_SUCCESS)
        die("couldn't get encoding");

    for (i=0; i < ARRAY_SIZE(attr); i++)
    {
        attr[i].read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
                       PERF_FORMAT_TOTAL_TIME_RUNNING;
        attr[i].disabled = 1;

        perf_fds[i] = perf_event_open(&attr[i], getpid(), -1, -1, 0);
        if (perf_fds[i] < 0)
                die("couldn't open fd");
    }
    for (i=0; i < ARRAY_SIZE(attr); i++)
    {
        ret = ioctl(perf_fds[i], PERF_EVENT_IOC_ENABLE, 0);
        if (ret)
                die("couldn't enable ioctl");
    }
}

void perf_end()
{
    size_t ret;
    unsigned int i;

    for (i=0; i < ARRAY_SIZE(perf_fds); i++)
    {
        ret = ioctl(perf_fds[i], PERF_EVENT_IOC_DISABLE, 0);
        if (ret)
                die("couldn't disable ioctl");
        ret = read(perf_fds[i], perf_values[i], sizeof(perf_values[i]));
        if (ret < sizeof(perf_values[i]))
                die("couldn't read event\n");
    }
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

double perf_scale(int i)
{
    if (!perf_values[i][2])
        perf_values[i][2] = 1;

    return ((double)perf_values[i][0] * perf_values[i][1])/
        perf_values[i][2];
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

        double misses = perf_scale(0);
        double cycles = perf_scale(1);

        printf("%d %g %g %g %g\n", ilog2(nkeys), search_time / 1000000.,
               insert_time / 1000000.,
               cycles,
               misses);

        fflush(stdout);
        veb_tree_free(veb);
    }
    return 0;
}
