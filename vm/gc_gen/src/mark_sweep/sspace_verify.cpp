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

#include "sspace_verify.h"
#include "sspace_chunk.h"
#include "sspace_mark_sweep.h"
#include "../utils/vector_block.h"
#include "../gen/gen.h"
#include "../finalizer_weakref/finalizer_weakref.h"

#ifdef SSPACE_VERIFY

#define VERIFY_CARD_SIZE_BYTES_SHIFT 12
#define VERIFY_CARD_SIZE_BYTES (1 << VERIFY_CARD_SIZE_BYTES_SHIFT)
#define VERIFY_CARD_LOW_MASK (VERIFY_CARD_SIZE_BYTES - 1)
#define VERIFY_CARD_HIGH_MASK (~VERIFY_CARD_LOW_MASK)

#define VERIFY_MAX_OBJ_SIZE_BYTES (1 << (32-VERIFY_CARD_SIZE_BYTES_SHIFT))

typedef struct Verify_Card {
  SpinLock lock;
  Vector_Block *block;
} Verify_Card;

typedef unsigned int Obj_Addr;

static GC *gc_in_verify = NULL;
static Verify_Card *alloc_verify_cards = NULL;
static Verify_Card *mark_verify_cards = NULL;
static POINTER_SIZE_INT card_num = 0;
static POINTER_SIZE_INT alloc_obj = 0;
volatile POINTER_SIZE_INT live_obj_in_mark = 0;

void sspace_verify_init(GC *gc)
{
  gc_in_verify = gc;
  
  POINTER_SIZE_INT heap_size = gc_gen_total_memory_size((GC_Gen*)gc);
  card_num = heap_size >> VERIFY_CARD_SIZE_BYTES_SHIFT;
  POINTER_SIZE_INT cards_size = sizeof(Verify_Card) * card_num;
  
  alloc_verify_cards = (Verify_Card*)STD_MALLOC(cards_size);
  memset(alloc_verify_cards, 0, cards_size);
  
  mark_verify_cards = (Verify_Card*)STD_MALLOC(cards_size);
  memset(mark_verify_cards, 0, cards_size);
}

static Obj_Addr compose_obj_addr(unsigned int offset, unsigned int size)
{
  assert(size < VERIFY_MAX_OBJ_SIZE_BYTES);
  return offset | (size << VERIFY_CARD_SIZE_BYTES_SHIFT);
}

static void *decompose_obj_addr(Obj_Addr obj_addr, POINTER_SIZE_INT card_index, unsigned int & size)
{
  assert(card_index < card_num);
  POINTER_SIZE_INT card_offset = obj_addr & VERIFY_CARD_LOW_MASK;
  POINTER_SIZE_INT heap_offset = VERIFY_CARD_SIZE_BYTES * card_index + card_offset;
  size = (obj_addr & VERIFY_CARD_HIGH_MASK) >> VERIFY_CARD_SIZE_BYTES_SHIFT;
  assert(size < VERIFY_MAX_OBJ_SIZE_BYTES);
  return (void*)(heap_offset + (POINTER_SIZE_INT)gc_heap_base(gc_in_verify));
}

static Boolean obj_addr_overlapped(Obj_Addr addr1, Obj_Addr addr2)
{
  unsigned int offset1 = addr1 & VERIFY_CARD_LOW_MASK;
  unsigned int size1 = (addr1 & VERIFY_CARD_HIGH_MASK) >> VERIFY_CARD_SIZE_BYTES_SHIFT;
  unsigned int ceiling1 = offset1 + size1;
  unsigned int offset2 = addr2 & VERIFY_CARD_LOW_MASK;
  unsigned int size2 = (addr2 & VERIFY_CARD_HIGH_MASK) >> VERIFY_CARD_SIZE_BYTES_SHIFT;
  unsigned int ceiling2 = offset2 + size2;
  
  unsigned int reason = 0;
  if(offset1 == offset2)
    reason = 1;
  if((offset1 < offset2) && (ceiling1 > offset2))
    reason = 2;
  if((offset2 < offset1) && (ceiling2 > offset1))
    reason = 3;
  if(!reason)
    return FALSE;
  printf("\nreason: %d\nold offset: %x  size: %d\nnew offset: %x size: %d", reason, (POINTER_SIZE_INT)offset1, size1, (POINTER_SIZE_INT)offset2, size2);
  return TRUE;
}

