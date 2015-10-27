#include <stdbool.h>
#include <stddef.h>

#include "pwwrapper.h"

enum bst_chain_dir { BST_LEFT_CHAIN, BST_RIGHT_CHAIN, BST_CHAINSIZE };

struct bst {
        struct bst_node_ *root;
        size_t numnode;
};

struct bst_node_ {
        void *memblk;
        size_t blksize;
        struct bst_node_ *linkchain[BST_CHAINSIZE];
};

struct bst *bst_init(void)
{
        struct bst *new_bst = calloc_or_die(1, sizeof(struct bst));

        /*redundant but logical*/
        new_bst->numnode = 0;
        new_bst->root = NULL;

        return new_bst;
}

bool bst_search(struct bst *current_bst, void *memblk)
{
        /*
         *avoid undefined behavior by returning directly
         *rather than trying to dereference a NULL pointer
         */
        if (NULL == current_bst)
                return false;

        enum bst_chain_dir dir = BST_LEFT_CHAIN;
        struct bst_node_ *tmp_ptr = current_bst->root;

        while (NULL != tmp_ptr) {
                if (memblk == tmp_ptr->memblk) {
                        break;
                } else {
                        dir = memblk > tmp_ptr->memblk;
                        tmp_ptr = tmp_ptr->linkchain[dir];
                }
        }

        return NULL != tmp_ptr;
}

void *bst_insert(struct bst *current_bst, size_t nmemb, size_t size)
{
        if (NULL == current_bst)
                return NULL;

        struct bst_node_ *bstnode = calloc_or_die(1, sizeof(struct bst_node_));

        current_bst->numnode++;
        bstnode->memblk = calloc_or_die(nmemb, size);
        bstnode->blksize = nmemb * size;
        /*again, redundant*/
        bstnode->linkchain[BST_LEFT_CHAIN] = NULL;
        bstnode->linkchain[BST_RIGHT_CHAIN] = NULL;
        
        /*
         *start to insert the new node into the bst
         */
        if (NULL == current_bst->root) {
                current_bst->root = bstnode;
        } else {
                enum bst_chain_dir dir = BST_LEFT_CHAIN;
                struct bst_node_ *tmp_ptr = current_bst->root;

                while (true) {
                        dir = bstnode->memblk > tmp_ptr->memblk;

                        if (NULL == tmp_ptr->linkchain[dir])
                                break;

                        tmp_ptr = tmp_ptr->linkchain[dir];
                }

                tmp_ptr->linkchain[dir] = bstnode;
        }

        return bstnode->memblk;
}
