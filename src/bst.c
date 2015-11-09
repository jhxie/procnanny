/*
 *Note: this implementation is based on the concepts and code examples from
 *http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_bst1.aspx
 *the code is modified heavily to suit the need of procnanny.
 */
#include <stdbool.h>
#include <stddef.h>

#include "bst.h"
#include "pwwrapper.h"

#include "memwatch.h"

enum bst_chain_dir { BST_LEFT_CHAIN, BST_RIGHT_CHAIN, BST_CHAINSIZE };

struct bst_node_ {
        long key;
        void *memblk;
        /*
         *Unlike the implementation of castack, here I use 2 separate fields
         *to store the size of the memory block.
         */
        size_t blknum;  /* number of blocks */ 
        size_t blksize; /* size of each block */ 
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

void *bst_find(struct bst *current_bst, long key)
{
        /*
         *avoid undefined behavior by returning directly
         *rather than trying to dereference a NULL pointer
         */
        if (NULL == current_bst)
                return NULL;

        enum bst_chain_dir dir    = BST_LEFT_CHAIN;
        struct bst_node_ *tmp_ptr = current_bst->root;

        while (NULL != tmp_ptr) {
                if (key == tmp_ptr->key) {
                        break;
                } else {
                        dir     = key > tmp_ptr->key;
                        tmp_ptr = tmp_ptr->linkchain[dir];
                }
        }

        return (NULL == tmp_ptr) ? NULL : tmp_ptr->memblk;
}

void *bst_add(struct bst *current_bst, long key, size_t blknum, size_t blksize)
{
        if (NULL == current_bst)
                return NULL;

        struct bst_node_ *bstnode = calloc_or_die(1, sizeof(struct bst_node_));

        current_bst->numnode++;
        bstnode->key                        = key;
        bstnode->memblk                     = calloc_or_die(blknum, blksize);
        bstnode->blknum                     = blknum;
        bstnode->blksize                    = blksize;
        /*again, redundant*/
        bstnode->linkchain[BST_LEFT_CHAIN]  = NULL;
        bstnode->linkchain[BST_RIGHT_CHAIN] = NULL;
        
        /*
         *start to insert the new node into the bst
         */
        if (NULL == current_bst->root) {
                current_bst->root = bstnode;
        } else {
                enum bst_chain_dir dir    = BST_LEFT_CHAIN;
                struct bst_node_ *tmp_ptr = current_bst->root;

                while (true) {
                        /*note that duplicate keys are NOT allowed*/
                        if (bstnode->key == tmp_ptr->key)
                                return NULL;

                        dir = bstnode->key > tmp_ptr->key;

                        if (NULL == tmp_ptr->linkchain[dir])
                                break;

                        tmp_ptr = tmp_ptr->linkchain[dir];
                }
                tmp_ptr->linkchain[dir] = bstnode;
        }

        return bstnode->memblk;
}