static Vector_Block *create_vector_block(unsigned int size)
{
  Vector_Block *block = (Vector_Block*)STD_MALLOC(size);
  vector_block_init(block, size);
  return block;
}

static void verify_card_get_block(Verify_Card *card)
{
  lock(card->lock);
  if(card->block){
    unlock(card->lock);
    return;
  }
  card->block = create_vector_block(VECTOR_BLOCK_DATA_SIZE_BYTES);
  unlock(card->lock);
}

void sspace_verify_alloc(void *addr, unsigned int size)
{
  assert(address_belongs_to_gc_heap(addr, gc_in_verify));
  ++alloc_obj;
  
  unsigned int heap_offset = (unsigned int)((POINTER_SIZE_INT)addr - (POINTER_SIZE_INT)gc_heap_base(gc_in_verify));
  unsigned int card_offset = heap_offset & VERIFY_CARD_LOW_MASK;
  Verify_Card *card = &alloc_verify_cards[heap_offset >> VERIFY_CARD_SIZE_BYTES_SHIFT];
  
  verify_card_get_block(card);
  Vector_Block *block = card->block;
  
  Obj_Addr obj_addr = compose_obj_addr(card_offset, size);
  
  lock(card->lock);
  Obj_Addr *p_addr = block->head;
  while(p_addr < block->tail){
    assert(!obj_addr_overlapped(obj_addr, *p_addr));
    p_addr++;
  }
  vector_block_add_entry(block, obj_addr);
  unlock(card->lock);
}

/* size is rounded up size */
static Boolean obj_position_is_correct(void *addr, unsigned int size)
{
  Chunk_Header *chunk = NULL;
  
  if(size <= SUPER_OBJ_THRESHOLD)
    chunk = NORMAL_CHUNK_HEADER(addr);
  else
    chunk = ABNORMAL_CHUNK_HEADER(addr);
  if(chunk->slot_size != size) return FALSE;
  if(((POINTER_SIZE_INT)addr - (POINTER_SIZE_INT)chunk->base) % size != 0) return FALSE;
  return TRUE;
}

/* size is real size of obj */
void sspace_verify_mark(void *addr, unsigned int size)
{
  assert(address_belongs_to_gc_heap(addr, gc_in_verify));
  atomic_inc32(&live_obj_in_mark);
  
  unsigned int heap_offset = (unsigned int)((POINTER_SIZE_INT)addr - (POINTER_SIZE_INT)gc_heap_base(gc_in_verify));
  unsigned int card_offset = heap_offset & VERIFY_CARD_LOW_MASK;
  Verify_Card *card = &mark_verify_cards[heap_offset >> VERIFY_CARD_SIZE_BYTES_SHIFT];
  
  verify_card_get_block(card);
  Vector_Block *block = card->block;
  
  if(size <= MEDIUM_OBJ_THRESHOLD)
    size = SMALL_SIZE_ROUNDUP(size);
  else if(size <= LARGE_OBJ_THRESHOLD)
    size = MEDIUM_SIZE_ROUNDUP(size);
  else if(size <= SUPER_OBJ_THRESHOLD)
    size = LARGE_SIZE_ROUNDUP(size);
  assert(obj_position_is_correct(addr, size));
  Obj_Addr obj_addr = compose_obj_addr(card_offset, size);
  
  lock(card->lock);
  Obj_Addr *p_addr = block->head;
  while(p_addr < block->tail){
    assert(!obj_addr_overlapped(obj_addr, *p_addr));
    p_addr++;
  }
  vector_block_add_entry(block, obj_addr);
  unlock(card->lock);
}

static void reverify_mark(void *addr, unsigned int size)
{
  assert(address_belongs_to_gc_heap(addr, gc_in_verify));
  
  unsigned int heap_offset = (unsigned int)((POINTER_SIZE_INT)addr - (POINTER_SIZE_INT)gc_heap_base(gc_in_verify));
  unsigned int card_offset = heap_offset & VERIFY_CARD_LOW_MASK;
  Verify_Card *card = &mark_verify_cards[heap_offset >> VERIFY_CARD_SIZE_BYTES_SHIFT];
  
  Vector_Block *block = card->block;
  assert(block);
  
  Obj_Addr obj_addr = compose_obj_addr(card_offset, size);
  
  Obj_Addr *p_addr = block->head;
  while(p_addr < block->tail){
    if(obj_addr == *p_addr){
      *p_addr = 0;
      break;
    }
    p_addr++;
  }
  assert(p_addr < block->tail);
}

