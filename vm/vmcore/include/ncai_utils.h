/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */
#ifndef _NCAI_UTILS_H_
#define _NCAI_UTILS_H_

#include "port_malloc.h"


#ifdef __cplusplus
extern "C" {
#endif


static inline void* ncai_alloc(size_t size)
{
    return STD_MALLOC(size);
}

static inline void* ncai_realloc(void* mem)
{
    return mem;
}

static inline void ncai_free(void* mem)
{
    STD_FREE(mem);
}



#ifdef __cplusplus
}
#endif

#endif // _NCAI_UTILS_H_
