#include "wspace.h"
#include "wspace_chunk.h"
#include "wspace_mark_sweep.h"
#include "gc_ms.h"
#include "../gen/gen.h"

static void collector_sweep_normal_chunk_concurrent(Collector *collector, Wspace *wspace, Chunk_Header *chunk)
{
  unsigned int slot_num = chunk->slot_num;
  unsigned int live_num = 0;
  unsigned int first_free_word_index = MAX_SLOT_INDEX;
  POINTER_SIZE_INT *table = chunk->table;
  
  unsigned int index_word_num = (slot_num + SLOT_NUM_PER_WORD_IN_TABLE - 1) / SLOT_NUM_PER_WORD_IN_TABLE;
  for(unsigned int i=0; i<index_word_num; ++i){
    table[i] &= cur_alloc_mask;
    unsigned int live_num_in_word = (table[i] == cur_alloc_mask) ? SLOT_NUM_PER_WORD_IN_TABLE : word_set_bit_num(table[i]);
    live_num += live_num_in_word;
    if((first_free_word_index == MAX_SLOT_INDEX) && (live_num_in_word < SLOT_NUM_PER_WORD_IN_TABLE)){
      first_free_word_index = i;
      pfc_set_slot_index((Chunk_Header*)chunk, first_free_word_index, cur_alloc_color);
    }
  }
  assert(live_num <= slot_num);
  collector->live_obj_size += live_num * chunk->slot_size;
  collector->live_obj_num += live_num;

  if(!live_num){  /* all objects in this chunk are dead */
    collector_add_free_chunk(collector, (Free_Chunk*)chunk);
   } else if(!chunk_is_reusable(chunk)){  /* most objects in this chunk are swept, add chunk to pfc list*/
    chunk->alloc_num = live_num;
    wspace_register_unreusable_normal_chunk(wspace, chunk);
   } else {  /* most objects in this chunk are swept, add chunk to pfc list*/
    chunk->alloc_num = live_num;
    wspace_put_pfc_backup(wspace, chunk);
  }
}

static inline void collector_sweep_abnormal_chunk_concurrent(Collector *collector, Wspace *wspace, Chunk_Header *chunk)
{
  assert(chunk->status == (CHUNK_ABNORMAL | CHUNK_USED));
  POINTER_SIZE_INT *table = chunk->table;
  table[0] &= cur_alloc_mask;
  if(!table[0]){    
    collector_add_free_chunk(collector, (Free_Chunk*)chunk);
  }
  else {
    wspace_register_live_abnormal_chunk(wspace, chunk);
    collector->live_obj_size += CHUNK_SIZE(chunk);
    collector->live_obj_num++;
  }
}

static void wspace_sweep_chunk_concurrent(Wspace* wspace, Collector* collector, Chunk_Header_Basic* chunk)
{  
  if(chunk->status & CHUNK_NORMAL){   /* chunk is used as a normal sized obj chunk */
    assert(chunk->status == (CHUNK_NORMAL | CHUNK_USED));
    collector_sweep_normal_chunk_concurrent(collector, wspace, (Chunk_Header*)chunk);
  } else {  /* chunk is used as a super obj chunk */
    assert(chunk->status == (CHUNK_ABNORMAL | CHUNK_USED));
    collector_sweep_abnormal_chunk_concurrent(collector, wspace, (Chunk_Header*)chunk);
  }
}

static Free_Chunk_List* wspace_get_free_chunk_list(Wspace* wspace)
{
  GC* gc = wspace->gc;
  Free_Chunk_List* free_chunk_list = (Free_Chunk_List*) STD_MALLOC(sizeof(Free_Chunk_List));
  assert(free_chunk_list);
  memset(free_chunk_list, 0, sizeof(Free_Chunk_List));
  
  /* Collect free chunks from collectors to one list */
  for(unsigned int i=0; i<gc->num_collectors; ++i){
    Free_Chunk_List *list = gc->collectors[i]->free_chunk_list;
    move_free_chunks_between_lists(free_chunk_list, list);
  }
  
  return free_chunk_list;
}

