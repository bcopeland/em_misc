#ifndef VEBTREE_H
#define VEBTREE_H

#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define MAX_HEIGHT 64

typedef struct {
    u64 objectid;
    u8 type;
    u64 offset;
} btrfs_key_t;

struct tree_node {
    btrfs_key_t key;
    struct tree_node *left;
    struct tree_node *right;
    int payload;
};

struct level_info {
    int subtree_depth;
    int top_size;
    int bottom_size;
};

/* A tree in van Emde Boas layout.  All pointers are implicit. */
struct veb {
    int height;
    int min_density;        /* min allowable density (16.16 fixed) */
    int max_density;        /* max allowable density */
    int count;              /* # of nodes */
    struct tree_node *elements;
    struct tree_node *scratch;
    struct level_info *level_info;

    int iter_pos[MAX_HEIGHT];
};

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

/* Function prototypes */
int veb_tree_insert(struct veb *veb, btrfs_key_t *search_key);
struct tree_node *veb_tree_search(struct veb *veb, btrfs_key_t *search_key);
struct veb *veb_tree_new(int nitems, bool clear);
void veb_tree_free(struct veb *veb);
void veb_tree_print(struct veb *veb);
void pointerize(struct veb *veb);
void die(char *s);
#endif
