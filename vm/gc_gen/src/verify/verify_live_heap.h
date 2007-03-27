#ifndef _VERIFY_LIVE_HEAP_H_
#define _VERIFY_LIVE_HEAP_H_

#include "../common/gc_common.h"

extern Boolean verify_live_heap;

void gc_init_heap_verification(GC* gc);
void gc_terminate_heap_verification(GC* gc);
void gc_verify_heap(GC* gc, Boolean is_before_gc);


void event_mutator_allocate_newobj(Partial_Reveal_Object* p_newobj, POINTER_SIZE_INT size, VT vt_raw);
void event_gc_collect_kind_changed(GC* gc);
#endif
