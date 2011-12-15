#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "types.h"
#include "bitlib.h"

/*
 *  Given the BFS numbering of a node, compute its vEB position.
 *
 *  BFS number is in the range of 1..#nodes.
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
     * position.  Thus we can find the subtree root by shifting off
     * depth - top_height rightmost bits.
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
    bfs_number &= (1 << subtree_depth) - 1;

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
    return &veb->elements[bfs_to_veb(bfs, veb->height) - 1];
}

static inline int bfs_left(int bfs_num)
{
    return 2 * bfs_num;
}

static inline int bfs_right(int bfs_num)
{
    return 2 * bfs_num + 1;
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

void veb_tree_link_leaf(struct veb *veb, int bfs_index, struct leaf *leaf)
{
    node_at(veb,bfs_index)->leaf = leaf;
}

/*
 *  Update this node so that it contains the maximum of the
 *  left subtree and the left-most node of the right subtree.
 *  This ensures that every node to the right is at least greater
 *  than or equal to this node.
 */
void veb_tree_recompute_index(struct veb *veb, int bfs_index)
{
    int i, nexti;
    int lefti = bfs_left(bfs_index);
    int righti = bfs_right(bfs_index);

    struct tree_node *left = node_at(veb, lefti);
    struct tree_node *right = node_at(veb, righti);

    int leftval = left->key;
    int rightval = right->key;

    /* explore to the left of right */
    nexti = bfs_left(righti);
    for (i=ilog2(righti) + 1; i < veb->height; i++)
    {
        struct tree_node *next = node_at(veb, nexti);
        nexti = bfs_left(nexti);
        rightval = next->key;
    }
    node_at(veb, bfs_index)->key = max(leftval, rightval);
}

/*
 *  Search through the tree to the end, or the first unoccupied
 *  node.  Insert the key there (may overwrite something.)
 */
void veb_tree_insert(struct veb *veb, key_t search_key)
{
    int i;
    int cmp;
    struct tree_node *root = veb->elements;
    struct tree_node *node = root;
    int bfs_num = 1;

    for (i=0; i < veb->height; i++)
    {
        int lefti = 2 * bfs_num - 1;
        int righti = 2 * bfs_num;
        struct tree_node *left = &root[bfs_to_veb(lefti, veb->height)];
        struct tree_node *right = &root[bfs_to_veb(righti, veb->height)];

        if (node->key == 0)
            goto found;

        cmp = search_key - node->key;

        if (cmp < 0) {
            node = left;
            bfs_num = lefti;
        }
        else {
            node = right;
            bfs_num = righti;
        }
    }
found:
    node->key = search_key;
}

/*
 *  Search down the tree to find the leaf that points to the segment
 *  containing search_key.  The internal node containing the leaf pointer
 *  is returned.
 */
struct tree_node *veb_tree_find(struct veb *veb, key_t search_key)
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

        if (cmp < 0) {
            node = left;
            bfs_num = lefti;
        }
        else {
            node = right;
            bfs_num = righti;
        }
    }
    return node;
}

/*
 * Create a new complete VEB layout tree capable of storing at
 * least nitems in the leaves.  The height of the tree will be
 * lg 2*nitems.
 */
struct veb *veb_tree_new(int nitems)
{
    int nodes = 2 * nitems - 1;
    int height = ilog2(nodes) + 1;

    struct veb *veb = malloc(sizeof(*veb));
    struct tree_node *elements = malloc(sizeof(*elements) * nodes);

    memset(elements, 0, sizeof(*elements) * nodes);

    veb->elements = elements;
    veb->height = height;
    return veb;
}

void veb_tree_free(struct veb *veb)
{
    free(veb->elements);
    free(veb);
}

