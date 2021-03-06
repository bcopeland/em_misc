#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <glib.h>
#include "veb_small_height.h"
#include "bitlib.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define NULL_KEY (0ULL)

//#define TEST_BFS
#define PTR_SEARCH

static inline int tree_size(int height)
{
    return (1 << height) - 1;
}

/*
 *  Table-lookup based bfs-to-veb.
 *
 *  We waste some cycles computing pos during a search, but
 *  this should still be much faster than recursion.
 */

static int bfs_to_veb_lu(struct level_info *l, int bfs_num)
{
    int pos[100];
    int d = 0;
    int i;

#ifdef TEST_BFS
    return bfs_num;
#endif

    int level = ilog2(bfs_num);

    pos[0] = 1;
    for (; d <= level; d++)
    {
        i = bfs_num >> (level - d);

        pos[d] = pos[l[d].subtree_depth] + l[d].top_size +
            (i & l[d].top_size) * (l[d].bottom_size);
    }
    return pos[d-1];
}

/*
 *  Setup pos tracking array for a given root.
 */
static int fill_pos(struct level_info *l, int bfs_num, int *pos)
{
    int d = 0;
    int i;

    int level = ilog2(bfs_num);

    pos[0] = 0;
    for (; d <= level; d++)
    {
        i = bfs_num >> (level - d);

        pos[d] = pos[l[d].subtree_depth] + l[d].top_size +
            (i & l[d].top_size) * (l[d].bottom_size);
    }
    return level;
}

static int bfs_to_veb(struct veb *veb, int bfs_num, int height)
{
    (void)height;
    return bfs_to_veb_lu(veb->level_info, bfs_num);
}

/*
 *  Given the BFS numbering of a node, compute its vEB position.
 *
 *  BFS number is in the range of 1..#nodes.  The return value
 *  is also 1-indexed.
 */
static int bfs_to_veb_recur(struct veb *veb, int bfs_number, int height)
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
        return bfs_to_veb_recur(veb, bfs_number, top_height);

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

    return prior_length + bfs_to_veb_recur(veb, bfs_number, bottom_height);
}

static inline int compare_key(btrfs_key_t *k1, btrfs_key_t *k2)
{
    if (k1->objectid > k2->objectid)
            return 1;
    if (k1->objectid < k2->objectid)
            return -1;
    if (k1->type > k2->type)
            return 1;
    if (k1->type < k2->type)
            return -1;
    if (k1->offset > k2->offset)
            return 1;
    if (k1->offset < k2->offset)
            return -1;
    return 0;
}

static void compute_levels(struct level_info *l, int top, int height)
{
    int split, top_height, bottom_height;

    if (height == 1)
        return;

    split = hyperceil((height + 1) / 2);
    bottom_height = split;
    top_height = height - bottom_height;

    l[top + top_height].subtree_depth = top;
    l[top + top_height].top_size = tree_size(top_height);
    l[top + top_height].bottom_size = tree_size(bottom_height);

    compute_levels(l, top, top_height);
    compute_levels(l, top + top_height, bottom_height);
}


static int compute_level_info(struct level_info *l, int height)
{
    compute_levels(l, 0, height);
    memset(&l[0], 0, sizeof(l[0]));
    return 0;
}

static inline struct tree_node *node_at_pos(struct veb *veb, int bfs_num,
                                            int *pos, int d)
{
    struct level_info *l = veb->level_info;
#ifdef TEST_BFS
    return node_at(veb, bfs_num);
#else
    pos[d] = pos[l[d].subtree_depth] + l[d].top_size +
         (bfs_num & l[d].top_size) * (l[d].bottom_size);

    return &veb->elements[pos[d]];
#endif
}

static inline struct tree_node *node_at(struct veb *veb, int bfs)
{
    return &veb->elements[bfs_to_veb(veb, bfs, veb->height) - 1];
}

static inline bool node_empty(struct tree_node *node)
{
    return node->key.objectid == NULL_KEY;
}

static inline bool node_valid(struct veb *veb, int bfs)
{
    return bfs > 0 &&
        bfs <= ((1 << veb->height) - 1) &&
        !node_empty(node_at(veb, bfs));
}

