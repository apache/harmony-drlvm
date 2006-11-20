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
 * @author Xiao-Feng Li, 2006/10/25
 */
 
#ifndef _SYNC_POOL_H_
#define _SYNC_POOL_H_

#include "sync_stack.h"

typedef Sync_Stack Pool;

inline Pool* sync_pool_create(unsigned int size){ return sync_stack_init(size); }
inline void sync_pool_destruct(Pool* pool){ sync_stack_destruct(pool); }

inline Boolean pool_is_empty(Pool* pool){ return stack_entry_count(pool)==0;}
inline Vector_Block* pool_get_entry(Pool* pool)
{ 
  Vector_Block* block = (Vector_Block*)sync_stack_pop(pool);
  assert( !block || (block->start == (unsigned int*)block->entries) );
  assert( !block || (block->head <= block->tail && block->tail <= block->end));
 
  return block;
}

inline void pool_put_entry(Pool* pool, void* value){ assert(value); Boolean ok = sync_stack_push(pool, (unsigned int)value); assert(ok);}

inline void pool_iterator_init(Pool* pool){ sync_stack_iterate_init(pool);}
inline Vector_Block* pool_iterator_next(Pool* pool){ return (Vector_Block*)sync_stack_iterate_next(pool);}

#endif /* #ifndef _SYNC_POOL_H_ */

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
 * @author Xiao-Feng Li, 2006/10/25
 */
 
#ifndef _SYNC_POOL_H_
#define _SYNC_POOL_H_

#include "sync_stack.h"

typedef Sync_Stack Pool;

inline Pool* sync_pool_create(unsigned int size){ return sync_stack_init(size); }
inline void sync_pool_destruct(Pool* pool){ sync_stack_destruct(pool); }

inline Boolean pool_is_empty(Pool* pool){ return stack_entry_count(pool)==0;}
inline Vector_Block* pool_get_entry(Pool* pool)
{ 
  Vector_Block* block = (Vector_Block*)sync_stack_pop(pool);
  assert( !block || (block->start == (unsigned int*)block->entries) );
  assert( !block || (block->head <= block->tail && block->tail <= block->end));
 
  return block;
}

inline void pool_put_entry(Pool* pool, void* value){ assert(value); Boolean ok = sync_stack_push(pool, (unsigned int)value); assert(ok);}

inline void pool_iterator_init(Pool* pool){ sync_stack_iterate_init(pool);}
inline Vector_Block* pool_iterator_next(Pool* pool){ return (Vector_Block*)sync_stack_iterate_next(pool);}

#endif /* #ifndef _SYNC_POOL_H_ */

