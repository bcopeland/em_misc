#ifndef VEBTREE_H
#define VEBTREE_H

#include "types.h"

void veb_tree_insert(struct veb *veb, key_t search_key);
struct tree_node *veb_tree_find(struct veb *veb, key_t search_key);
struct veb *veb_tree_new(int nitems);
void veb_tree_free(struct veb *veb);
void veb_tree_print(struct veb *veb);

void veb_tree_set_node_key(struct veb *veb, int bfs_index, key_t key);
void veb_tree_recompute_index(struct veb *veb, int bfs_index);
void veb_tree_link_leaf(struct veb *veb, int bfs_index, struct leaf *leaf);
#endif
