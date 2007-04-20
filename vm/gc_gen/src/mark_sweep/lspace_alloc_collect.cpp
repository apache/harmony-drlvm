/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @author Ji Qi, 2006/10/05
 */

#include "lspace.h"
#include "../gen/gen.h"
#include "../common/space_tuner.h"

static void free_pool_lock_nr_list(Free_Area_Pool* pool, unsigned int list_index)
{
    Lockable_Bidir_List* list_head = &pool->sized_area_list[list_index];
    lock(list_head->lock);
}

static void free_pool_unlock_nr_list(Free_Area_Pool* pool, unsigned int list_index)
{
    Lockable_Bidir_List* list_head = &pool->sized_area_list[list_index];
    unlock(list_head->lock);
}

static unsigned int free_pool_nr_list_is_empty(Free_Area_Pool* pool, unsigned int list_index)
{
    Bidir_List* head = (Bidir_List*)(&pool->sized_area_list[list_index]);
    return (head->next == head);
}
static void* free_pool_former_lists_atomic_take_area_piece(Free_Area_Pool* pool, unsigned int list_hint, POINTER_SIZE_INT size)
{
    Free_Area* free_area;
    void* p_result;
    POINTER_SIZE_SINT remain_size;
    POINTER_SIZE_INT alloc_size = ALIGN_UP_TO_KILO(size);
    unsigned int new_list_nr = 0;
    Lockable_Bidir_List* head = &pool->sized_area_list[list_hint];

    assert(list_hint < MAX_LIST_INDEX);

    free_pool_lock_nr_list(pool, list_hint);
    /*Other LOS allocation may race with this one, so check list status here.*/
    if(free_pool_nr_list_is_empty(pool, list_hint)){
        free_pool_unlock_nr_list(pool, list_hint);
        return NULL;
    }

    free_area = (Free_Area*)(head->next);
    /*if the list head is not NULL, it definitely satisfies the request. */   
    remain_size = free_area->size - alloc_size;
    assert(remain_size >= 0);
    if( remain_size >= GC_OBJ_SIZE_THRESHOLD){
        new_list_nr = pool_list_index_with_size(remain_size);
        p_result = (void*)((POINTER_SIZE_INT)free_area + remain_size);
        if(new_list_nr == list_hint){
            free_area->size = remain_size;
            free_pool_unlock_nr_list(pool, list_hint);
            return p_result;
        }else{
            free_pool_remove_area(pool, free_area);
            free_pool_unlock_nr_list(pool, list_hint);
            free_area->size = remain_size;
            free_pool_lock_nr_list(pool, new_list_nr);
            free_pool_add_area(pool, free_area);
            free_pool_unlock_nr_list(pool, new_list_nr);
            return p_result;            
        }
    }
    else
    {
        free_pool_remove_area(pool, free_area);
        free_pool_unlock_nr_list(pool, list_hint);
        p_result = (void*)((POINTER_SIZE_INT)free_area + remain_size);
        if(remain_size > 0){
            assert((remain_size >= KB) && (remain_size < GC_OBJ_SIZE_THRESHOLD));
            free_area->size = remain_size;
        }
        return p_result;
    }
    assert(0);
    return NULL;
}

static void* free_pool_last_list_atomic_take_area_piece(Free_Area_Pool* pool, POINTER_SIZE_INT size)
{
    void* p_result;
    POINTER_SIZE_SINT remain_size = 0;
    POINTER_SIZE_INT alloc_size = ALIGN_UP_TO_KILO(size);
    Free_Area* free_area = NULL;
    Free_Area* new_area = NULL;
    unsigned int new_list_nr = 0;        
    Lockable_Bidir_List* head = &(pool->sized_area_list[MAX_LIST_INDEX]);
    
    free_pool_lock_nr_list(pool, MAX_LIST_INDEX );
    /*The last list is empty.*/
    if(free_pool_nr_list_is_empty(pool, MAX_LIST_INDEX)){
        free_pool_unlock_nr_list(pool, MAX_LIST_INDEX );                
        return NULL;
    }
    
    free_area = (Free_Area*)(head->next);
    while(  free_area != (Free_Area*)head ){
        remain_size = free_area->size - alloc_size;
        if( remain_size >= GC_OBJ_SIZE_THRESHOLD){
            new_list_nr = pool_list_index_with_size(remain_size);
            p_result = (void*)((POINTER_SIZE_INT)free_area + remain_size);
            if(new_list_nr == MAX_LIST_INDEX){
                free_area->size = remain_size;
                free_pool_unlock_nr_list(pool, MAX_LIST_INDEX);
                return p_result;
            }else{
                free_pool_remove_area(pool, free_area);
                free_pool_unlock_nr_list(pool, MAX_LIST_INDEX);
                free_area->size = remain_size;
                free_pool_lock_nr_list(pool, new_list_nr);
                free_pool_add_area(pool, free_area);
                free_pool_unlock_nr_list(pool, new_list_nr);
                return p_result;            
            }
        }
        else if(remain_size >= 0)
        {
            free_pool_remove_area(pool, free_area);
            free_pool_unlock_nr_list(pool, MAX_LIST_INDEX);
            p_result = (void*)((POINTER_SIZE_INT)free_area + remain_size);
            if(remain_size > 0){
                assert((remain_size >= KB) && (remain_size < GC_OBJ_SIZE_THRESHOLD));
                free_area->size = remain_size;
            }
            return p_result;
        }
        else free_area = (Free_Area*)free_area->next;
    }
    /*No adequate area in the last list*/
    free_pool_unlock_nr_list(pool, MAX_LIST_INDEX );
    return NULL;
}

