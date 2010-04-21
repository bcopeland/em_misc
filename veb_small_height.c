#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "veb_small_height.h"
#include "bitlib.h"

#define NULL_KEY ~0

static inline int tree_size(int height)
{
    return (1 << height) - 1;
}

/*
 *  Given the BFS numbering of a node, compute its vEB position.
 *
 *  BFS number is in the range of 1..#nodes.  The return value
 *  is also 1-indexed.
 */
int bfs_to_veb(int bfs_number, int height)
{
    int split;
    int top_height, bottom_height;
    int depth;
    int subtree_depth, subtree_root, num_subtrees;
    int toptree_size, subtree_size;
    unsigned int mask;
    int prior_length;

    /* if this is a size-3 tree, bfs number is sufficient */
    if (height <= 2)
        return bfs_number;

    /* depth is level of the specific node */
    depth = ilog2(bfs_number);

    /* the vEB layout recursively splits the tree in half */
    split = hyperceil((height + 1) / 2);
    bottom_height = split;
    top_height = height - bottom_height;

    /* node is located in top half - recurse */
    if (depth < top_height)
        return bfs_to_veb(bfs_number, top_height);

    /*
     * Each level adds another bit to the BFS number in the least
     * significant position.  Thus we can find the subtree root by
     * shifting off depth - top_height rightmost bits.
     */
    subtree_depth = depth - top_height;
    subtree_root = bfs_number >> subtree_depth;

    /*
     * Similarly, the new bfs_number relative to subtree root has
     * the bit pattern representing the subtree root replaced with
     * 1 since it is the new root.  This is equivalent to
     * bfs' = bfs / sr + bfs % sr.
     */

    /* mask off common bits */
    num_subtrees = 1 << top_height;
    mask = subtree_root << subtree_depth;
    bfs_number &= ~mask;

    /* replace it with one */
    bfs_number |= 1 << subtree_depth;

    /*
     * Now we need to count all the nodes before this one, then the
     * position within this subtree.  The number of siblings before
     * this subtree root in the layout is the bottom k-1 bits of the
     * subtree root.
     */
    subtree_size = (1 << bottom_height) - 1;
    toptree_size = (1 << top_height) - 1;

    prior_length = toptree_size +
        (subtree_root & (num_subtrees - 1)) * subtree_size;

    return prior_length + bfs_to_veb(bfs_number, bottom_height);
}

static inline struct tree_node *node_at(struct veb *veb, int bfs)
{
    if (bfs_to_veb(bfs,veb->height) - 1 > tree_size(veb->height))
    {
        printf("size: %d %d %d\n", bfs, bfs_to_veb(bfs, veb->height),
            tree_size(veb->height));
    }
    return &veb->elements[bfs_to_veb(bfs, veb->height) - 1];
}

static inline bool node_empty(struct tree_node *node)
{
    return node->key == NULL_KEY;
}

static inline bool node_valid(struct veb *veb, int bfs)
{
    return bfs > 0 &&
        bfs <= ((1 << veb->height) - 1) &&
        !node_empty(node_at(veb, bfs));
}

static inline int bfs_left(int bfs_num)
{
    return 2 * bfs_num;
}

static inline int bfs_right(int bfs_num)
{
    return 2 * bfs_num + 1;
}

static inline int bfs_parent(int bfs_num)
{
    return bfs_num / 2;
}

static inline int bfs_is_right(int bfs_num)
{
    return bfs_num & 1;
}

static inline int bfs_peer(int bfs_num)
{
    if (bfs_is_right(bfs_num))
        return bfs_num & ~1;

    return bfs_num | 1;
}

static int bfs_first(struct veb *veb, int subtree_root)
{
    int bfs = subtree_root;

    if (!node_valid(veb, bfs))
        return -1;

    while (node_valid(veb, bfs))
        bfs = bfs_left(bfs);

    return bfs_parent(bfs);
}

static int bfs_next(struct veb *veb, int bfs_num, int subtree_root)
{
    int bfs_next, tail;

    /* If at root with no right child, done */
    if (bfs_num == subtree_root &&
        !node_valid(veb, bfs_right(bfs_num)))
        return -1;

    /* If there's a right child, go right then all the way left */
    if (node_valid(veb, bfs_right(bfs_num)))
    {
        bfs_next = bfs_right(bfs_num);

        while (node_valid(veb, bfs_next))
            bfs_next = bfs_left(bfs_next);

        return bfs_parent(bfs_next);
    }

    /* Else go back up until we can move right */
    tail = bfs_num;
    bfs_next = bfs_parent(bfs_num);
    while (bfs_is_right(tail) &&
           bfs_next != subtree_root)
    {
        tail = bfs_next;
        bfs_next = bfs_parent(bfs_next);
    }

    /* at root from right side? */
    if (bfs_next <= subtree_root && bfs_is_right(tail))
        return -1;

    return bfs_next;
}

