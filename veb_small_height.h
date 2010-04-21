#ifndef VEBTREE_H
#define VEBTREE_H

#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int key_t;

struct tree_node {
    key_t key;
    /* values go here.  (Darn.) */
};

/* A tree in van Emde Boas layout.  All pointers are implicit. */
struct veb {
    int height;
    int min_density;        /* min allowable density (16.16 fixed) */
    int max_density;        /* max allowable density */
    struct tree_node *elements;
    key_t *scratch;
};

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

/* Function prototypes */
int veb_tree_insert(struct veb *veb, key_t search_key);
struct tree_node *veb_tree_search(struct veb *veb, key_t search_key);
struct veb *veb_tree_new(int nitems);
void veb_tree_free(struct veb *veb);
void veb_tree_print(struct veb *veb);

void veb_tree_set_node_key(struct veb *veb, int bfs_index, key_t key);
void veb_tree_recompute_index(struct veb *veb, int bfs_index);
#endif
