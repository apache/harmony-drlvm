#include "verifier_common.h"
#include "verify_gc_effect.h"
#include "verify_mutator_effect.h"
Boolean verifier_compare_objs_pools(Pool* objs_pool_before_gc, Pool* objs_pool_after_gc, Pool* free_pool ,Object_Comparator object_comparator)
{
  Vector_Block* objs_set_before_gc = pool_get_entry(objs_pool_before_gc);
  Vector_Block* objs_set_after_gc = pool_get_entry(objs_pool_after_gc);
  while(objs_set_before_gc && objs_set_after_gc){
    POINTER_SIZE_INT* iter_1 = vector_block_iterator_init(objs_set_before_gc);
    POINTER_SIZE_INT* iter_2 = vector_block_iterator_init(objs_set_after_gc);
    while(!vector_block_iterator_end(objs_set_before_gc, iter_1) 
                && !vector_block_iterator_end(objs_set_after_gc, iter_2) ){
      if(!(*object_comparator)(iter_1, iter_2)){
        assert(0);
        printf("ERROR\n");
        return FALSE;
      }
      iter_1 = vector_block_iterator_advance(objs_set_before_gc, iter_1);
      iter_2 = vector_block_iterator_advance(objs_set_after_gc, iter_2);
    }
    if(!vector_block_iterator_end(objs_set_before_gc, iter_1) 
                || !vector_block_iterator_end(objs_set_after_gc, iter_2) )    
      return FALSE;
 
    vector_block_clear(objs_set_before_gc);
    vector_block_clear(objs_set_after_gc);
    pool_put_entry(free_pool, objs_set_before_gc);
    pool_put_entry(free_pool, objs_set_after_gc);
    objs_set_before_gc = pool_get_entry(objs_pool_before_gc);
    objs_set_after_gc = pool_get_entry(objs_pool_after_gc);
  }
  if(pool_is_empty(objs_pool_before_gc)&&pool_is_empty(objs_pool_before_gc))
    return TRUE;
  else 
    return FALSE;
}

Boolean verifier_copy_rootsets(GC* gc, Heap_Verifier* heap_verifier)
{
  Heap_Verifier_Metadata* verifier_metadata = heap_verifier->heap_verifier_metadata;
  GC_Verifier* gc_verifier = heap_verifier->gc_verifier;
  gc_verifier->root_set = verifier_free_set_pool_get_entry(verifier_metadata->free_set_pool);
  
  GC_Metadata* gc_metadata = gc->metadata;
  pool_iterator_init(gc_metadata->gc_rootset_pool);
  Vector_Block* root_set = pool_iterator_next(gc_metadata->gc_rootset_pool);
  
  while(root_set){    
    POINTER_SIZE_INT* iter = vector_block_iterator_init(root_set);
    while(!vector_block_iterator_end(root_set,iter)){
      REF* p_ref = (REF* )*iter;
      iter = vector_block_iterator_advance(root_set,iter);
      if(*p_ref == COMPRESSED_NULL) continue;
      verifier_rootset_push(p_ref,gc_verifier->root_set);
    } 
    root_set = pool_iterator_next(gc_metadata->gc_rootset_pool);
  }  
  pool_put_entry(verifier_metadata->root_set_pool, gc_verifier->root_set);
  
  gc_verifier->root_set = NULL;
  return TRUE;
}

Boolean verify_rootset_slot(REF* p_ref, Heap_Verifier* heap_verifier)
{
  GC_Gen* gc    = (GC_Gen*)heap_verifier->gc;
  Space* mspace = gc_get_mos(gc);
  Space* lspace  = gc_get_los(gc);

  Partial_Reveal_Object* p_obj = read_slot(p_ref);
  if(p_obj == NULL){
    if(gc->collect_kind !=MINOR_COLLECTION ||(!heap_verifier->gc_is_gen_mode && !NOS_PARTIAL_FORWARD)){
      assert(0);
      return FALSE;
    }else{
      return TRUE;
    }
  }
  if(!heap_verifier->gc_is_gen_mode){
    assert(!address_belongs_to_gc_heap(p_ref, heap_verifier->gc));
    if(address_belongs_to_gc_heap(p_ref, heap_verifier->gc)){
      printf("ERROR\n");
      return FALSE;
    }
  }
  assert(address_belongs_to_gc_heap(p_obj,heap_verifier->gc));
  if(heap_verifier->is_before_gc){
    //if(!address_belongs_to_gc_heap(p_ref) && address_belongs_to_gc_heap(p_obj)){
    if(!address_belongs_to_gc_heap(p_obj, heap_verifier->gc)){
      printf("error!\n");
      return FALSE;
    }
  }else{
    if(heap_verifier->gc_verifier->is_before_fallback_collection){
      if(!address_belongs_to_gc_heap(p_obj, heap_verifier->gc)){
        printf("error!\n");
        assert(0);
        return FALSE;
      }
      return TRUE;
    }
    assert(address_belongs_to_space(p_obj, mspace) || address_belongs_to_space(p_obj, lspace));
    if(!address_belongs_to_space(p_obj, mspace) && !address_belongs_to_space(p_obj, lspace)){
      printf("Error\n");
      return FALSE;
   }
  }
  return TRUE;
}


