#ifndef INTERIOR_POINTER_H
#define INTERIOR_POINTER_H 

#include "gc_common.h"

void add_root_set_entry_interior_pointer(void **slot, int offset, Boolean is_pinned);
void update_rootset_interior_pointer();

#endif //INTERIOR_POINTER_H
