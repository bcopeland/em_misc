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
    mask = (1 << num_subtrees) - 1;
    bfs_number &= ~(mask << subtree_depth);

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

/* Update parent pointers in array to point to this node
 *
 * node - original tree node
 * pos - location of new node in the array
 * out_buf - array to update
 */
void update_parent_pointers(struct tree_node *node,
                            int pos,
                            struct tree_node *out_buf)
{
    if (node->parent) {
        struct tree_node *parent;
        parent = &out_buf[node->parent->veb_pos];

        if (parent->left == node)
            parent->left = &out_buf[pos];
        else {
            assert(parent->right == node);
            parent->right = &out_buf[pos];
        }

        out_buf[pos].parent = parent;
        out_buf[pos].veb_pos = pos;
    }
}

struct tree_node *build_empty_tree(int height)
{
    struct tree_node *node;
    if (height == 0)
        return NULL;

    node = malloc(sizeof(*node));
    memset(node, 0, sizeof(*node));

    node->left = build_empty_tree(height-1);
    node->right = build_empty_tree(height-1);

    return node;
}

/*
 *  Encodes the binary tree specified by root into an array.
 */
int encode_tree(struct tree_node *root, int pos, int height,
                struct tree_node *out_buf)
{
    int split;
    int tsize, bsize;

    int i, j;

    if (height == 1)
    {
        memcpy(&out_buf[pos], root, sizeof(*root));
        root->veb_pos = pos;
        update_parent_pointers(root, pos, out_buf);
        return pos+1;
    }

    split = hyperceil((height + 1) / 2);
    bsize = split;
    tsize = height - bsize;

    // recursively layout half-height tree
    pos = encode_tree(root, pos, tsize, out_buf);

    // encode all of the children from left to right at height/2
    for (i=0; i < (1 << tsize); i++)
    {
        struct tree_node *tree = root;
        for (j=tsize-1; j >=0; j--) {
            if (i & (1 << j)) {
                tree = tree->right;
            }
            else {
                tree = tree->left;
            }
            if (!tree)
                break;
        }
        if (tree)
            pos = encode_tree(tree, pos, bsize, out_buf);
    }
    return pos;
}

void veb_tree_print_in_order(struct tree_node *tree)
{
    if (!tree)
        return;

    veb_tree_print_in_order(tree->left);
    printf("%d\n", tree->key);
    veb_tree_print_in_order(tree->right);
}

/*
 *  Search down the tree to find the leaf that points to the segment
 *  containing search_key.  The name is a bit of a misnomer, the
 *  parent of the leaf is returned.
 */
struct tree_node *veb_tree_find_leaf(struct tree_node *root, key_t search_key)
{
    int cmp;

    if (!root)
        return NULL;

    if (root->leaf)
        return root;

    cmp = search_key - root->key;

    if (cmp <= 0)
        return veb_tree_find_leaf(root->left, search_key);
    else
        return veb_tree_find_leaf(root->right, search_key);
}

void veb_tree_free(struct tree_node *root)
{
    if (!root)
        return;

    veb_tree_free(root->left);
    veb_tree_free(root->right);

    free(root);
}

/*
 *  Given an array of tree nodes, set up the links to
 *  descendent nodes to form a complete binary tree.
 *  The array entries used are selected according to
 *  vEB layout.
 *
 *  TODO: do this in-place instead of building two trees.
 *
 *  Also TODO: use mmap for the array.
 */
struct tree_node *veb_tree_init(int nitems)
{
    int nodes = nitems * 2;
    int height = ilog2(nodes);

    struct tree_node *out = malloc(sizeof(struct tree_node *) * nodes);
    struct tree_node *tree = build_empty_tree(height);
    encode_tree(tree, 0, height, out);
    veb_tree_free(tree);
    return out;
}