Boolean wspace_get_free_chunk_concurrent(Wspace *wspace, Free_Chunk* chunk)
{
  POINTER_SIZE_INT chunk_size = CHUNK_SIZE(chunk);
  assert(!(chunk_size % CHUNK_GRANULARITY));

  Free_Chunk_List* free_list = NULL;

  /*Find list*/
  if(chunk_size > HYPER_OBJ_THRESHOLD)
    free_list = wspace->hyper_free_chunk_list;
  else if(!((POINTER_SIZE_INT)chunk & NORMAL_CHUNK_LOW_MASK) && !(chunk_size & NORMAL_CHUNK_LOW_MASK))
    free_list = &wspace->aligned_free_chunk_lists[ALIGNED_CHUNK_SIZE_TO_INDEX(chunk_size)];
  else
    free_list = &wspace->unaligned_free_chunk_lists[UNALIGNED_CHUNK_SIZE_TO_INDEX(chunk_size)];

  /*Lock this free list*/
  lock(free_list->lock);

  /*Search free list for chunk*/
  Free_Chunk* chunk_iter = free_list->head;
  while((POINTER_SIZE_INT)chunk_iter){
    if((POINTER_SIZE_INT)chunk_iter == (POINTER_SIZE_INT)chunk){
      /*Find chunk and delete from list.*/     
      free_list_detach_chunk(free_list, chunk);
      unlock(free_list->lock);
      return TRUE;
    }
    chunk_iter = chunk_iter->next;
  }
  
  unlock(free_list->lock);
  
  return FALSE;
}

void wspace_merge_adj_free_chunks(Wspace* wspace,Free_Chunk* chunk)
{
  Free_Chunk *wspace_ceiling = (Free_Chunk*)space_heap_end((Space*)wspace);

  /* Check if the back adjcent chunks are free */
  Free_Chunk *back_chunk = (Free_Chunk*)chunk->adj_next;
  while(back_chunk < wspace_ceiling && (back_chunk->status & CHUNK_FREE)){
    assert(chunk < back_chunk);
    /* Remove back_chunk from list */
    if(wspace_get_free_chunk_concurrent(wspace,back_chunk)){
      back_chunk = (Free_Chunk*)back_chunk->adj_next;
      chunk->adj_next = (Chunk_Header_Basic*)back_chunk;
    }else{
      break;
    }
  }

  chunk->status = CHUNK_FREE | CHUNK_MERGED;
  /* put the free chunk to the according free chunk list */
  wspace_put_free_chunk_to_tail(wspace, chunk);

}

static void wspace_merge_list_concurrent(Wspace* wspace, Free_Chunk_List* free_list)
{
  lock(free_list->lock);
  Free_Chunk* chunk = free_list->head;
  
  while(chunk && !is_free_chunk_merged(chunk)){
    free_list_detach_chunk(free_list, chunk);
    unlock(free_list->lock);
    
    wspace_merge_adj_free_chunks(wspace, chunk);
    
    lock(free_list->lock);
    chunk = free_list->head;
  }
  
  unlock(free_list->lock);
}

static void wspace_merge_free_chunks_concurrent(Wspace* wspace, Free_Chunk_List* free_list)
{
  Free_Chunk *chunk = free_list->head;

  /*merge free list*/
  wspace_merge_list_concurrent(wspace, free_list);
  
  /*check free pool*/
  unsigned int i;
  
  for(i = NUM_ALIGNED_FREE_CHUNK_BUCKET; i--;)
    wspace_merge_list_concurrent(wspace, &wspace->aligned_free_chunk_lists[i]);

  for(i = NUM_UNALIGNED_FREE_CHUNK_BUCKET; i--;)
    wspace_merge_list_concurrent(wspace, &wspace->unaligned_free_chunk_lists[i]);

  wspace_merge_list_concurrent(wspace, wspace->hyper_free_chunk_list);
}

static void wspace_reset_free_list_chunks(Wspace* wspace, Free_Chunk_List* free_list)
{
  lock(free_list->lock);
  Free_Chunk* chunk = free_list->head;
  
  while(chunk ){
    assert(chunk->status & CHUNK_FREE);
    chunk->status = CHUNK_FREE;
    chunk = chunk->next;
  }
  
  unlock(free_list->lock);
}