void veb_tree_print_in_order(struct veb *veb)
{
    int bfs = bfs_first(veb, 1);

    while (bfs != -1)
    {
        printf("%d\n", node_at(veb, bfs)->key);
        bfs = bfs_next(veb, bfs, 1);
    }
    printf("\n");
}

void veb_tree_print(struct veb *veb)
{
    int i;
    for (i=0; i < (1 << veb->height) - 1; i++)
    {
        if (is_power_of_two(i+1))
            printf("\n");

        printf("%04d  ", node_at(veb, i+1)->key);
    }
    printf("\n");
}


void veb_tree_set_node_key(struct veb *veb, int bfs_index, key_t key)
{
    node_at(veb,bfs_index)->key = key;
}

/*
 * Compute the density in an array.
 * Occupation is the number of elements, height is the height of
 * the subtree.
 *
 * Result in 16.16 fixed point.
 */
int density(int occupation, int height)
{
    int nodes = (1 << height) - 1;

    return (occupation << 16) / nodes;
}

/*
 * Compute target density for a level in the tree.
 * Height is the height of the subtree from the leaf.
 *
 * Result in 16.16 fixed point.
 */
int target_density(struct veb *veb, int height)
{
    return veb->max_density -
        ((veb->max_density - veb->min_density) >> 16) *
         ((height << 16) / veb->height);
}

void veb_tree_distribute(struct veb *veb, int bfs_root,
                         key_t *scratch, int count, int node_count)
{
    int item = count/2;

    /* since there are an odd number of elements at any tree,
     * left and right always have same size
     */
    int child_sz = node_count / 2;

    assert(bfs_root < (1 << veb->height));

    if (item == -1)
        node_at(veb, bfs_root)->key = NULL_KEY;
    else
        node_at(veb, bfs_root)->key = scratch[item];

    /* out of elements? tell the lower levels to zero fill */
    if (item == 0)
        count = -1;
    else
        count = item;

    if (child_sz > 0)
    {
        veb_tree_distribute(veb, bfs_left(bfs_root), scratch, count,
            child_sz);
        veb_tree_distribute(veb, bfs_right(bfs_root), &scratch[item + 1],
            count, child_sz);
    }
}

/*
 *  Returns the number of non-empty nodes in the subtree rooted
 *  at bfs_root.
 */
static int tree_occupation(struct veb *veb, int bfs_root)
{
    if (!node_valid(veb, bfs_root))
        return 0;

    return 1 + tree_occupation(veb, bfs_left(bfs_root)) +
               tree_occupation(veb, bfs_right(bfs_root));
}

static int serialize(struct veb *veb, int bfs_root, key_t insert,
                     key_t *scratch)
{
    int count = 0;
    int bfs = bfs_first(veb, bfs_root);
    bool inserted = false;

    while (bfs != -1)
    {
        struct tree_node *node = node_at(veb, bfs);

        if (insert < node->key && !inserted) {
            scratch[count++] = insert;
            inserted = true;
        }

        scratch[count++] = node->key;

        bfs = bfs_next(veb, bfs, bfs_root);
    }

    if (!inserted)
        scratch[count++] = insert;

    return count;
}

/*
 *  Embiggen the tree.
 */
void veb_tree_grow(struct veb *veb)
{
    int i;
    int height = veb->height + 1;
    int oldsize = (1 << veb->height) - 1;
    int newsize = 1 << height;
    struct tree_node *new_elem;
    key_t *new_scratch;

    new_elem = malloc(sizeof(*new_elem) * newsize);
    new_scratch = malloc(sizeof(*new_scratch) * newsize);
    memset(new_elem, NULL_KEY, sizeof(*new_elem) * newsize);

    printf("Alloced %d nodes\n", newsize);
    printf("Copying %d nodes\n", oldsize);

    for (i=1; i < oldsize; i++)
    {
        memcpy(&new_elem[bfs_to_veb(i, height) - 1],
               &veb->elements[bfs_to_veb(i, height-1) - 1],
               sizeof(new_elem[0]));
    }

    free(veb->elements);
    free(veb->scratch);

    veb->elements = new_elem;
    veb->scratch = new_scratch;
    veb->height++;
}