void* lspace_try_alloc(Lspace* lspace, POINTER_SIZE_INT alloc_size){
  void* p_result = NULL;
  Free_Area_Pool* pool = lspace->free_pool;  
  unsigned int list_hint = pool_list_index_with_size(alloc_size);  
  list_hint = pool_list_get_next_flag(pool, list_hint);  

  while((!p_result) && (list_hint <= MAX_LIST_INDEX)){
      /*List hint is not the last list, so look for it in former lists.*/
      if(list_hint < MAX_LIST_INDEX){
          p_result = free_pool_former_lists_atomic_take_area_piece(pool, list_hint, alloc_size);
          if(p_result){
              memset(p_result, 0, alloc_size);
              POINTER_SIZE_INT vold = lspace->alloced_size;
              POINTER_SIZE_INT vnew = vold + alloc_size;
              while( vold != atomic_casptrsz(&lspace->alloced_size, vnew, vold) ){                      
                  vold = lspace->alloced_size;
                  vnew = vold + alloc_size;
              }
              return p_result;
          }else{
              list_hint ++;
              list_hint = pool_list_get_next_flag(pool, list_hint);
              continue;
          }
      }
      /*List hint is the last list, so look for it in the last list.*/
      else
      {
          p_result = free_pool_last_list_atomic_take_area_piece(pool, alloc_size);
          if(p_result){
              memset(p_result, 0, alloc_size);
              POINTER_SIZE_INT vold = lspace->alloced_size;
              POINTER_SIZE_INT vnew = vold + alloc_size;
              while( vold != atomic_casptrsz(&lspace->alloced_size, vnew, vold) ){                      
                  vold = lspace->alloced_size;
                  vnew = vold + alloc_size;
              }
              return p_result;
          }
          else break;
      }
  }
  return p_result;
}

void* lspace_alloc(POINTER_SIZE_INT size, Allocator *allocator)
{
    unsigned int try_count = 0;
    void* p_result = NULL;
    POINTER_SIZE_INT alloc_size = ALIGN_UP_TO_KILO(size);
    Lspace* lspace = (Lspace*)gc_get_los((GC_Gen*)allocator->gc);
    Free_Area_Pool* pool = lspace->free_pool;
    
    while( try_count < 2 ){
        if(p_result = lspace_try_alloc(lspace, alloc_size))
          return p_result;

        /*Failled, no adequate area found in all lists, so GC at first, then get another try.*/   
        if(try_count == 0){
            vm_gc_lock_enum();
            /*Check again if there is space for the obj, for maybe other mutator 
            threads issus a GC in the time gap of waiting the gc lock*/
            if(p_result = lspace_try_alloc(lspace, alloc_size)){
              vm_gc_unlock_enum();
              return p_result;            
            }
            lspace->failure_size = round_up_to_size(alloc_size, KB);

            gc_reclaim_heap(allocator->gc, GC_CAUSE_LOS_IS_FULL);

            if(lspace->success_ptr){
              p_result = lspace->success_ptr;
              lspace->success_ptr = NULL;
              vm_gc_unlock_enum();
              return p_result;
            }
            vm_gc_unlock_enum();
            try_count ++;
        }else{
            try_count ++;
        }
    }
    return NULL;
}

