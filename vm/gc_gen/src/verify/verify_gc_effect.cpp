#include "verifier_common.h"
#include "verify_gc_effect.h"

void verifier_init_GC_verifier(Heap_Verifier* heap_verifier)
{
  GC_Verifier* gc_verifier = (GC_Verifier*)STD_MALLOC(sizeof(GC_Verifier));
  assert(gc_verifier);
  memset(gc_verifier, 0, sizeof(GC_Verifier));
  
  gc_verifier->trace_stack = gc_verifier->objects_set = gc_verifier->root_set = NULL;
  gc_verifier->is_tracing_resurrect_obj = FALSE;
  gc_verifier->num_live_objects_after_gc = gc_verifier->num_live_objects_before_gc = 0;
  gc_verifier->size_live_objects_after_gc = gc_verifier->size_live_objects_before_gc = 0;
  gc_verifier->num_resurrect_objects_after_gc = gc_verifier->num_resurrect_objects_before_gc = 0;
  gc_verifier->size_resurrect_objects_after_gc = gc_verifier->size_resurrect_objects_before_gc = 0;
  heap_verifier->gc_verifier = gc_verifier;
}
void verifier_destruct_GC_verifier(Heap_Verifier* heap_verifier)
{
  assert(!heap_verifier->gc_verifier ->trace_stack);
  assert(!heap_verifier->gc_verifier ->objects_set );
  assert(!heap_verifier->gc_verifier ->root_set);
  STD_FREE(heap_verifier->gc_verifier );
  heap_verifier->gc_verifier  = NULL;
}


void verifier_clear_objsets(Heap_Verifier* heap_verifier)
{
  Heap_Verifier_Metadata* verifier_metadata = heap_verifier->heap_verifier_metadata;
  verifier_clear_pool(verifier_metadata->objects_pool_before_gc, verifier_metadata->free_objects_pool, FALSE);
  verifier_clear_pool(verifier_metadata->objects_pool_after_gc, verifier_metadata->free_objects_pool, FALSE);
#ifndef BUILD_IN_REFERENT
  verifier_clear_pool(verifier_metadata->resurrect_objects_pool_before_gc, verifier_metadata->free_objects_pool, FALSE);
  verifier_clear_pool(verifier_metadata->resurrect_objects_pool_after_gc, verifier_metadata->free_objects_pool, FALSE);
#endif
}

void verify_gc_reset(Heap_Verifier* heap_verifier)
{
  GC_Verifier* gc_verifier = heap_verifier->gc_verifier;
  
  gc_verifier->trace_stack = gc_verifier->objects_set = gc_verifier->root_set = NULL;
  gc_verifier->is_tracing_resurrect_obj = FALSE;
  gc_verifier->num_live_objects_after_gc = gc_verifier->num_live_objects_before_gc = 0;
  gc_verifier->size_live_objects_after_gc = gc_verifier->size_live_objects_before_gc = 0;
#ifndef BUILD_IN_REFERENT
  gc_verifier->num_resurrect_objects_after_gc = gc_verifier->num_resurrect_objects_before_gc = 0;
  gc_verifier->size_resurrect_objects_after_gc = gc_verifier->size_resurrect_objects_before_gc = 0;
#endif

  verifier_clear_rootsets(heap_verifier);
  verifier_clear_objsets(heap_verifier);
}

void verify_live_finalizable_obj(Heap_Verifier* heap_verifier, Pool* live_finalizable_objs_pool)
{
  pool_iterator_init(live_finalizable_objs_pool);
  Vector_Block* live_fin_objs = pool_iterator_next(live_finalizable_objs_pool);
  while(live_fin_objs){
    POINTER_SIZE_INT * iter = vector_block_iterator_init(live_fin_objs);
    while(!vector_block_iterator_end(live_fin_objs, iter)){
      Partial_Reveal_Object* p_fin_obj = read_slot((REF*)iter);
      iter = vector_block_iterator_advance(live_fin_objs, iter);
      if(p_fin_obj==NULL) continue;
      assert(obj_is_marked_in_vt(p_fin_obj));
      if(!obj_is_marked_in_vt(p_fin_obj)){
        printf("ERROR\n");
      }
    }
    live_fin_objs = pool_iterator_next(live_finalizable_objs_pool);
  }
}

void* verifier_copy_obj_information(Partial_Reveal_Object* p_obj)
{
  Live_Object_Inform* p_obj_information = (Live_Object_Inform* )STD_MALLOC(sizeof(Live_Object_Inform));
  assert(p_obj_information);
  p_obj_information->vt_raw = obj_get_vt_raw(p_obj);
  p_obj_information->address = p_obj;
  return (void*) p_obj_information;
}

static Boolean fspace_object_was_forwarded(Partial_Reveal_Object *p_obj, Fspace *fspace, Heap_Verifier* heap_verifier)
{
  GC_Verifier* gc_verifier = heap_verifier->gc_verifier;
  assert(obj_belongs_to_space(p_obj, (Space*)fspace));
  unsigned int forwarded_first_part;
  if(!gc_verifier->gc_collect_kind == MINOR_COLLECTION  || !NOS_PARTIAL_FORWARD || heap_verifier->gc_is_gen_mode)
    forwarded_first_part = true;
  else
    forwarded_first_part = forward_first_half^1;
  /* forward_first_half is flipped after the collection, so the condition is reversed as well */
  return forwarded_first_part? (p_obj < object_forwarding_boundary):(p_obj >= object_forwarding_boundary);
}