static void check_mark_cards(void)
{
  for(POINTER_SIZE_INT i=0; i<card_num; i++){
    Vector_Block *block = mark_verify_cards[i].block;
    if(!block)
      continue;
    Obj_Addr *p_addr = block->head;
    while(p_addr < block->tail){
      if(*p_addr){
        unsigned int size = 0;
        void *addr = NULL;
        addr = decompose_obj_addr(*p_addr, i, size);
        printf("Extra mark obj: %x  size: %d\n", (POINTER_SIZE_INT)addr, size);
      }
      p_addr++;
    }
    vector_block_clear(block);
  }
}

static void clear_verify_cards(void)
{
  for(POINTER_SIZE_INT i=0; i<card_num; i++){
    Verify_Card *card = &alloc_verify_cards[i];
    if(card->block)
      vector_block_clear(card->block);
  }
}

static void summarize_sweep_verify(GC *gc)
{
  POINTER_SIZE_INT live_obj_num = 0;
  for(unsigned int i=0; i<gc->num_collectors; ++i){
    live_obj_num += gc->collectors[i]->live_obj_num;
  }
  printf("Live obj in sweeping: %d\n", live_obj_num);
}

void sspace_verify_free_area(POINTER_SIZE_INT *start, POINTER_SIZE_INT size)
{
  POINTER_SIZE_INT *p_value = start;
  
  assert(!(size % BYTES_PER_WORD));
  size /= BYTES_PER_WORD;
  while(size--)
    assert(!*p_value++);
}

void sspace_verify_after_collection(GC *gc)
{
  printf("Allocated obj: %d\n", alloc_obj);
  alloc_obj = 0;
  printf("Live obj in marking: %d\n", live_obj_in_mark);
  live_obj_in_mark = 0;
  
  summarize_sweep_verify(gc);
  
  clear_verify_cards();
  
  Sspace *sspace = (Sspace*)gc_get_pos((GC_Gen*)gc);
  Chunk_Header *chunk = (Chunk_Header*)space_heap_start((Space*)sspace);
  Chunk_Header *sspace_ceiling = (Chunk_Header*)space_heap_end((Space*)sspace);
  POINTER_SIZE_INT total_live_obj = 0;
  
  for(; chunk < sspace_ceiling; chunk = (Chunk_Header*)CHUNK_END(chunk)){
    /* chunk is free before GC */
    if(chunk->status == CHUNK_FREE){
      unsigned int header_size = sizeof(Free_Chunk);
      //sspace_verify_free_area((POINTER_SIZE_INT*)((POINTER_SIZE_INT)chunk + header_size), (POINTER_SIZE_INT)chunk->adj_next - (POINTER_SIZE_INT)chunk - header_size);
      continue;
    }
    if(chunk->status & CHUNK_ABNORMAL){
      assert(chunk->status == (CHUNK_ABNORMAL | CHUNK_IN_USE));
      assert(chunk->slot_size > SUPER_OBJ_THRESHOLD);
      Partial_Reveal_Object *obj = (Partial_Reveal_Object*)chunk->base;
      assert(chunk->slot_size == vm_object_size(obj));
      assert(get_obj_info_raw(obj) & SUPER_OBJ_MASK);
    }
    /* chunk is used as a normal sized obj chunk */
    unsigned int slot_num = chunk->slot_num;
    POINTER_SIZE_INT *table = chunk->table;
    POINTER_SIZE_INT live_obj_in_chunk = 0;
    
    unsigned int word_index = 0;
    for(unsigned int i=0; i<slot_num; ++i){
      unsigned int color_index = COLOR_BITS_PER_OBJ * i;
      word_index = color_index / BITS_PER_WORD;
      void *p_obj = slot_index_to_addr(chunk, i);
      if(table[word_index] & (cur_alloc_color << (color_index % BITS_PER_WORD))){
        sspace_verify_alloc(p_obj, chunk->slot_size);
        reverify_mark(p_obj, chunk->slot_size);
        ++live_obj_in_chunk;
      } else {
        //sspace_verify_free_area((POINTER_SIZE_INT*)p_obj, chunk->slot_size);
      }
    }
    total_live_obj += live_obj_in_chunk;
  }
  printf("Live obj after collection: %d\n", total_live_obj);
  check_mark_cards();
}

