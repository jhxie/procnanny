#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "castack.h"
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

// these 2 may be put into the header
typedef struct node_ *link;

struct castack {
    link   head;
    size_t numblk;
};
// these 2 may be put into the header

struct node_ {
    void *memblk;
    link next;
};

static inline void *calloc_or_die_(size_t nmemb, size_t size)
{
        void *rtn_ptr = calloc(nmemb, size);

        if (rtn_ptr == NULL) {
                perror("calloc()");
                exit(EXIT_FAILURE);
        } else {
                return rtn_ptr;
        }
}

/*
 *The convetions used here is "head pointer, null tail"
 *Concept coming from Robert Sedgewick
 */
struct castack *castack_init(void)
{
        struct castack *new_castack = calloc_or_die_(1, sizeof(struct castack));
        new_castack->numblk = 0;
        new_castack->head = NULL;

        return new_castack;
}

static void castack_pushnode_(struct castack *current_castack)
{
        // first allocate space for node
        link memblk_ptr = calloc_or_die_(1, sizeof(struct node_));

        // then link the node
        if (current_castack->head != NULL) {
                // let the next field of new node point to
                // previously allocated node
                memblk_ptr->next = current_castack->head;
        } else {
                memblk_ptr->next = NULL;
        }

        // let the head node pointer point to the new node
        current_castack->head = memblk_ptr;
        current_castack->head->memblk = NULL;
        current_castack->numblk++;
}

void *castack_push(struct castack *current_castack, size_t nmemb, size_t size)
{
        castack_pushnode_(current_castack);
        // lastly allocate space for the memblk field of new node
        current_castack->head->memblk = calloc_or_die_(nmemb, size);

        return current_castack->head->memblk;
}

void castack_pushaddr(struct castack *current_castack, void *mem)
{
        castack_pushnode_(current_castack);
        current_castack->head->memblk = mem;
}

void castack_pop(struct castack *current_castack)
{
        link tmp_ptr = NULL;

        if (current_castack->head != NULL) {
                // free the memory block pointed by individual node
                free(current_castack->head->memblk);
                tmp_ptr = current_castack->head->next;
                // then free the node itself
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
        link tmp_ptr = NULL;

        // similar to pop but with a loop
        while (current_castack->head != NULL) {
                // free the memory block pointed by individual node
                free(current_castack->head->memblk);
                tmp_ptr = current_castack->head->next;
                // then free the node itself
                free(current_castack->head);
                current_castack->head = tmp_ptr;
                current_castack->numblk--;
        }
}

void castack_destroy(struct castack *current_castack)
{
        castack_free(current_castack);

        // free the castack itself
        free(current_castack);
}