void verifier_update_info_before_resurrect(Heap_Verifier* heap_verifier)
{
  if(!heap_verifier->need_verify_gc) return;
  GC_Verifier* gc_verifier = heap_verifier->gc_verifier;
  Heap_Verifier_Metadata* verifier_metadata = heap_verifier->heap_verifier_metadata;

  if(heap_verifier->is_before_gc){
    pool_put_entry(verifier_metadata->objects_pool_before_gc, gc_verifier->objects_set);
    gc_verifier->objects_set = verifier_free_set_pool_get_entry(verifier_metadata->free_objects_pool);
    assert(gc_verifier->objects_set);
    return;
  }else{
    pool_put_entry(verifier_metadata->objects_pool_after_gc, gc_verifier->objects_set);
    gc_verifier->objects_set = verifier_free_set_pool_get_entry(verifier_metadata->free_objects_pool);
    assert(gc_verifier->objects_set);
    return;
  }

}

void verifier_update_info_after_resurrect(Heap_Verifier* heap_verifier)
{
  if(!heap_verifier->need_verify_gc) return;
  GC_Verifier* gc_verifier = heap_verifier->gc_verifier;
  Heap_Verifier_Metadata* verifier_metadata = heap_verifier->heap_verifier_metadata;

  if(heap_verifier->is_before_gc){
    pool_put_entry(verifier_metadata->resurrect_objects_pool_before_gc, gc_verifier->objects_set);
    gc_verifier->objects_set = NULL;
    assert(!gc_verifier->objects_set);
    return;
  }else{
    pool_put_entry(verifier_metadata->resurrect_objects_pool_after_gc, gc_verifier->objects_set);
    gc_verifier->objects_set = NULL;
    assert(!gc_verifier->objects_set);
    return;
  }

}


void verifier_update_verify_info(Partial_Reveal_Object* p_obj, Heap_Verifier* heap_verifier)
{
  if(!heap_verifier->need_verify_gc) return;
  Heap_Verifier_Metadata* verifier_metadata = heap_verifier->heap_verifier_metadata;
  GC_Verifier* gc_verifier = heap_verifier->gc_verifier;

  GC_Gen* gc = (GC_Gen*)heap_verifier->gc;
  Space* mspace = gc_get_mos(gc);
  Space* nspace = gc_get_nos(gc);
  Space* lspace  = gc_get_los(gc);

  if(!gc_verifier->is_before_fallback_collection && gc_verifier->gc_collect_kind == MINOR_COLLECTION){
    if(!heap_verifier->is_before_gc){
      assert(!obj_belongs_to_space(p_obj, nspace) ||!fspace_object_was_forwarded(p_obj, (Fspace*)nspace, heap_verifier));
      if(obj_belongs_to_space(p_obj, nspace) && fspace_object_was_forwarded(p_obj, (Fspace*)nspace, heap_verifier)){
        gc_verifier->is_verification_passed = FALSE;
      }
    }
  }else if(!gc_verifier->is_before_fallback_collection){
    if(!heap_verifier->is_before_gc){
      assert(!obj_belongs_to_space(p_obj, nspace));
      if(obj_belongs_to_space(p_obj, nspace)){
        gc_verifier->is_verification_passed = FALSE;
      }
    }
  }
   /*store the object information*/
  void* p_obj_information =  verifier_copy_obj_information(p_obj);
   
#ifndef BUILD_IN_REFERENT
  if(!gc_verifier->is_tracing_resurrect_obj){
#endif    
    /*size and number*/
    if(heap_verifier->is_before_gc){
      verifier_set_push(p_obj_information, gc_verifier->objects_set, verifier_metadata->objects_pool_before_gc);
      gc_verifier->num_live_objects_before_gc ++;
      gc_verifier->size_live_objects_before_gc += vm_object_size(p_obj);
    }else{
      verifier_set_push(p_obj_information, gc_verifier->objects_set, verifier_metadata->objects_pool_after_gc);
      gc_verifier->num_live_objects_after_gc ++;
      gc_verifier->size_live_objects_after_gc += vm_object_size(p_obj);
    }
    return;
    
#ifndef BUILD_IN_REFERENT    
  }else{
    
    if(heap_verifier->is_before_gc){
      verifier_set_push(p_obj_information, gc_verifier->objects_set, verifier_metadata->resurrect_objects_pool_before_gc);
      gc_verifier->num_resurrect_objects_before_gc ++;
      gc_verifier->size_resurrect_objects_before_gc += vm_object_size(p_obj);
    }else{
      verifier_set_push(p_obj_information, gc_verifier->objects_set, verifier_metadata->resurrect_objects_pool_after_gc);
      gc_verifier->num_resurrect_objects_after_gc ++;
      gc_verifier->size_resurrect_objects_after_gc += vm_object_size(p_obj);
    }
    return;
    
  }
#endif

}