/*
void sspace_verify_super_obj(GC *gc)
{
  Sspace *sspace = (Sspace*)gc_get_pos((GC_Gen*)gc);
  Chunk_Header *chunk = (Chunk_Header*)space_heap_start((Space*)sspace);
  Chunk_Header *sspace_ceiling = (Chunk_Header*)space_heap_end((Space*)sspace);
  
  for(; chunk < sspace_ceiling; chunk = (Chunk_Header*)CHUNK_END(chunk)){
    if(chunk->status & CHUNK_ABNORMAL){
      assert(chunk->status == (CHUNK_ABNORMAL | CHUNK_IN_USE));
      assert(chunk->slot_size > SUPER_OBJ_THRESHOLD);
      Partial_Reveal_Object *obj = (Partial_Reveal_Object*)chunk->base;
      assert(chunk->slot_size == vm_object_size(obj));
      assert(get_obj_info_raw(obj) & SUPER_OBJ_MASK);
    }
  }
}
*/


/* sspace verify marking with vtable marking in advance */

static Pool *trace_pool = NULL;
static Vector_Block *trace_stack = NULL;
POINTER_SIZE_INT live_obj_in_verify_marking = 0;

static Boolean obj_mark_in_vtable(GC *gc, Partial_Reveal_Object *obj)
{
  assert(address_belongs_to_gc_heap(obj, gc));
  assert((vm_object_size(obj) <= SUPER_OBJ_THRESHOLD) || (get_obj_info_raw(obj) & SUPER_OBJ_MASK));
  Boolean marked = obj_mark_in_vt(obj);
#ifdef SSPACE_VERIFY
  if(marked) live_obj_in_verify_marking++;
#endif
  return marked;
}

static void tracestack_push(void *p_obj)
{
  vector_stack_push(trace_stack, (POINTER_SIZE_INT)p_obj);
  
  if( !vector_stack_is_full(trace_stack)) return;
  
  pool_put_entry(trace_pool, trace_stack);
  trace_stack = free_task_pool_get_entry(&gc_metadata);
  assert(trace_stack);
}

static FORCE_INLINE void scan_slot(GC *gc, REF *p_ref)
{
  Partial_Reveal_Object *p_obj = read_slot(p_ref);
  if( p_obj == NULL) return;
  
  if(obj_mark_in_vtable(gc, p_obj))
    tracestack_push(p_obj);
  
  return;
}

static FORCE_INLINE void scan_object(GC *gc, Partial_Reveal_Object *p_obj)
{
  if(!object_has_ref_field(p_obj) ) return;
  
  REF *p_ref;
  
  if (object_is_array(p_obj)) {   /* scan array object */
    
    Partial_Reveal_Array *array = (Partial_Reveal_Array*)p_obj;
    unsigned int array_length = array->array_len;
    
    p_ref = (REF*)((POINTER_SIZE_INT)array + (int)array_first_element_offset(array));
    
    for (unsigned int i = 0; i < array_length; i++) {
      scan_slot(gc, p_ref+i);
    }
  } else { /* scan non-array object */
    
    unsigned int num_refs = object_ref_field_num(p_obj);
    
    int *ref_iterator = object_ref_iterator_init(p_obj);
    
    for(unsigned int i=0; i<num_refs; i++){
      p_ref = object_ref_iterator_get(ref_iterator+i, p_obj);
      scan_slot(gc, p_ref);
    }
#ifndef BUILD_IN_REFERENT
    //scan_weak_reference(collector, p_obj, scan_slot);
#endif
  }
}

static void trace_object(GC *gc, Partial_Reveal_Object *p_obj)
{
  scan_object(gc, p_obj);
  
  while( !vector_stack_is_empty(trace_stack)){
    p_obj = (Partial_Reveal_Object *)vector_stack_pop(trace_stack);
    scan_object(gc, p_obj);
  }
}

