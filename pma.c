/* packed memory array routines */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "bitlib.h"

/*
 *  A packed memory array is a resizing array storing ordered values.
 *  The array is divided into windows, and then further divided into
 *  segments, each of lg N size.  Insertions are done via search of an
 *  implicit binary tree.
 *
 *  Spaces are left between values to meet a density criterion in order
 *  to reduce the cost of insertions, and the array is periodically
 *  rebalanced to ensure equal density throughout.  Once the array reaches
 *  a maximum density, its size is doubled.  Likewise, its size is halved
 *  when a minimum density is reached on deletion.
 */

/*
 *   N = 40
 *   lg N = 6 = seg size
 *
 *   nsegs = hyperceil(40/6) = 8
 *
 *                 48
 *         24              24
 *     12      12      12      12
 *   6 | 6 | 6 | 6 | 6 | 6 | 6 | 6
 *
 *
 */
struct pma {
    /* thresholds for density at lowest level */
    double max_seg_density;
    double min_seg_density;

    /* thresholds for entire array density */
    double max_density;
    double min_density;

    int *region;        /* allocated array */
    int size;           /* total size of array */
    int segsize;        /* size of a segment */
    int nsegs;          /* number of segments */
    int height;         /* height of the implicit tree */
};

/*
 *  Constructs a new PMA of the given size.
 *
 *  initial_size is rounded up so that the number of segments
 *  is a power of two.
 */
struct pma *pma_new(int initial_size)
{
    struct pma *p = malloc(sizeof(*p));

    p->segsize = ilog2(hyperceil(initial_size));
    p->nsegs = hyperceil(p->segsize);
    p->size = p->nsegs * p->segsize;
    p->region = calloc(sizeof(*p->region) * p->size, 1);
    p->height = ilog2(p->size);

    p->max_seg_density = 0.92;
    p->min_seg_density = 0.08;
    p->max_density = 0.7;
    p->min_density = 0.3;

    return p;
}


int empty(int *array, int index)
{
    return array[index] == 0;
}

void pma_print(struct pma *p)
{
    int i;
    for (i = 0; i < p->size; i++)
    {
        if (empty(p->region, i))
            printf(".. ");
        else
            printf("%02d ", p->region[i]);
    }
    printf("\n");
}

int rebalance_insert(struct pma *p, int start, int height, int occupation,
                     int val)
{
    int window_size = p->segsize * (1 << height);
    int window_start = start - start % window_size;
    int window_end = window_start + window_size;
    int length = window_size;
    int i, j;
    bool inserted = false;
    int pos;

    printf("balance size %d (seg size %d)\n", window_size, p->segsize);

    occupation += 1;

    /* stride is number of extra spaces to add per non-empty item,
     * in fixed-point with 8-bits of resolution
     */
    unsigned int stride = ((length - occupation) << 8) / occupation;

    /* First move all of the elements to the left, including the
     * item we wish to insert
     */
    for (i=j=window_start; i < window_end; i++)
    {
        if (!empty(p->region, i))
        {
            /* insert val in the proper place */
            if (p->region[i] > val && !inserted) {
                p->region[j++] = val;
                inserted = true;
            }

            p->region[j++] = p->region[i];
        }
    }
    if (!inserted)
        p->region[j++] = val;

    /* zero rest of array */
    memset(&p->region[j], 0, (window_end - j) * sizeof(p->region[0]));

    /* now redistribute from the right.  We compute the target
     * spaces using fixed-point in pos.
     */
    pos = ((window_end - 1) << 8) - stride;
    for (i = j-1; i >= 0; i--)
    {
        j = pos >> 8;
        p->region[j] = p->region[i];
        if (j != i)
            p->region[i] = 0;

        pos -= (1 << 8) + stride;
    }
    return 0;
}

double target_density(struct pma *p, int height)
{
    int max_height = p->height;

    double result = p->max_density + (p->max_density - p->max_seg_density) *
        (max_height - height)/(double) max_height;

    printf("tgt density at h %d is %f\n", height, result);
    return result;
}

/*
 *  Compute the density of a window at a certain start position
 *  and tree height.
 */
double density(struct pma *p, int start, int height, int *occupation)
{
    int occupied = 0;
    int i;

    /* scan backwards to the window start and ahead to the window end */
    int window_size = p->segsize * (1 << height);

    /* FIXME, start & ~window_size or so */
    int window_start = window_size * (start / window_size);
    int window_end = window_size * (start / window_size + 1);

    for (i = start; i >= window_start; i--)
        if (!empty(p->region, i))
            occupied++;

    for (i = start + 1; i < window_end; i++)
        if (!empty(p->region, i))
            occupied++;

    *occupation = occupied;

    printf("density at h %d is %g\n", height, (double)occupied / window_size);
    return (double)occupied / window_size;
}

/* insert y at pointer x.  can be binary search or tree driven. */
void pma_insert_at(struct pma *p, int x, int y)
{
    int occupation = 0;
    int height = 0;

    /* if the slot is free, go ahead and take it */
    if (empty(p->region, x))
    {
        p->region[x] = y;
        return;
    }

    while (density(p, x, height, &occupation) > target_density(p, height))
        height++;

    /* FIXME, if this assert fails, we need to double the PMA size */
    assert(height <= p->height);

    /* rebalance this window and add y */
    rebalance_insert(p, x, height, occupation, y);
}

bool pma_bin_search(int *region, int min_i, int max_i, int value,
                    int *ins_pt)
{
    int mid;
    int l, r;

    mid = (min_i + max_i)/2;

    while (min_i != max_i)
    {
        /* now scan left & right to find a non-empty slot */
        l = r = mid;
        while (empty(region, l) &&
               empty(region, r) &&
               (l > min_i || r < max_i))
        {
            if (l > min_i) l--;
            if (r < max_i) r++;
        }

        if (!empty(region, l))
            mid = l;
        else if (!empty(region, r))
            mid = r;
        else  /* entire region is empty, insert at current midpoint */
            break;

        if (region[mid] < value)
            min_i = mid + 1;
        else if (region[mid] > value)
            max_i = mid - 1;
        else
            break;

        mid = (min_i + max_i)/2;
    }
    *ins_pt = mid;
    return region[mid] == value;
}

int pma_insert(struct pma *p, int y)
{
    int pos;
    bool found;

    /* binary search to find the predecessor item */
    found = pma_bin_search(p->region, 0, p->size-1, y, &pos);
    if (!found)
        pma_insert_at(p, pos, y);

    return 0;
}


int main(int argc, char *argv[])
{
    struct pma *p;

    p = pma_new(5);

    pma_print(p);
    pma_insert(p, 1);
    pma_print(p);
    pma_insert(p, 10);
    pma_print(p);
    pma_insert(p, 33);
    pma_print(p);
    pma_insert(p, 1);
    pma_print(p);
    pma_insert(p, 2);
    pma_print(p);
    pma_insert(p, 8);
    pma_print(p);
    pma_insert(p, 12);
    pma_print(p);
    pma_insert(p, 1);
    pma_print(p);
    pma_insert(p, 35);
    pma_print(p);
    pma_insert(p, 37);
    pma_print(p);
    pma_insert(p, 3);
    pma_print(p);
    pma_insert(p, 4);
    pma_print(p);
    return 0;
}