Boolean compare_live_obj_inform(POINTER_SIZE_INT* obj_container1,POINTER_SIZE_INT* obj_container2)
{
  Live_Object_Inform* obj_inform_1 = (Live_Object_Inform*)*obj_container1;
  Live_Object_Inform* obj_inform_2 = (Live_Object_Inform*)*obj_container2;
  if(((POINTER_SIZE_INT)obj_inform_1->vt_raw) == ((POINTER_SIZE_INT)obj_inform_2->vt_raw)){
    /*FIXME: erase live object information in compare_function. */
    STD_FREE(obj_inform_1);
    STD_FREE(obj_inform_2);
    return TRUE;
  }else{ 
    STD_FREE(obj_inform_1);
    STD_FREE(obj_inform_2);
    return FALSE;
  }
}


void verify_gc_effect(Heap_Verifier* heap_verifier)
{
  GC_Verifier* gc_verifier = heap_verifier->gc_verifier;
  
  if(gc_verifier->num_live_objects_before_gc != gc_verifier->num_live_objects_after_gc){
    gc_verifier->is_verification_passed = FALSE;
    printf("ERROR");
  }
  
  if(gc_verifier->size_live_objects_before_gc != gc_verifier->size_live_objects_after_gc){
    printf("ERROR");    
    gc_verifier->is_verification_passed = FALSE;
  }
  
#ifndef BUILD_IN_REFERENT  
  if(gc_verifier->num_resurrect_objects_before_gc != gc_verifier->num_resurrect_objects_after_gc){
    printf("ERROR");
    gc_verifier->is_verification_passed = FALSE;
  }
  
  if(gc_verifier->size_resurrect_objects_before_gc != gc_verifier->size_resurrect_objects_after_gc){
    printf("ERROR");    
    gc_verifier->is_verification_passed = FALSE;
  }
#endif
  
  Heap_Verifier_Metadata* verifier_metadata = heap_verifier->heap_verifier_metadata;
  Pool* free_pool = verifier_metadata->free_objects_pool;

  Object_Comparator object_comparator = compare_live_obj_inform;
  Boolean passed = verifier_compare_objs_pools(verifier_metadata->objects_pool_before_gc, 
                    verifier_metadata->objects_pool_after_gc , free_pool, object_comparator);
  if(!passed)     gc_verifier->is_verification_passed = FALSE;
#ifndef BUILD_IN_REFERENT
  passed = verifier_compare_objs_pools(verifier_metadata->resurrect_objects_pool_before_gc, 
                    verifier_metadata->resurrect_objects_pool_after_gc , free_pool, object_comparator);
  if(!passed)     gc_verifier->is_verification_passed = FALSE;
#endif
}


void verifier_pool_clear_objs_mark_bit(Pool* marked_objs_pool)
{
  pool_iterator_init(marked_objs_pool);
  Vector_Block* objs_set = pool_iterator_next(marked_objs_pool);
  
  while(objs_set){
    POINTER_SIZE_INT* iter = vector_block_iterator_init(objs_set);
    while(!vector_block_iterator_end(objs_set,iter)){
      Live_Object_Inform* p_verify_obj = (Live_Object_Inform* )*iter;
      iter = vector_block_iterator_advance(objs_set,iter);

      Partial_Reveal_Object* p_obj = p_verify_obj->address;
      assert(p_obj != NULL); 
      assert(obj_is_marked_in_vt(p_obj));
      obj_unmark_in_vt(p_obj);
    } 
    objs_set = pool_iterator_next(marked_objs_pool);
  }
}

void verifier_clear_objs_mark_bit(Heap_Verifier* heap_verifier)
{
  Pool* marked_objs_pool = NULL;
  
  Heap_Verifier_Metadata* verifier_metadata = heap_verifier->heap_verifier_metadata;
  if(heap_verifier->is_before_gc) {
    verifier_pool_clear_objs_mark_bit(verifier_metadata->objects_pool_before_gc);
  #ifndef BUILD_IN_REFERENT
    verifier_pool_clear_objs_mark_bit(verifier_metadata->resurrect_objects_pool_before_gc);
  #endif
  }else{
    verifier_pool_clear_objs_mark_bit(verifier_metadata->objects_pool_after_gc);
  #ifndef BUILD_IN_REFERENT
    verifier_pool_clear_objs_mark_bit(verifier_metadata->resurrect_objects_pool_after_gc);
  #endif
  }
  return;
}


void verifier_reset_gc_verification(Heap_Verifier* heap_verifier)
{
  if(!heap_verifier->need_verify_gc) return;
  heap_verifier->gc_verifier->is_verification_passed = TRUE;
  verifier_copy_rootsets(heap_verifier->gc, heap_verifier);
}
void verifier_clear_gc_verification(Heap_Verifier* heap_verifier)
{
  verify_gc_reset(heap_verifier);  
  verifier_set_fallback_collection(heap_verifier->gc_verifier, FALSE);  
}