void sspace_verify_vtable_mark(GC *gc)
{
  GC_Metadata *metadata = gc->metadata;
  Pool *rootset_pool = metadata->gc_rootset_pool;
  
  trace_stack = free_task_pool_get_entry(metadata);
  trace_pool = sync_pool_create();
  
  pool_iterator_init(rootset_pool);
  Vector_Block *root_set = pool_iterator_next(rootset_pool);
  
  while(root_set){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(root_set);
    while(!vector_block_iterator_end(root_set, iter)){
      REF *p_ref = (REF*)*iter;
      iter = vector_block_iterator_advance(root_set, iter);
      
      Partial_Reveal_Object *p_obj = read_slot(p_ref);
      assert(p_obj!=NULL);
      if(obj_mark_in_vtable(gc, p_obj))
        tracestack_push(p_obj);
    }
    root_set = pool_iterator_next(metadata->gc_rootset_pool);
  }
  /* put back the last trace_stack task */
  pool_put_entry(trace_pool, trace_stack);
  
  /* second step: iterate over the mark tasks and scan objects */
  /* get a task buf for the mark stack */
  trace_stack = free_task_pool_get_entry(metadata);
  
  Vector_Block *mark_task = pool_get_entry(trace_pool);
  
  while(mark_task){
    POINTER_SIZE_INT *iter = vector_block_iterator_init(mark_task);
    while(!vector_block_iterator_end(mark_task, iter)){
      Partial_Reveal_Object *p_obj = (Partial_Reveal_Object*)*iter;
      iter = vector_block_iterator_advance(mark_task, iter);
      
      trace_object(gc, p_obj);
    }
    /* run out one task, put back to the pool and grab another task */
    vector_stack_clear(mark_task);
    pool_put_entry(metadata->free_task_pool, mark_task);
    mark_task = pool_get_entry(trace_pool);
  }
  
  /* put back the last mark stack to the free pool */
  vector_stack_clear(trace_stack);
  pool_put_entry(metadata->free_task_pool, trace_stack);
  trace_stack = NULL;
  sync_pool_destruct(trace_pool);
  trace_pool = NULL;
  printf("Live obj in vtable marking: %d\n", live_obj_in_verify_marking);
  live_obj_in_verify_marking = 0;
}


#endif



#ifdef SSPACE_TIME

inline uint64 tsc()
{
  __asm _emit 0x0F;
  __asm _emit 0x31
}

#define CPU_HZ 3000000  // per ms

static uint64 gc_start_time;
static uint64 mark_start_time;
static uint64 sweep_start_time;
static uint64 merge_start_time;

void sspace_gc_time(GC *gc, Boolean before_gc)
{
  if(before_gc){
    gc_start_time = tsc();
    mark_start_time = gc_start_time;
  } else {
    uint64 end_time = tsc();
    assert(end_time > gc_start_time);
    printf("\n\nGC %d time: %dms\n\n", gc->num_collections, (end_time-gc_start_time) / CPU_HZ);
  }
}

void sspace_mark_time(Boolean before_mark)
{
  assert(before_mark == FALSE);
  if(before_mark){
    mark_start_time = tsc();
  } else {
    uint64 end_time = tsc();
    assert(end_time > mark_start_time);
    printf("\nMark time: %dms\n", (end_time-mark_start_time) / CPU_HZ);
    sweep_start_time = end_time;
  }
}

void sspace_sweep_time(Boolean before_sweep)
{
  assert(before_sweep == FALSE);
  if(before_sweep){
    sweep_start_time = tsc();
  } else {
    uint64 end_time = tsc();
    assert(end_time > sweep_start_time);
    printf("\nSweep time: %dms\n", (end_time-sweep_start_time) / CPU_HZ);
    merge_start_time = end_time;
  }
}

void sspace_merge_time(Boolean before_merge)
{
  assert(before_merge == FALSE);
  if(before_merge){
    merge_start_time = tsc();
  } else {
    uint64 end_time = tsc();
    assert(end_time > merge_start_time);
    printf("\nMerge time: %dms\n\n", (end_time-merge_start_time) / CPU_HZ);
  }
}

#endif