/*
 *  Given a leaf in the tree, compute the density at its parent,
 *  until the density is in range.
 *
 *  Then copy the values into a new array (including search_key).
 *
 *  When done, reinsert all of the keys from the array using
 *  veb_tree_distribute.
 */
int veb_tree_rebalance(struct veb *veb, int bfs_num, key_t search_key)
{
    int parent;
    int height = 2;
    int count;

    /* count the new element and the one in this leaf */
    int occupation = 2;

    /*
     * find the nearest ancestor w of v with density < target.
     * we count the occupation of our sibling plus one for the
     * parent (since the parent must be occupied).  Repeat until
     * we meet the density.
     */
    parent = bfs_parent(bfs_num);
    occupation += tree_occupation(veb, bfs_peer(bfs_num)) + 1;

    while (density(occupation, height) >= target_density(veb, height))
    {
        bfs_num = parent;
        occupation += tree_occupation(veb, bfs_peer(bfs_num)) + 1;
        parent = bfs_parent(bfs_num);
        height++;
    }
    if (height == veb->height)
    {
        /* table full, rebuild the table */
        veb_tree_grow(veb);

        /* and retry */
        return -1;
    }
    assert(parent > 0);

    /* copy the elements from parent into an array */
    count = serialize(veb, parent, search_key, veb->scratch);

    if (count >= (1 << height) - 1){
        printf("count: %d (height %d)\n", count, height);
        veb_tree_print_in_order(veb);
        assert(count < (1 << height) - 1);
    }

    /* now redistribute */
    veb_tree_distribute(veb, parent, veb->scratch, count, (1 << height) - 1);
    return 0;
}

/*
 *  Search through the tree to the first unoccupied node, then
 *  add the value.  If the new depth is greater than the height bound,
 *  then the tree must be rebalanced.
 */
int veb_tree_insert(struct veb *veb, key_t search_key)
{
    int res;
    int i;
    int cmp;
    int bfs_num = 1;

    for (i=1; i < veb->height; i++)
    {
        struct tree_node *node = node_at(veb, bfs_num);

        cmp = search_key - node->key;

        if (node_empty(node) || cmp == 0)
        {
            node->key = search_key;
            return 0;
        }

        if (cmp < 0)
            bfs_num = bfs_left(bfs_num);
        else
            bfs_num = bfs_right(bfs_num);

    }
    /* no space, rebalance and insert */
    res = veb_tree_rebalance(veb, bfs_parent(bfs_num), search_key);

    /* if tree was resized, start the search over */
    if (res == -1)
        return veb_tree_insert(veb, search_key);

    return 0;
}

/*
 *  Search down the tree to find the leaf that points to the segment
 *  containing search_key.  The internal node is returned.
 */
struct tree_node *veb_tree_search(struct veb *veb, key_t search_key)
{
    int i;
    int cmp;
    struct tree_node *root = veb->elements;
    struct tree_node *node = root;
    int bfs_num = 1;

    for (i=1; i < veb->height; i++)
    {
        int lefti = bfs_left(bfs_num);
        int righti = bfs_right(bfs_num);
        struct tree_node *left = node_at(veb, lefti);
        struct tree_node *right = node_at(veb, righti);

        cmp = search_key - node->key;

        if (cmp == 0)
            return node;

        if (cmp < 0) {
            node = left;
            bfs_num = lefti;
        }
        else {
            node = right;
            bfs_num = righti;
        }
    }
    return NULL;
}

/*
 * Create a new complete VEB layout tree capable of storing at
 * least nitems in the leaves.  The height of the tree will be
 * lg 2*nitems.
 */
struct veb *veb_tree_new(int nitems)
{
    int height = ilog2(2 * nitems) + 1;
    int nodes = 1 << height;

    struct veb *veb = malloc(sizeof(*veb));
    struct tree_node *elements = malloc(sizeof(*elements) * nodes);
    key_t *scratch = malloc(sizeof(*scratch) * nodes);

    printf("Alloced %d nodes\n", nodes);

    memset(elements, NULL_KEY, sizeof(*elements) * nodes);

    /* density range from 0.5 to 1 */
    veb->min_density = 1 << 15;
    veb->max_density = 1 << 16;

    veb->elements = elements;
    veb->scratch = scratch;
    veb->height = height;
    return veb;
}

void veb_tree_free(struct veb *veb)
{
    /* something bad going on here... */
    free(veb->elements);
    free(veb->scratch);
    free(veb);
}
