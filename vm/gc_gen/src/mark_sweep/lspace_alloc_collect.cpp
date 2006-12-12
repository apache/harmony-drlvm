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
struct GC_Gen;
Space* gc_get_los(GC_Gen* gc);

void* lspace_alloc(unsigned int size, Allocator *allocator)
{
  vm_gc_lock_enum();
  unsigned int try_count = 0;
  Lspace* lspace = (Lspace*)gc_get_los((GC_Gen*)allocator->gc);
  Free_Area_Pool* pool = lspace->free_pool;
  Free_Area* free_area;
  Free_Area* remain_area;
  void* p_return = NULL;

  while( try_count < 2 ){
    free_area = free_pool_find_size_area(pool, size);
    
    /*Got one free area!*/
    if(free_area != NULL){
      assert(free_area->size >= size);
      free_pool_remove_area(pool, free_area);
      p_return = (void*)free_area;
      unsigned int old_size = free_area->size;
      memset(p_return, 0, sizeof(Free_Area));

      /* we need put the remaining area back if it size is ok.*/
      void* new_start = (Free_Area*)ALIGN_UP_TO_KILO(((unsigned int)free_area + size));
      unsigned int alloc_size = (unsigned int)new_start - (unsigned int)free_area;
      unsigned int new_size = old_size - alloc_size;
      
      remain_area = free_area_new(new_start, new_size);
      if( remain_area )
        free_pool_add_area(pool, remain_area);

      vm_gc_unlock_enum();
      return p_return;
    }

    if(try_count++ == 0) 
      gc_reclaim_heap(allocator->gc, GC_CAUSE_LOS_IS_FULL);

  }

  vm_gc_unlock_enum();
  return NULL;
}

void lspace_sweep(Lspace* lspace)
{
  /* reset the pool first because its info is useless now. */
  free_area_pool_reset(lspace->free_pool);

  unsigned int mark_bit_idx, cur_size;
  void *cur_area_start, *cur_area_end;



  Partial_Reveal_Object* p_prev_obj = (Partial_Reveal_Object *)lspace->heap_start;
  Partial_Reveal_Object* p_next_obj = lspace_get_first_marked_object(lspace, &mark_bit_idx);

  cur_area_start = (void*)ALIGN_UP_TO_KILO(p_prev_obj);
  cur_area_end = (void*)ALIGN_DOWN_TO_KILO(p_next_obj);


  while(cur_area_end){
    cur_size = (unsigned int)cur_area_end - (unsigned int)cur_area_start;
      
    Free_Area* cur_area = free_area_new(cur_area_start, cur_size);
    /* successfully create an area */
    if( cur_area )
      free_pool_add_area(lspace->free_pool, cur_area);

    p_prev_obj = p_next_obj;
    p_next_obj = lspace_get_next_marked_object(lspace, &mark_bit_idx);

    cur_area_start = (void*)ALIGN_UP_TO_KILO((unsigned int)p_prev_obj + vm_object_size(p_prev_obj));
    cur_area_end = (void*)ALIGN_DOWN_TO_KILO(p_next_obj);
    
  }

   /* cur_area_end == NULL */
  cur_area_end = (void*)ALIGN_DOWN_TO_KILO(lspace->heap_end);
  cur_size = (unsigned int)cur_area_end - (unsigned int)cur_area_start;
  Free_Area* cur_area = free_area_new(cur_area_start, cur_size);
  /* successfully create an area */
  if( cur_area )
    free_pool_add_area(lspace->free_pool, cur_area);

   return;

}
