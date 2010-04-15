#ifndef PMA_H
#define PMA_H
struct pma *pma_new(int initial_size);
void pma_print(struct pma *p);
int pma_insert(struct pma *p, key_t key);
struct leaf *pma_search(struct pma *p, key_t key);
void pma_free(struct pma *p);
#endif
