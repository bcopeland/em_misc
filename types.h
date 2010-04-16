#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int key_t;

/* Holds an item stored in the PMA */
struct leaf {
    struct tree_node *parent;
    key_t key;
    char value[10];
};

/* Binary tree that indexes segments in the PMA */
struct tree_node {
    key_t key;
    key_t min_key;
    key_t max_key;
    struct leaf *leaf;
};

/* A tree in van Emde Boas layout.  All pointers are implicit. */
struct veb {
    int height;
    struct tree_node *elements;
};

/* Packed Memory Array */
struct pma {
    /* thresholds for density at lowest level */
    double max_seg_density;
    double min_seg_density;

    /* thresholds for entire array density */
    double max_density;
    double min_density;

    struct leaf *region;        /* allocated array */
    int size;           /* total size of array */
    int segsize;        /* size of a segment */
    int nsegs;          /* number of segments */
    int height;         /* height of the implicit tree */
    int nitems;         /* total number of items */

    /* index structure (array in veb layout) */
    struct veb *index;
};

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

#endif
