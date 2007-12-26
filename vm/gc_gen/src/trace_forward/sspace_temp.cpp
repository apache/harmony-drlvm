/* this file is temporarily existent for developing semi-space algorithm */
#include "../semi_space/sspace.h"

Sspace *sspace_initialize(GC* gc, void* start, POINTER_SIZE_INT sspace_size, POINTER_SIZE_INT commit_size){return null;}
void sspace_destruct(Sspace *sspace){ return;}

void* sspace_alloc(unsigned size, Allocator *allocator){return NULL;}
void* semispace_alloc(unsigned size, Allocator *allocator){return NULL;}

Boolean sspace_alloc_block(Sspace* sspace, Allocator* allocator){return FALSE;}

void sspace_collection(Sspace* sspace){return;}
void sspace_reset_after_collection(Sspace* sspace){return;}
void sspace_prepare_for_collection(Sspace* sspace){return;}