static void wspace_reset_free_chunks_status(Wspace* wspace)
{
  unsigned int i;
  
  for(i = NUM_ALIGNED_FREE_CHUNK_BUCKET; i--;)
    wspace_reset_free_list_chunks(wspace, &wspace->aligned_free_chunk_lists[i]);

  for(i = NUM_UNALIGNED_FREE_CHUNK_BUCKET; i--;)
    wspace_reset_free_list_chunks(wspace, &wspace->unaligned_free_chunk_lists[i]);

  wspace_reset_free_list_chunks(wspace, wspace->hyper_free_chunk_list);

}

static void allocator_sweep_local_chunks(Allocator *allocator)
{
  Wspace *wspace = gc_get_wspace(allocator->gc);
  Size_Segment **size_segs = wspace->size_segments;
  Chunk_Header ***local_chunks = allocator->local_chunks;
  
  for(unsigned int i = SIZE_SEGMENT_NUM; i--;){
    if(!size_segs[i]->local_alloc){
      assert(!local_chunks[i]);
      continue;
    }
    Chunk_Header **chunks = local_chunks[i];
    assert(chunks);
    for(unsigned int j = size_segs[i]->chunk_num; j--;){
      if(chunks[j]){
        unsigned int slot_num = chunks[j]->slot_num;
        POINTER_SIZE_INT *table = chunks[j]->table;
        
        unsigned int index_word_num = (slot_num + SLOT_NUM_PER_WORD_IN_TABLE - 1) / SLOT_NUM_PER_WORD_IN_TABLE;
        for(unsigned int i=0; i<index_word_num; ++i){
          //atomic sweep.
          POINTER_SIZE_INT old_word = table[i];
          POINTER_SIZE_INT new_word = old_word & cur_alloc_mask;
          while(old_word != new_word){
            POINTER_SIZE_INT temp = (POINTER_SIZE_INT)atomic_casptr((volatile void**) &table[i],(void*) new_word,(void*) old_word);
            if(temp == old_word){
              break;
            }
            old_word = table[i];
            new_word = old_word & cur_alloc_mask;
          }
        }
      }
    }
  }
}


static void gc_sweep_mutator_local_chunks(GC *gc)
{
#ifdef USE_MARK_SWEEP_GC
  lock(gc->mutator_list_lock);     // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

  /* release local chunks of each mutator in unique mark-sweep GC */
  Mutator *mutator = gc->mutator_list;
  while(mutator){
    wait_mutator_signal(mutator, DISABLE_COLLECTOR_SWEEP_LOCAL_CHUNKS);
    allocator_sweep_local_chunks((Allocator*)mutator);
    mutator = mutator->next;
  }

  unlock(gc->mutator_list_lock);
#endif
}

static void gc_check_mutator_local_chunks(GC *gc, unsigned int handshake_signal)
{
  lock(gc->mutator_list_lock);     // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

  /* release local chunks of each mutator in unique mark-sweep GC */
  Mutator *mutator = gc->mutator_list;
  while(mutator){
    wait_mutator_signal(mutator, handshake_signal);
    mutator = mutator->next;
  }

  unlock(gc->mutator_list_lock);
}


static volatile unsigned int num_sweeping_collectors = 0;

/*Concurrent Sweep:  
   The mark bit and alloc bit is exchanged before entering this function. 
   This function is to clear the mark bit and merge the free chunks concurrently.   
  */
