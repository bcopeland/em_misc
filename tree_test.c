#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bitlib.h"
#include <glib.h>
#include <assert.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

typedef uint8_t u8;
typedef uint64_t u64;

struct tree_node {
    struct tree_node *left, *right, *parent;
    int val;
    int number;
    int height;
    int n_desc;
    int veb_pos;
};

struct tree_info {
    int B;
    int D;
    int T;
} tinfo[16];


int bit_set(uint8_t *bitmap, int bit)
{
    return !!(bitmap[bit >> 3] & (1 << (bit & 7)));
}

int set_bit(uint8_t *bitmap, int bit, int val)
{
    bitmap[bit >> 3] |= (val << (bit & 7));
}

static int swap(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

int fls(int f)
{
    int order;
    for (order = 0; f; f >>= 1, order++) ;

    return order;
}

int hyperfloor(int f)
{
    return 1 << (fls(f) - 1);
}

int hyperceil(int f)
{
    return 1 << fls(f-1);
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

int encode_tree_dfs(struct tree_node *root, int pos, int height,
                struct tree_node *out_buf)
{

    if (!root)
        return pos;

    memcpy(&out_buf[pos], root, sizeof(*root));
    root->veb_pos = pos;
    update_parent_pointers(root, pos, out_buf);
    pos++;

    pos = encode_tree_dfs(root->left, pos, height, out_buf);
    pos = encode_tree_dfs(root->right, pos, height, out_buf);
    return pos;
}

int encode_tree_bfs(struct tree_node *root, int pos, int height,
                struct tree_node *out_buf)
{
    GQueue *queue = g_queue_new();
    struct tree_node *node;

    g_queue_push_tail(queue, root);
    while (!g_queue_is_empty(queue))
    {
        node = g_queue_pop_head(queue);

        memcpy(&out_buf[pos], node, sizeof(*node));
        node->veb_pos = pos;
        update_parent_pointers(node, pos, out_buf);
        pos++;

        if (node->left)
            g_queue_push_tail(queue, node->left);

        if (node->right)
            g_queue_push_tail(queue, node->right);
    }
}

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

void print_tree_dfs(struct tree_node *tree)
{
    if (!tree)
        return;

    printf("%d\n", tree->val);
    if (tree->parent)
        printf("parent: %d\n", tree->parent->val);
    print_tree_dfs(tree->left);
    print_tree_dfs(tree->right);
}

void print_tree_in_order(struct tree_node *tree)
{
    if (!tree)
        return;

    print_tree_in_order(tree->left);
    printf("%d %d\n", tree->val, tree->height);
    print_tree_in_order(tree->right);
}

struct tree_node *build_numbered_tree_int(int height)
{
    struct tree_node *node;
    if (height == 0)
        return;

    node = malloc(sizeof(*node));
    memset(node, 0, sizeof(*node));

    node->left = build_numbered_tree_int(height-1);
    node->right = build_numbered_tree_int(height-1);

    return node;
}

struct tree_node *build_numbered_tree(int height)
{
    struct tree_node *tree;
    struct tree_node *node;
    GQueue *queue = g_queue_new();
    int i = 0;

    tree = build_numbered_tree_int(height);
    // BFS
    g_queue_push_tail(queue, tree);
    while (!g_queue_is_empty(queue))
    {
        // dequeue node, add children to tail
        node = g_queue_pop_head(queue);
        node->number = i++;
        if (node->left)
        {
            g_queue_push_tail(queue, node->left);
            g_queue_push_tail(queue, node->right);
        }
    }

    return tree;
}

void update_tree_height(struct tree_node *node)
{
    int l=0, r=0;
    int lct=0, rct=0;

    if (!node)
        return;

    if (node->left) {
        l = node->left->height;
        lct = node->left->n_desc;
    }

    if (node->right) {
        r = node->right->height;
        rct = node->right->n_desc;
    }

    node->height = 1 + max(l,r);
    node->n_desc = 1 + lct + rct;
    update_tree_height(node->parent);
}

struct tree_node *rotate_right(struct tree_node *tree)
{
    if (tree->left)
    {
        struct tree_node *tmp;

        tmp = tree->left;
        tree->left = tree->left->right;
        tmp->right = tree;

        tmp->parent = tree->parent;
        tree->parent = tmp;

        if (tree->left)
            tree->left->parent = tree;

        // automatically fixes new parent
        update_tree_height(tree);

        return tmp;
    }
    return tree;
}

struct tree_node *rotate_left(struct tree_node *tree)
{
    int itmp;

    if (tree->right)
    {
        struct tree_node *tmp;

        tmp = tree->right;
        tree->right = tree->right->left;
        tmp->left = tree;

        tmp->parent = tree->parent;
        tree->parent = tmp;

        if (tree->right)
            tree->right->parent = tree;

        // automatically fixes new parent
        update_tree_height(tree);

        return tmp;
    }
    return tree;
}

struct tree_node *partition_tree(struct tree_node *tree, int k)
{
    int l = 0;

    if (!tree)
        return NULL;

    if (tree->left)
        l = tree->left->n_desc;

    if (l > k) {
        tree->left = partition_tree(tree->left, k);
        tree = rotate_right(tree);
    }
    else if (l < k) {
        tree->right = partition_tree(tree->right, k - l - 1);
        tree = rotate_left(tree);
    }

    return tree;
}

/* recursively partition on the medians */
struct tree_node *balance_tree(struct tree_node *tree)
{
    if (!tree)
        return NULL;

    tree = partition_tree(tree, tree->n_desc / 2);
    tree->left = balance_tree(tree->left);
    tree->right = balance_tree(tree->right);
    return tree;
}

struct tree_node *tree_node_create(int data)
{
    struct tree_node *new_tree;

    new_tree = malloc(sizeof(*new_tree));
    memset(new_tree, 0, sizeof(*new_tree));

    new_tree->height = 1;
    new_tree->n_desc = 1;
    new_tree->val = data;
    return new_tree;
}

struct tree_node *tree_find(struct tree_node *root, int data)
{
    int cmp;

    if (!root)
        return NULL;

    cmp = data - root->val;

    if (cmp == 0)
        return root;

    if (cmp < 0)
        return tree_find(root->left, data);
    else
        return tree_find(root->right, data);
}

struct tree_node *tree_find_insert(struct tree_node *root, int data)
{
    struct tree_node *node;
    int cmp;

    if (!root)
        return tree_node_create(data);

    cmp = data - root->val;

    /* already have this node, ignore */
    if (cmp == 0)
        return root;

    if (cmp < 0)
    {
        /* left side */
        if (root->left) {
            node = tree_find_insert(root->left, data);
            update_tree_height(root);
            return node;
        }

        node = tree_node_create(data);
        node->parent = root;
        root->left = node;
        update_tree_height(root);

        return node;
    }
    else
    {
        /* right side */
        if (root->right) {
            node = tree_find_insert(root->right, data);
            update_tree_height(root);
            return node;
        }

        node = tree_node_create(data);
        node->parent = root;
        root->right = node;
        update_tree_height(root);
        return node;
    }
}

struct tree_node *tree_add_value(struct tree_node *root, int data)
{
    struct tree_node *result;

    result = tree_find_insert(root, data);
    return (root) ? root : result;
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

#define NKEYS (1 << 16)
//#define NKEYS (1 << 4)
//#define NTRIALS (100000000)
#define NTRIALS (100000000)

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

/* runs the profile loop and returns total # of us */
u64 runprof(struct tree_node *tree, int *values, int nkeys, int ntrials)
{
    int i;
    struct timespec start_time;
    struct timespec end_time;
    struct timespec diff_time;

    srandom(100);
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (i=0; i < ntrials; i++)
    {
        struct tree_node *t;

        int which = i % NKEYS; // random() % NKEYS;
        t = tree_find(tree, values[which]);
        assert(t->val == values[which]);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    timespec_sub(&end_time, &start_time, &diff_time);
    return diff_time.tv_sec * 1000000 + (diff_time.tv_nsec / 1000);
}

int permute_array(int *array, int count)
{
    int i, j;

    srand(100);
    for (i=0; i < count; i++)
    {
        j = i + (random() % (count - i));
        swap(&array[i], &array[j]);
    }
}

int main(int argc, char *argv[])
{
    FILE *fp;
    struct tree_node *encode_buf;
    struct tree_node *encode_buf_bfs;
    struct tree_node *encode_buf_dfs;
    int n_strs = 0, i;
    int count = 0;
    int *values = malloc(NKEYS * sizeof(int));
    u64 time;

    struct tree_node *tree = NULL;

    srandom(1);
    for (i=0; i < NKEYS; i++)
    {
        values[i] = random();
        tree = tree_add_value(tree, values[i]);
        count++;
    }
    printf("%d keys, unbalanced height : %d\n", count, tree->height);
    tree = balance_tree(tree);
    printf("balanced height : %d\n", tree->height);

    encode_buf = malloc(count * sizeof(struct tree_node));
    encode_buf_bfs = malloc(count * sizeof(struct tree_node));
    encode_buf_dfs = malloc(count * sizeof(struct tree_node));

    encode_tree(tree, 0, tree->height, encode_buf);
    encode_tree_bfs(tree, 0, tree->height, encode_buf_bfs);
    encode_tree_dfs(tree, 0, tree->height, encode_buf_dfs);

    free(empty_cache());

    permute_array(values, NKEYS);

    u64 base_time = runprof(tree, values, NKEYS, NTRIALS);
    u64 bfs_time = runprof(&encode_buf_bfs[0], values, NKEYS, NTRIALS);
    u64 dfs_time = runprof(&encode_buf_dfs[0], values, NKEYS, NTRIALS);
    u64 veb_time = runprof(&encode_buf[0], values, NKEYS, NTRIALS);

    printf("%g %g %g %g\n",
        base_time / 1000000.,
        bfs_time / 1000000.,
        dfs_time / 1000000.,
        veb_time / 1000000.);
}
