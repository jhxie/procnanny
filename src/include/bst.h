#ifndef BST_H_
#define BST_H_

#include <stdbool.h>
#include <stddef.h>

/*
 *No typedef for struct bst_node_ to avoid collision with other headers:
 *noteably link in <unistd.h>.
 */
struct bst {
        struct bst_node_ *root;
        size_t numnode;
};

static inline bool bst_isempty(struct bst *current_bst)
        __attribute__((always_inline));
static inline size_t bst_report(struct bst *current_bst)
        __attribute__((always_inline));

static inline bool bst_isempty(struct bst *current_bst)
{
                return current_bst->root == NULL;
}

static inline size_t bst_report(struct bst *current_bst)
{
                return current_bst->numnode;
}
#endif
