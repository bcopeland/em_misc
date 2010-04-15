/* packed memory array routines */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "vebtree.h"
#include "types.h"
#include "bitlib.h"

/*
 *  A packed memory array is a resizing array storing ordered values.
 *  The array is divided into windows, and then further divided into
 *  segments, each of lg N size.  Insertions are done via search of an
 *  binary tree in vEB order.
 *
 *  Spaces are left between values to meet a density criterion in order
 *  to reduce the cost of insertions, and the array is periodically
 *  rebalanced to ensure equal density throughout.  Once the array reaches
 *  a maximum density, its size is doubled.  Likewise, its size is halved
 *  when a minimum density is reached on deletion.
 */

static int rebalance_insert(struct pma *p, int start, int height,
                            int occupation, int newval);

/*
 *  Set the keys in the veb tree to match the values stored in
 *  the PMA.  Tricky?!
 */
void rebuild_index(struct pma *p)
{
}

/*
 *  Reallocates a PMA to be at least as large as new_size.
 *
 *  The number of segments should be a power of two so that
 *  we can construct a binary tree on top of it.
 *
 *  The size of segments themselves, and total size of the
 *  array, may not necessarily be a power of two.
 *
 *  So we set segment size to be log(new_size), then make the
 *  number of segments the hyperceil of that value, and
 *  increase array size accordingly.
 *
 *  This is used for initial allocation with p->size and
 *  p->region set to zero.
 */
static void pma_reallocate(struct pma *p, int new_size)
{
    int old_size = p->size;

    int round_up_size = hyperceil(new_size);

    p->segsize = ilog2(round_up_size);
    p->nsegs = hyperceil(round_up_size / p->segsize);
    p->size = p->nsegs * p->segsize;
    p->region = realloc(p->region, sizeof(*p->region) * p->size);
    p->height = ilog2(p->nsegs);

    memset(&p->region[old_size], 0,
           (p->size - old_size) * sizeof(*p->region));

    p->index = veb_tree_init(p->size);

    rebuild_index(p);
}

/*
 *  Constructs a new PMA of the given size.
 *
 *  initial_size is rounded up so that the number of segments
 *  is a power of two.
 */
struct pma *pma_new(int initial_size)
{
    struct pma *p = malloc(sizeof(*p));

    p->size = 0;
    p->region = NULL;
    pma_reallocate(p, initial_size);

    p->max_seg_density = 0.92;
    p->min_seg_density = 0.08;
    p->max_density = 0.7;
    p->min_density = 0.3;
    p->nitems = 0;

    return p;
}

void pma_free(struct pma *p)
{
}

static void pma_grow(struct pma *p)
{
    printf("before grow, size = %d, height = %d\n", p->size, p->height);
    pma_reallocate(p, p->size * 2);
    printf("after grow, size = %d, height = %d\n", p->size, p->height);
}

static int empty(struct leaf *array, int index)
{
    return array[index].key == 0;
}

void pma_print(struct pma *p)
{
    int i;
    for (i = 0; i < p->size; i++)
    {
        if (empty(p->region, i))
            printf(".. ");
        else
            printf("%02d ", p->region[i].key);
    }
    printf("\n");
}

static int rebalance_insert(struct pma *p, int start, int height,
                            int occupation, key_t new_key)
{
    int window_size = p->segsize * (1 << height);
    int window_start = start - start % window_size;
    int window_end = window_start + window_size;
    int length = window_size;
    int i, j;
    int pos;

    printf("balance size %d (seg size %d)\n", window_size, p->segsize);
    assert(window_size <= p->size);

    if (new_key)
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
            /* insert new value in the proper place */
            if (new_key && p->region[i].key > new_key) {

                /* swap it out so we don't overwrite anything */
                key_t tmp = p->region[i].key;
                p->region[j++].key = new_key;
                new_key = tmp;
            }
            else
                p->region[j++].key = p->region[i].key;
        }
    }
    if (new_key)
    {
        p->region[j++].key = new_key;
        p->nitems++;
    }

    /* zero rest of array */
    memset(&p->region[j], 0, (window_end - j) * sizeof(p->region[0]));

    /* now redistribute from the right.  We compute the target
     * spaces using fixed-point in pos.
     */
    pos = ((window_end - 1) << 8) - stride;
    for (i = j-1; i >= window_start; i--)
    {
        j = pos >> 8;
        p->region[j].key = p->region[i].key;
        if (j != i)
            p->region[i].key= 0;

        pos -= (1 << 8) + stride;
    }
    return 0;
}

static double target_density(struct pma *p, int height)
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
static double density(struct pma *p, int start, int height, int *occupation)
{
    int occupied = 0;
    int i;

    /* scan backwards to the window start and ahead to the window end */
    int window_size = p->segsize * (1 << height);
    int window_start = start - start % window_size;
    int window_end = window_start + window_size;

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
static void pma_insert_at(struct pma *p, int x, int y)
{
    int occupation = 0;
    int height = 0;

    while (density(p, x, height, &occupation) > target_density(p, height))
    {
        height++;

        /* requested height is taller than the tree, double the size */
        if (height > p->height)
        {
            pma_grow(p);
            height--;
        }
    }

    assert(height <= p->height);

    /* rebalance this window and add y */
    rebalance_insert(p, x, height, occupation, y);
}

static bool pma_bin_search(struct leaf *region, int min_i, int max_i, int value,
                    int *ins_pt)
{
    int mid;
    int l, r;

    mid = (min_i + max_i)/2;

    while (min_i < max_i)
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

        if (region[mid].key < value)
            min_i = mid + 1;
        else if (region[mid].key > value)
            max_i = mid - 1;
        else
            break;

        mid = (min_i + max_i)/2;
    }
    *ins_pt = mid;
    return region[mid].key == value;
}

struct leaf *pma_search(struct pma *p, key_t key)
{
    struct tree_node *parent;
    struct leaf *start;
    //struct leaf *new_item;
    int pos;

    parent = veb_tree_find_leaf(p->index, key);

    start = parent->leaf;

    /* scan the segment starting at parent->leaf for insert pt */
    pma_bin_search(p->region, start - &p->region[0], p->segsize, key, &pos);

    return &p->region[pos];
}

void pma_insert(struct pma *p, key_t key)
{
    struct tree_node *parent;
    struct leaf *start;
    //struct leaf *new_item;
    int pos;

    parent = veb_tree_find_leaf(p->index, key);

    start = parent->leaf;

    /* scan the segment starting at parent->leaf for insert pt */
    pma_bin_search(p->region, start - &p->region[0], p->segsize, key, &pos);

    /* now insert it */
    pma_insert_at(p, pos, key);

    /* update index 
    new_item->parent = parent;
    if (parent->key < key)
        parent->key = key;
    */
}