void lspace_compute_object_target(Collector* collector, Lspace* lspace)
{
  void* dest_addr = lspace->heap_start;
  unsigned int iterate_index = 0;
  Partial_Reveal_Object* p_obj = lspace_get_first_marked_object(lspace, &iterate_index);

  assert(!collector->rem_set);
  collector->rem_set = free_set_pool_get_entry(collector->gc->metadata);
  
  while( p_obj ){
    assert( obj_is_marked_in_vt(p_obj));
    unsigned int obj_size = vm_object_size(p_obj);
    assert(((POINTER_SIZE_INT)dest_addr + obj_size) <= (POINTER_SIZE_INT)lspace->heap_end);
    Obj_Info_Type obj_info = get_obj_info_raw(p_obj);
    if( obj_info != 0 ) {
      collector_remset_add_entry(collector, (Partial_Reveal_Object **)dest_addr);
      collector_remset_add_entry(collector, (Partial_Reveal_Object **)obj_info);
    }
      
    obj_set_fw_in_oi(p_obj, dest_addr);
    dest_addr = (void *)ALIGN_UP_TO_KILO(((POINTER_SIZE_INT) dest_addr + obj_size));
    p_obj = lspace_get_next_marked_object(lspace, &iterate_index);
  }

  pool_put_entry(collector->gc->metadata->collector_remset_pool, collector->rem_set);
  collector->rem_set = NULL;
  
  lspace->scompact_fa_start = dest_addr;
  lspace->scompact_fa_end= lspace->heap_end;
  return;
}

void lspace_sliding_compact(Collector* collector, Lspace* lspace)
{
  unsigned int iterate_index = 0;
  Partial_Reveal_Object* p_obj = lspace_get_first_marked_object(lspace, &iterate_index);

  while( p_obj ){
    assert( obj_is_marked_in_vt(p_obj));
    obj_unmark_in_vt(p_obj);
    
    unsigned int obj_size = vm_object_size(p_obj);
    Partial_Reveal_Object *p_target_obj = obj_get_fw_in_oi(p_obj);
    POINTER_SIZE_INT target_obj_end = (POINTER_SIZE_INT)p_target_obj + obj_size;
    if( p_obj != p_target_obj){
      memmove(p_target_obj, p_obj, obj_size);
      /*Fixme: For LOS_Shrink debug*/
//      unsigned int padding_lenth = ALIGN_UP_TO_KILO(target_obj_end) - target_obj_end;
//      memset(p_target_obj, 0, padding_lenth);
    }
    set_obj_info(p_target_obj, 0);
    p_obj = lspace_get_next_marked_object(lspace, &iterate_index);  
  }

  return;
}

void lspace_reset_after_collection(Lspace* lspace)
{
    GC* gc = lspace->gc;
    Space_Tuner* tuner = gc->tuner;
    POINTER_SIZE_INT trans_size = tuner->tuning_size;
    POINTER_SIZE_INT new_fa_size = 0;
    assert(!(trans_size%GC_BLOCK_SIZE_BYTES));
    
    /* Reset the pool first because its info is useless now. */
    free_area_pool_reset(lspace->free_pool);

    switch(tuner->kind){
      case TRANS_FROM_MOS_TO_LOS:{
        assert(!lspace->move_object);
        void* origin_end = lspace->heap_end;
        lspace->heap_end = (void*)(((GC_Gen*)gc)->mos->blocks);
        /*The assumption that the first word of one KB must be zero when iterating lspace in 
        that function lspace_get_next_marked_object is not true*/
        Free_Area* trans_fa = free_area_new(origin_end, trans_size);
        free_pool_add_area(lspace->free_pool, trans_fa);
        lspace->committed_heap_size += trans_size;
        lspace->reserved_heap_size += trans_size;
        if(lspace->move_object){
            Block* mos_first_block = ((GC_Gen*)gc)->mos->blocks;
            lspace->heap_end = (void*)mos_first_block;
            new_fa_size = (POINTER_SIZE_INT)lspace->scompact_fa_end - (POINTER_SIZE_INT)lspace->scompact_fa_start;
            Free_Area* fa = free_area_new(lspace->scompact_fa_start,  new_fa_size);
            if(new_fa_size >= GC_OBJ_SIZE_THRESHOLD) free_pool_add_area(lspace->free_pool, fa);
        }
        break;
      }
      case TRANS_FROM_LOS_TO_MOS:{
        assert(lspace->move_object);
        assert(tuner->tuning_size);
        Block* mos_first_block = ((GC_Gen*)gc)->mos->blocks;
        assert( (POINTER_SIZE_INT)lspace->heap_end - trans_size == (POINTER_SIZE_INT)mos_first_block );
        lspace->heap_end = (void*)mos_first_block;
        lspace->committed_heap_size -= trans_size;
        lspace->reserved_heap_size -= trans_size;
        /*LOS_Shrink: We don't have to scan lspace to build free pool when slide compact LOS*/
        assert((POINTER_SIZE_INT)lspace->scompact_fa_end > (POINTER_SIZE_INT)lspace->scompact_fa_start + tuner->tuning_size);
        new_fa_size = (POINTER_SIZE_INT)lspace->scompact_fa_end - (POINTER_SIZE_INT)lspace->scompact_fa_start - tuner->tuning_size;
        Free_Area* fa = free_area_new(lspace->scompact_fa_start,  new_fa_size);
        free_pool_add_area(lspace->free_pool, fa);
        break;
      }
      default:{
        if(lspace->move_object){
          assert(tuner->kind == TRANS_NOTHING);
          assert(!tuner->tuning_size);
          new_fa_size = (POINTER_SIZE_INT)lspace->scompact_fa_end - (POINTER_SIZE_INT)lspace->scompact_fa_start;
          Free_Area* fa = free_area_new(lspace->scompact_fa_start,  new_fa_size);
          free_pool_add_area(lspace->free_pool, fa);
        }
        break;
      }
    }

    /*For_statistic los information.*/
    lspace->alloced_size = 0;    
    lspace->surviving_size = 0;

    los_boundary = lspace->heap_end;
}

