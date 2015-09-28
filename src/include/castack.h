#ifndef CASTACK_H_
#define CASTACK_H_

#include <stddef.h>
#include <stdbool.h>

/*
 *                 Public Interface of castack(calloc based stack)
 */

/*
 *Initialize a calloc based stack.
 */
struct castack *castack_init    (void) __attribute__ ((warn_unused_result));

/*
 *The address returned by this function can be realloc()'d but can _never_
 *be called free() upon -- that is taken care of by the castack_free() and
 *castack_destroy().
 */
void           *castack_push    (struct castack *current_castack,
                                 size_t nmemb, size_t size)
                                __attribute__((warn_unused_result));
/*
 *Push an existing allocated heap memroy onto the castack.
 */
void            castack_pushaddr(struct castack *current_castack, void *mem);

/*
 *Pop the most recently allocated  memory block and free it:
 *may accidentally free memory that is not mean to be freed.
 */
void            castack_pop     (struct castack *current_castack);

/*
 *Report whether the castack is empty.
 */
bool            castack_empty   (struct castack *current_castack);

/*
 *Return number of memory blocks tracked by castack so far.
 */
size_t          castack_report  (struct castack *current_castack);

/*
 *Traverse through the castack and free all memory blocks tracked by it.
 */
void            castack_free    (struct castack *current_castack);

/*
 *In addition to all the memory blocks, destroy the castack itself as well.
 */
void            castack_destroy (struct castack *current_castack);

#endif
