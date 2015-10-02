#if !defined (CSUTILS_H_) && defined (CS_ONLY)
#define CSUTILS_H_

static void *calloc_or_die_(size_t nmemb, size_t size);
static void *realloc_or_die_(void *const ptr, size_t size);
static void castack_pushnode_(struct castack *current_castack);

#endif
