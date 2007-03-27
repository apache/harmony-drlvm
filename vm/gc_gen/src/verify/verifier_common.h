#ifndef _VERIFIER_COMMON_H_
#define _VERIFIER_COMMON_H_
#include "verify_live_heap.h"
#include "verifier_metadata.h"
#include "../common/gc_common.h"
#include "../common/gc_space.h"
#include "../gen/gen.h"

struct Heap_Verifier;
struct Allocation_Verifier;
struct GC_Verifier;
struct WriteBarrier_Verifier;
struct RootSet_Verifier;

typedef void (*Object_Scanner)(struct Heap_Verifier*);

typedef struct Heap_Verifier{
  GC* gc;
  GC_Verifier* gc_verifier;
  WriteBarrier_Verifier* writebarrier_verifier;
  RootSet_Verifier* rootset_verifier;
  Allocation_Verifier* allocation_verifier;
  Heap_Verifier_Metadata* heap_verifier_metadata;

  Boolean is_before_gc;
  Boolean gc_is_gen_mode;
  Boolean need_verify_gc;
  Boolean need_verify_allocation;
  Boolean need_verify_rootset;
  Boolean need_verify_writebarrier;
  
  Object_Scanner all_obj_scanner;
  Object_Scanner live_obj_scanner;
} Heap_Verifier;



typedef Boolean (*Object_Comparator)(POINTER_SIZE_INT*, POINTER_SIZE_INT*);

extern Heap_Verifier* get_heap_verifier();

extern void verifier_metadata_initialize(Heap_Verifier* heap_verifier);
extern void verifier_init_object_scanner(Heap_Verifier* heap_verifier);

extern void verifier_scan_los_objects(Space* space, Heap_Verifier* heap_verifier);


Boolean verifier_copy_rootsets(GC* gc, Heap_Verifier* heap_verifier);
Boolean verifier_compare_objs_pools(Pool* objs_pool_before_gc, Pool* objs_pool_after_gc, Pool* free_pool ,Object_Comparator object_comparator);
Boolean verifier_parse_options(Heap_Verifier* heap_verifier, char* options);
void verifier_log_before_gc(Heap_Verifier* heap_verifier);
void verifier_log_after_gc(Heap_Verifier* heap_verifier);
void verifier_log_start();
Boolean verify_rootset_slot(REF* p_ref, Heap_Verifier*  heap_verifier);





inline void verifier_set_gen_mode(Heap_Verifier* heap_verifier)
{  heap_verifier->gc_is_gen_mode = gc_is_gen_mode();  }

inline Boolean need_verify_gc_effect(Heap_Verifier* heap_verifier)
{  return heap_verifier->need_verify_gc && !heap_verifier->is_before_gc; }

inline Boolean need_scan_live_objs(Heap_Verifier* heap_verifier)
{
  if(heap_verifier->need_verify_gc) return TRUE;
  else if(heap_verifier->need_verify_writebarrier && !heap_verifier->is_before_gc) return TRUE;
  else return FALSE;
}

inline Boolean need_verify_mutator_effect(Heap_Verifier* heap_verifier)
{
  if(!heap_verifier->is_before_gc) return FALSE;
  return heap_verifier->need_verify_allocation || heap_verifier->need_verify_rootset 
                || heap_verifier->need_verify_writebarrier;
}

inline Boolean need_scan_all_objs(Heap_Verifier* heap_verifier)
{
  if(!heap_verifier->is_before_gc) return FALSE;
  return heap_verifier->need_verify_allocation || heap_verifier->need_verify_writebarrier;

}

inline void verify_live_object_slot(REF* p_ref, Heap_Verifier* heap_verifier)
{
  assert(p_ref);
  assert(address_belongs_to_gc_heap(read_slot(p_ref), (GC*)heap_verifier->gc));
  Partial_Reveal_Object* UNUSED p_obj = read_slot(p_ref);
  assert(p_obj);
  assert(obj_get_vt(p_obj));
  assert(!address_belongs_to_gc_heap(uncompress_vt(obj_get_vt(p_obj)), (GC*)heap_verifier->gc));
}

inline void verify_all_object_slot(REF* p_ref, Heap_Verifier* heap_verifier)
{
  assert(p_ref);
  assert(address_belongs_to_gc_heap(read_slot(p_ref), (GC*)heap_verifier->gc));
}

inline void verify_object_header(Partial_Reveal_Object* p_obj, Heap_Verifier* heap_verifier)
{
  assert(p_obj);
  assert(address_belongs_to_gc_heap(p_obj, (GC*)heap_verifier->gc));

  assert(obj_get_vt(p_obj));
  assert(!address_belongs_to_gc_heap(uncompress_vt(obj_get_vt(p_obj)), (GC*)heap_verifier->gc));
}


inline void verifier_clear_rootsets(Heap_Verifier* heap_verifier)
{
  Heap_Verifier_Metadata* verifier_metadata = heap_verifier->heap_verifier_metadata;
  verifier_clear_pool(verifier_metadata->root_set_pool, verifier_metadata->free_set_pool, FALSE);
}

#define VERIFY_WB_MARK_BIT 0x01

inline void wb_mark_in_slot(REF* p_ref){
  REF ref = *p_ref;
  *p_ref = (REF)((POINTER_SIZE_INT)ref | VERIFY_WB_MARK_BIT);
}

inline void wb_unmark_in_slot(REF* p_ref){
  REF ref = *p_ref;
  *p_ref = (REF)((POINTER_SIZE_INT)ref & ~VERIFY_WB_MARK_BIT);
}

inline Boolean wb_is_marked_in_slot(REF* p_ref){
  REF ref = *p_ref;
  return (POINTER_SIZE_INT)ref & VERIFY_WB_MARK_BIT;
}

inline REF verifier_get_object_slot(REF* p_ref)
{  
  REF ref = *p_ref;   
  return (REF)((POINTER_SIZE_INT)ref | VERIFY_WB_MARK_BIT);
}


#define UNREACHABLE_OBJ_MARK_IN_VT 0x02

inline void tag_unreachable_obj(Partial_Reveal_Object* p_obj)
{
  Partial_Reveal_VTable* vt = uncompress_vt(obj_get_vt_raw(p_obj));
  obj_set_vt(p_obj, compress_vt((Partial_Reveal_VTable*)((POINTER_SIZE_INT)vt | UNREACHABLE_OBJ_MARK_IN_VT)));
}

inline Boolean is_unreachable_obj(Partial_Reveal_Object* p_obj)
{
  return ((POINTER_SIZE_INT)obj_get_vt_raw(p_obj) & UNREACHABLE_OBJ_MARK_IN_VT);
}

#endif 
