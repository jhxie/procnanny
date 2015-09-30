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
                                 size_t nmemb,
                                 size_t size)
                                __attribute__((warn_unused_result));

/*
 *Traverse through the castack and reallocate the memory pointed to by mem.
 *
 *On failure(the memory block pointed by mem does not exist on the castack,
 *or the realloc() call failed), exit the program with reasons presented.
 *
 *All the rest behaviors are the same as realloc(), with 2 exceptions:
 *If the 1st parameter is NULL, or the new size is larger than the old size,
 *the allocated memory will be initialized.
 *
 *Note this function takes O(n) to find the mem, but normally this is not
 *a big problem since the memory block you are going to realloc() is near
 *the top of the castack.
 */
void           *castack_realloc (struct castack *current_castack,
                                 void *mem,
                                 size_t size)
                                 __attribute__((warn_unused_result));

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
 *In addition to all the memory blocks, destroy the castack itself as well;
 *then set the current_castack to NULL.
 */
void            castack_destroy (struct castack **current_castack);

#endif
