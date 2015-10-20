#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "castack.h"
#include "pwwrapper.h"

#define CS_ONLY
#include "csutils.h"
#undef CS_ONLY

#include "memwatch.h"

/*
 *Since this structure is never exposed to the outside world other than
 *this translation unit, the naming convention for the data member of
 *node_ remains normal: no underline postfix.
 *I use underline postfix exclusively for data fields that are "private"
 *in nature: their data fields should never be changed by any functions
 *other than the ones supplied here.
 *A primitive application of OO ideology.
 */

 /*these below may be put into the header*/
struct castack {
    struct node_ *head;
    size_t numblk;
};
 /*these above may be put into the header*/

struct node_ {
    void *memblk;
    size_t blksize;
    struct node_ *next;
};

/*
 *The conventions used here is "head pointer, null tail"
 *Concept coming from Robert Sedgewick
 */
struct castack *castack_init(void)
{
        struct castack *new_castack = calloc_or_die_(1, sizeof(struct castack));
        new_castack->numblk = 0;
        new_castack->head = NULL;

        return new_castack;
}

void *castack_push(struct castack *current_castack, size_t nmemb, size_t size)
{
        castack_pushnode_(current_castack);
        /*lastly allocate space for the memblk field of new node*/
        current_castack->head->blksize = nmemb * size;

        return current_castack->head->memblk = calloc_or_die_(nmemb, size);
}

void *castack_realloc(struct castack *current_castack, void *mem, size_t size)
{
        /*
         *if the passed in memory block does not even exist on the heap:
         *according to the manual page of realloc(),
         *"If ptr is NULL, then the call is equivalent to malloc(size),
         *for all values of size"
         */
        if (mem == NULL) {
                castack_pushnode_(current_castack);
                current_castack->head->blksize = size;
                return current_castack->head->memblk = calloc_or_die_(1, size);
        }

        /*try find the memory block on the castack*/
        struct node_ *tmp_ptr = current_castack->head;

        while (tmp_ptr != NULL) {
                if (tmp_ptr->memblk == mem)
                        break;
                tmp_ptr = tmp_ptr->next;
        }

        /*if the address passed does not happen to be on the castack*/
        if (tmp_ptr == NULL) {
                return NULL;
        } else if (size == 0) {
                /*
                 *"if size is equal to zero, and ptr is not NULL, then the call
                 *is equivalent to free(ptr)"
                 */
                free(tmp_ptr->memblk);
                tmp_ptr->memblk = NULL;
        } else {
                /*
                 *if the old block size is greater than the new block size,
                 *we do not need to worry about anything else
                 */
                tmp_ptr->memblk = realloc_or_die_(tmp_ptr->memblk, size);
                if (tmp_ptr->blksize < size) {
                        memset(tmp_ptr->memblk + tmp_ptr->blksize,
                               0,
                               size - tmp_ptr->blksize);
                }
        }
        tmp_ptr->blksize = size;

        return tmp_ptr->memblk;
}

void castack_pop(struct castack *current_castack)
{
        struct node_ *tmp_ptr = NULL;

        if (current_castack->head != NULL) {
                /*free the memory block pointed by individual node*/
                free(current_castack->head->memblk);
                tmp_ptr = current_castack->head->next;
                /*then free the node itself*/
                free(current_castack->head);
                current_castack->head = tmp_ptr;
                current_castack->numblk--;
        }
}

bool castack_empty(struct castack *current_castack)
{
        return current_castack->head == NULL;
}

size_t castack_report(struct castack *current_castack)
{
        return current_castack->numblk;
}

void castack_free(struct castack *current_castack)
{
        struct node_ *tmp_ptr = NULL;

        /*similar to pop but with a loop*/
        while (current_castack->head != NULL) {
                /*free the memory block pointed by individual node*/
                free(current_castack->head->memblk);
                tmp_ptr = current_castack->head->next;
                /*then free the node itself*/
                free(current_castack->head);
                current_castack->head = tmp_ptr;
                current_castack->numblk--;
        }
}

void castack_destroy(struct castack **current_castack)
{
        castack_free(*current_castack);

        /*free the castack itself*/
        free(*current_castack);
        *current_castack = NULL;
}

/*All the functions that have internal linkage are listed below.*/

static void castack_pushnode_(struct castack *current_castack)
{
        /*first allocate space for node*/
        struct node_ *memblk_ptr = calloc_or_die_(1, sizeof(struct node_));

        /*then link the node*/
        if (current_castack->head != NULL) {
                 /*
                  *let the next field of new node point to
                  *previously allocated node
                  */
                memblk_ptr->next = current_castack->head;
        } else {
                memblk_ptr->next = NULL;
        }

        /*let the head node pointer point to the new node*/
        current_castack->head = memblk_ptr;
        current_castack->head->memblk = NULL;
        current_castack->head->blksize = 0;
        current_castack->numblk++;
}

/*All the functions that have internal linkage are listed above.*/