static inline bool node_valid_pos(struct veb *veb, int bfs)
{
    return bfs > 0 &&
        bfs <= ((1 << veb->height) - 1) &&
        !node_empty(node_at_pos(veb, bfs, veb->iter_pos, ilog2(bfs)));
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

    fill_pos(veb->level_info, subtree_root, veb->iter_pos);

    if (!node_valid_pos(veb, bfs))
        return -1;

    while (node_valid_pos(veb, bfs))
        bfs = bfs_left(bfs);

    return bfs_parent(bfs);
}

static int bfs_next(struct veb *veb, int bfs_num, int subtree_root)
{
    int bfs_next, tail;

    /* If at root with no right child, done */
    if (bfs_num == subtree_root &&
        !node_valid_pos(veb, bfs_right(bfs_num)))
        return -1;

    /* If there's a right child, go right then all the way left */
    if (node_valid_pos(veb, bfs_right(bfs_num)))
    {
        bfs_next = bfs_right(bfs_num);

        while (node_valid_pos(veb, bfs_next))
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
        printf("%lld\n", (unsigned long long) node_at(veb, bfs)->key.objectid);
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

        printf("%04lld  ", (unsigned long long) node_at(veb, i+1)->key.objectid);
    }
    printf("\n");
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
    int nodes = tree_size(height);
    u64 tmp = occupation;
    return ((tmp << 16) + 0x8000) / nodes;
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

double density_f(int occupation, int height)
{
    int nodes = tree_size(height);
    return (double) occupation / nodes;
}

double target_density_f(struct veb *veb, int height)
{
    double maxd = veb->max_density / 65536.0;
    double mind = veb->min_density / 65536.0;

    return maxd -
        (maxd - mind) * (((double)height - 2)/ veb->height);
}

void veb_tree_distribute_inner(struct veb *veb, int bfs_root,
                               struct tree_node *scratch, int ofs, int count,
                               int *pos, int d)
{
    int item = count/2;
    int left_ct = item;
    int right_ct = count - item - 1;

    assert(d < veb->height);

    memcpy(node_at_pos(veb, bfs_root, pos, d), &scratch[ofs + item],
           sizeof(scratch[0]));

    if (left_ct > 0)
        veb_tree_distribute_inner(veb, bfs_left(bfs_root), scratch, ofs,
                                  left_ct, pos, d+1);
    if (right_ct > 0)
        veb_tree_distribute_inner(veb, bfs_right(bfs_root), scratch,
            ofs + item + 1, right_ct, pos, d+1);
}

void veb_tree_distribute(struct veb *veb, int bfs_root,
                         struct tree_node *scratch, int ofs, int count)
{
    int pos[MAX_HEIGHT];
    int depth;

    assert(bfs_root < (1 << veb->height));

    depth = fill_pos(veb->level_info, bfs_root, pos);
    veb_tree_distribute_inner(veb, bfs_root, scratch, ofs, count, pos, depth);
}

static int tree_occupation_inner(struct veb *veb, int bfs_root)
{
    if (!node_valid_pos(veb, bfs_root))
        return 0;

    return 1 + tree_occupation_inner(veb, bfs_left(bfs_root)) +
               tree_occupation_inner(veb, bfs_right(bfs_root));
}

/*
 *  Returns the number of non-empty nodes in the subtree rooted
 *  at bfs_root.
 */
static int tree_occupation(struct veb *veb, int bfs_root)
{
    int res;
    fill_pos(veb->level_info, bfs_root, veb->iter_pos);
    res = tree_occupation_inner(veb, bfs_root);

    return res;
}

static int serialize(struct veb *veb, int bfs_root, btrfs_key_t *insert,
                     struct tree_node *scratch)
{
    int count = 0;
    int bfs = bfs_first(veb, bfs_root);
    bool inserted = false;
    struct tree_node *node;

    GQueue *queue = g_queue_new();

    while (bfs != -1)
    {
        struct tree_node *node = node_at_pos(veb, bfs, veb->iter_pos,
            ilog2(bfs));

        if (insert && compare_key(insert, &node->key) < 0 && !inserted) {
            memcpy(&scratch[count++].key, insert, sizeof(*insert));
            inserted = true;
        }
        g_queue_push_tail(queue, node);

        memcpy(&scratch[count++].key, &node->key, sizeof(node->key));

        bfs = bfs_next(veb, bfs, bfs_root);
    }

    if (!inserted && insert)
        memcpy(&scratch[count++].key, insert, sizeof(*insert));

    while (!g_queue_is_empty(queue))
    {
        node = g_queue_pop_head(queue);
        node->key.objectid = NULL_KEY;
    }
    g_queue_free(queue);

    return count;
}

static int mem_used = 0;
void *setup_mmap(int initial_size, bool clear)
{
    void *ptr;
    char *fn = "mmap_region.dat";
    int res;
    int flags = O_RDWR | O_CREAT;

    if (clear)
        flags |= O_TRUNC;

    int fd = open(fn, flags, 0644);
    if (fd < 0) {
        perror("open");
        return NULL;
    }

    ptr = mmap(NULL, 0x7fffffff, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        perror("mmap");
        die("");
    }
    res = ftruncate(fd, 0x7fffffff);
    if (res < 0)
    {
        perror("ftruncate");
        die("");
    }

    mem_used = initial_size;

    return ptr;
}

void *realloc_mem(void *ptr, int new_size)
{
    mem_used = new_size;
    return ptr;
}

void release_memory(void *ptr)
{
    msync(ptr, 0x7fffffff, MS_SYNC);
    munmap(ptr, 0x7fffffff);
}

void save_veb_info(struct veb *veb)
{
    FILE *fp = fopen("veb_info.txt", "w");
    if (!fp) {
        die("writing veb info");
    }
    fprintf(fp, "%d\n%d\n", veb->height, veb->count);
    fclose(fp);
}

void load_veb_info(struct veb *veb)
{
    int height = 0, count = 0, res;

    FILE *fp = fopen("veb_info.txt", "r");
    if (!fp) {
        die("reading veb info");
    }
    res = fscanf(fp, "%d\n%d\n", &height, &count);
    assert(res == 2);
    fclose(fp);

    veb->height = height;
    veb->count = count;
}


/*
 *  Embiggen the tree.
 */
void veb_tree_grow(struct veb *veb)
{
    int height = veb->height + 1;
    int newsize = 1 << height;
    struct tree_node *new_elem;
    struct level_info *li;
    struct tree_node *new_scratch;
    int count;

    new_elem = realloc_mem(veb->elements, sizeof(*new_elem) * newsize);
    new_scratch = malloc(sizeof(*new_scratch) * newsize);
    li = malloc(sizeof(*li) * height);
    compute_level_info(li, height);

    free(veb->scratch);
    veb->scratch = new_scratch;

    // serialize entire tree
    count = serialize(veb, 1, NULL, veb->scratch);

    veb->elements = new_elem;
    veb->level_info = li;
    veb->height++;

    // now rebuild
    veb_tree_distribute(veb, 1, veb->scratch, 0, count);
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
int veb_tree_rebalance(struct veb *veb, int bfs_num, btrfs_key_t *search_key)
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
    fill_pos(veb->level_info, parent, veb->iter_pos);
    occupation += tree_occupation(veb, bfs_peer(bfs_num)) + 1;

    while (density_f(occupation, height) > target_density_f(veb, height) &&
           height < veb->height)
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

    if (count > (1 << height) - 1){
        printf("count: %d (height %d) occupation %d dens %g\n", count, height,
                occupation, density_f(occupation,height));
        assert(count <= (1 << height) - 1);
    }

    /* now redistribute */
    veb_tree_distribute(veb, parent, veb->scratch, 0, count);
    return 0;
}

/*
 *  Search through the tree to the first unoccupied node, then
 *  add the value.  If the new depth is greater than the height bound,
 *  then the tree must be rebalanced.
 */
int veb_tree_insert(struct veb *veb, btrfs_key_t *search_key)
{
    int res;
    int d;
    int cmp;
    int bfs_num = 1;
    int pos[MAX_HEIGHT];
    struct level_info *l = veb->level_info;

    pos[0] = 0;
    for (d=0; d < veb->height; d++)
    {
#ifdef TEST_BFS
        struct tree_node *node = node_at(veb, bfs_num);
#else
        pos[d] = pos[l[d].subtree_depth] + l[d].top_size +
            (bfs_num & l[d].top_size) * (l[d].bottom_size);

        struct tree_node *node = &veb->elements[pos[d]];
#endif

        if (node_empty(node))
        {
            memcpy(&node->key, search_key, sizeof(*search_key));
            veb->count++;
            return 0;
        }

        cmp = compare_key(search_key, &node->key);

        if (cmp < 0)
            bfs_num = bfs_left(bfs_num);
        else if (cmp > 0)
            bfs_num = bfs_right(bfs_num);
        else
            return 0;
    }
    /* no space, rebalance and insert */
    res = veb_tree_rebalance(veb, bfs_parent(bfs_num), search_key);

    /* if tree was resized, start the search over */
    if (res == -1)
        return veb_tree_insert(veb, search_key);

    return 0;
}

#ifdef PTR_SEARCH
struct tree_node *veb_tree_search(struct veb *veb, btrfs_key_t *search_key)
{
    int d;
    int cmp;
    struct tree_node *node = &veb->elements[0];

    for (d=0; d < veb->height; d++)
    {
        cmp = compare_key(search_key, &node->key);

        if (cmp < 0)
            node = node->left;
        else if (cmp > 0)
            node = node->right;
        else
            return node;
    }
    return NULL;
}

#else
/*
 *  Search down the tree to find the leaf that points to the segment
 *  containing search_key.  The internal node is returned.
 */
struct tree_node *veb_tree_search(struct veb *veb, btrfs_key_t *search_key)
{
    int d;
    int cmp;
    int bfs_num = 1;
    int pos[MAX_HEIGHT];
    struct level_info *l = veb->level_info;

    pos[0] = 0;
    for (d=0; d < veb->height; d++)
    {
#ifdef TEST_BFS
        struct tree_node *node = node_at(veb, bfs_num);
#else
        pos[d] = pos[l[d].subtree_depth] + l[d].top_size +
            (bfs_num & l[d].top_size) * (l[d].bottom_size);

        struct tree_node *node = &veb->elements[pos[d]];
#endif

        cmp = compare_key(search_key, &node->key);

        if (cmp < 0)
            bfs_num = bfs_left(bfs_num);
        else if (cmp > 0)
            bfs_num = bfs_right(bfs_num);
        else
            return node;

    }
    return NULL;
}
#endif

/*
 * Create a new complete VEB layout tree capable of storing at
 * least nitems in the leaves.  The height of the tree will be
 * lg 2*nitems.
 */
struct veb *veb_tree_new(int nitems, bool clear)
{
    int height = ilog2(2 * nitems) + 1;
    int nodes = 1 << height;

    struct veb *veb = malloc(sizeof(*veb));
    struct tree_node *elements;
    struct tree_node *scratch;

    struct level_info *li;

    /* printf("Alloced %d nodes\n", nodes); */

    if (!clear) {
        load_veb_info(veb);
        height = veb->height;
        nodes = 1 << height;
    }

    elements = setup_mmap(sizeof(*elements) * nodes, clear);
    scratch = malloc(sizeof(*scratch) * nodes);
    li = malloc(sizeof(*li) * height);

    /* density range from 0.5 to 1 */
    veb->min_density = 0x08000;
    veb->max_density = 0x10000;
    compute_level_info(li, height);

    veb->level_info = li;
    veb->elements = elements;
    veb->scratch = scratch;
    veb->height = height;
    veb->count = 0;
    veb->iter_pos[0] = 0;

    return veb;
}

void pointerize(struct veb *veb)
{
    int i;

    for (i=1; i < (1 << veb->height); i++)
    {
        struct tree_node *node = node_at(veb, i);

        if (!node_empty(node))
        {
            if (node_valid(veb, bfs_left(i)))
                node->left = node_at(veb, bfs_left(i));

            if (node_valid(veb, bfs_right(i)))
                node->right = node_at(veb, bfs_right(i));
        }
    }
}

void veb_tree_free(struct veb *veb)
{
    save_veb_info(veb);
    release_memory(veb->elements);

    free(veb->scratch);
    free(veb);
}