void lspace_sweep(Lspace* lspace)
{
  unsigned int mark_bit_idx = 0;
  POINTER_SIZE_INT cur_size = 0;
  void *cur_area_start, *cur_area_end;

  /*If it is TRANS_FROM_MOS_TO_LOS now, we must clear the fa alread added in lspace_reset_after_collection*/
  free_area_pool_reset(lspace->free_pool);

  Partial_Reveal_Object* p_prev_obj = (Partial_Reveal_Object *)lspace->heap_start;
  Partial_Reveal_Object* p_next_obj = lspace_get_first_marked_object(lspace, &mark_bit_idx);
  if(p_next_obj){
    obj_unmark_in_vt(p_next_obj);
    /* we need this because, in hybrid situation of gen_mode and non_gen_mode, LOS will only be marked
       in non_gen_mode, and not reset in gen_mode. When it switches back from gen_mode to non_gen_mode,
       the last time marked object is thought to be already marked and not scanned for this cycle. */
    obj_clear_dual_bits_in_oi(p_next_obj);
    /*For_statistic: sum up the size of suvived large objects, useful to deciede los extention.*/
    lspace->surviving_size += ALIGN_UP_TO_KILO(vm_object_size(p_next_obj));    
  }

  cur_area_start = (void*)ALIGN_UP_TO_KILO(p_prev_obj);
  cur_area_end = (void*)ALIGN_DOWN_TO_KILO(p_next_obj);


  while(cur_area_end){
    cur_size = (POINTER_SIZE_INT)cur_area_end - (POINTER_SIZE_INT)cur_area_start;
      
    Free_Area* cur_area = free_area_new(cur_area_start, cur_size);
    /* successfully create an area */
    if( cur_area )
      free_pool_add_area(lspace->free_pool, cur_area);

    p_prev_obj = p_next_obj;
    p_next_obj = lspace_get_next_marked_object(lspace, &mark_bit_idx);
    if(p_next_obj){
      obj_unmark_in_vt(p_next_obj);
      obj_clear_dual_bits_in_oi(p_next_obj);
      /*For_statistic: sum up the size of suvived large objects, useful to deciede los extention.*/
      lspace->surviving_size += ALIGN_UP_TO_KILO(vm_object_size(p_next_obj));
    }

    cur_area_start = (void*)ALIGN_UP_TO_KILO((POINTER_SIZE_INT)p_prev_obj + vm_object_size(p_prev_obj));
    cur_area_end = (void*)ALIGN_DOWN_TO_KILO(p_next_obj);
    
  }

   /* cur_area_end == NULL */
  cur_area_end = (void*)ALIGN_DOWN_TO_KILO(lspace->heap_end);
  cur_size = (POINTER_SIZE_INT)cur_area_end - (POINTER_SIZE_INT)cur_area_start;
  Free_Area* cur_area = free_area_new(cur_area_start, cur_size);
  /* successfully create an area */
  if( cur_area )
    free_pool_add_area(lspace->free_pool, cur_area);

   mark_bit_idx = 0;
   assert(!lspace_get_first_marked_object(lspace, &mark_bit_idx));

  /*Update survive ratio here. If we tune LOS this time, the ratio is computed by the new committed size.*/
  /*Fixme: We should keep the surviving size of last time, and set denominator to last_survive + current_alloc*/
  lspace->survive_ratio = (float)lspace->surviving_size / (float)lspace->committed_heap_size;

  return;

}
