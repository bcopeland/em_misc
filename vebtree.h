#ifndef VEBTREE_H
#define VEBTREE_H
struct tree_node *veb_tree_init(int nitems);
struct tree_node *veb_tree_find_leaf(struct tree_node *root, key_t search_key);
#endif