void wspace_sweep_concurrent(Collector* collector)
{
  GC *gc = collector->gc;
  Wspace *wspace = gc_get_wspace(gc);

  unsigned int num_active_collectors = gc->num_active_collectors;
  
  atomic_cas32(&num_sweeping_collectors, 0, num_active_collectors+1);

  Pool* used_chunk_pool = wspace->used_chunk_pool;

  Chunk_Header_Basic* chunk_to_sweep;
  
  /*1. Grab chunks from used list, sweep the chunk and push back to PFC backup list & free list.*/
  chunk_to_sweep = chunk_pool_get_chunk(used_chunk_pool);
  while(chunk_to_sweep != NULL){
    wspace_sweep_chunk_concurrent(wspace, collector, chunk_to_sweep);
    chunk_to_sweep = chunk_pool_get_chunk(used_chunk_pool);
  }

  /*2. Grab chunks from PFC list, sweep the chunk and push back to PFC backup list & free list.*/
  Pool* pfc_pool = wspace_grab_next_pfc_pool(wspace);
  while(pfc_pool != NULL){
    if(!pool_is_empty(pfc_pool)){
      /*sweep the chunks in pfc_pool. push back to pfc backup list*/
      chunk_to_sweep = chunk_pool_get_chunk(pfc_pool);
      while(chunk_to_sweep != NULL){
        assert(chunk_to_sweep->status == (CHUNK_NORMAL | CHUNK_NEED_ZEROING));
        chunk_to_sweep->status = CHUNK_NORMAL | CHUNK_USED;
        wspace_sweep_chunk_concurrent(wspace, collector, chunk_to_sweep);
        chunk_to_sweep = chunk_pool_get_chunk(pfc_pool);
      }
    }
    /*grab more pfc pools*/
    pfc_pool = wspace_grab_next_pfc_pool(wspace);
  }

  unsigned int old_num = atomic_inc32(&num_sweeping_collectors);
  if( ++old_num == num_active_collectors ){    
    
    /*3. Check the local chunk of mutator*/
    gc_sweep_mutator_local_chunks(wspace->gc);

    /*4. Sweep gloabl alloc normal chunks again*/
    gc_set_sweeping_global_normal_chunk();
    gc_check_mutator_local_chunks(wspace->gc, DISABLE_COLLECTOR_SWEEP_GLOBAL_CHUNKS);
    wspace_init_pfc_pool_iterator(wspace);
    Pool* pfc_pool = wspace_grab_next_pfc_pool(wspace);
    while(pfc_pool != NULL){
      if(!pool_is_empty(pfc_pool)){
        chunk_to_sweep = chunk_pool_get_chunk(pfc_pool);
        while(chunk_to_sweep != NULL){
          assert(chunk_to_sweep->status == (CHUNK_NORMAL | CHUNK_NEED_ZEROING));
          chunk_to_sweep->status = CHUNK_NORMAL | CHUNK_USED;
          wspace_sweep_chunk_concurrent(wspace, collector, chunk_to_sweep);
          chunk_to_sweep = chunk_pool_get_chunk(pfc_pool);
        }
      }
      /*grab more pfc pools*/
      pfc_pool = wspace_grab_next_pfc_pool(wspace);
    }
    gc_unset_sweeping_global_normal_chunk();
    
    /*4. Check the used list again.*/
    chunk_to_sweep = chunk_pool_get_chunk(used_chunk_pool);
    while(chunk_to_sweep != NULL){
      wspace_sweep_chunk_concurrent(wspace, collector, chunk_to_sweep);
      chunk_to_sweep = chunk_pool_get_chunk(used_chunk_pool);
    }

    /*5. Switch the PFC backup list to PFC list.*/
    wspace_exchange_pfc_pool(wspace);

    /*6. Put back live abnormal chunk and normal unreusable chunk*/
    Chunk_Header* used_abnormal_chunk = wspace_get_live_abnormal_chunk(wspace);
    while(used_abnormal_chunk){      
      used_abnormal_chunk->status = CHUNK_USED | CHUNK_ABNORMAL;
      wspace_register_used_chunk(wspace,used_abnormal_chunk);
      used_abnormal_chunk = wspace_get_live_abnormal_chunk(wspace);
    }
    pool_empty(wspace->live_abnormal_chunk_pool);

    Chunk_Header* unreusable_normal_chunk = wspace_get_unreusable_normal_chunk(wspace);
    while(unreusable_normal_chunk){  
      unreusable_normal_chunk->status = CHUNK_USED | CHUNK_NORMAL;
      wspace_register_used_chunk(wspace,unreusable_normal_chunk);
      unreusable_normal_chunk = wspace_get_unreusable_normal_chunk(wspace);
    }
    pool_empty(wspace->unreusable_normal_chunk_pool);
    
    
    /*7. Merge free chunks*/
    Free_Chunk_List* free_chunk_list = wspace_get_free_chunk_list(wspace);
    wspace_merge_free_chunks_concurrent(wspace, free_chunk_list);
    wspace_reset_free_chunks_status(wspace);
    
    /* let other collectors go */
    num_sweeping_collectors++;
  }
  while(num_sweeping_collectors != num_active_collectors + 1);
}