Boolean verifier_parse_options(Heap_Verifier* heap_verifier, char* options)
{
  char* verifier_options = options;
  char* option = NULL;
  for (option = strtok(verifier_options,","); option; option = strtok(NULL,",")) {
    string_to_upper(option);
    if(!strcmp(option, "ROOTSET")) heap_verifier->need_verify_rootset = TRUE;
    else if (!strcmp(option, "WRITEBARRIER")) heap_verifier->need_verify_writebarrier = TRUE;
    else if (!strcmp(option, "ALLOCATION")) heap_verifier->need_verify_allocation= TRUE;
    else if (!strcmp(option, "GC")) heap_verifier->need_verify_gc= TRUE;
    else if(!strcmp(option, "DEFAULT")){
      heap_verifier->need_verify_rootset = TRUE;
      heap_verifier->need_verify_writebarrier = TRUE;
      heap_verifier->need_verify_gc= TRUE;
    }else if(!strcmp(option, "ALL")){
      heap_verifier->need_verify_rootset = TRUE;
      heap_verifier->need_verify_writebarrier = TRUE;
      heap_verifier->need_verify_allocation= TRUE;
      heap_verifier->need_verify_gc= TRUE;
    }else{
      printf("Parse verify option error.\n");
      printf("Usage: -XDgc.verify=rooset,writebarrier,allocation,gc \n");
      printf("Usage: -XDgc.verify=default \n");
      printf("Usage: -XDgc.verify=all \n");
      return FALSE;
    }
  }
  return TRUE;
}


void verifier_log_before_gc(Heap_Verifier* heap_verifier)
{
  Allocation_Verifier* alloc_verifier = heap_verifier->allocation_verifier;
  WriteBarrier_Verifier* wb_verifier = heap_verifier->writebarrier_verifier;
  RootSet_Verifier* rootset_verifier = heap_verifier->rootset_verifier;

  printf("before gc:\n");

  if(heap_verifier->need_verify_allocation){
    printf(" Allocation Verify: %s , ", alloc_verifier->is_verification_passed?"passed":"failed");
    printf(" new nos: %d : %d , ", alloc_verifier->num_nos_newobjs, alloc_verifier->num_nos_objs);
    printf(" new los: %d : %d \n", alloc_verifier->num_los_newobjs, 
          alloc_verifier->num_los_objs-alloc_verifier->last_num_los_objs);
  }

  if(heap_verifier->need_verify_rootset){
    printf(" RootSet Verify: %s , ", rootset_verifier->is_verification_passed?"passed":"failed");
    printf(" num: %d  , ", rootset_verifier->num_slots_in_rootset);
    printf(" error num: %d \n", rootset_verifier->num_error_slots);

  }

  if(heap_verifier->need_verify_writebarrier){
    printf(" WriteBarrier Verify: %s , ", wb_verifier->is_verification_passed?"passed":"failed");
    printf(" num cached: %d  , ", wb_verifier->num_ref_wb_in_remset);
    printf(" num real : %d \n", wb_verifier->num_ref_wb_after_scanning);
  }
  printf("===============================================\n");

}

void verifier_log_start()
{
  printf("\n===============================================\n");
}

void verifier_log_after_gc(Heap_Verifier* heap_verifier)
{
  GC_Verifier* gc_verifier = heap_verifier->gc_verifier;
  printf("after gc:\n");
  if(heap_verifier->need_verify_gc){
    printf(" GC Verify: %s \n", gc_verifier->is_verification_passed?"passed":"failed");
    printf(" live obj  :   NUM   before %d ,  after %d \n", gc_verifier->num_live_objects_before_gc, gc_verifier->num_live_objects_after_gc);
    printf(" live obj  :   SIZE   before %d MB,  after %d MB \n", gc_verifier->size_live_objects_before_gc>>20, gc_verifier->size_live_objects_after_gc>>20);
    printf(" resurrect obj:  NUM   before %d      , after %d \n", gc_verifier->num_resurrect_objects_before_gc, gc_verifier->num_resurrect_objects_after_gc);
    printf(" resurrect obj : SIZE   before %d MB,  after %d MB\n", gc_verifier->size_resurrect_objects_before_gc>>20, gc_verifier->size_resurrect_objects_after_gc>>20);
  }
  printf("===============================================\n");

}

