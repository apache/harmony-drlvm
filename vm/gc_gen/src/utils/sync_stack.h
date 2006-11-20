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
 
#ifndef _SYNC_STACK_H_
#define _SYNC_STACK_H_

typedef struct Sync_Stack{
  unsigned int* top; /* pointing to the first filled entry */
  unsigned int* cur; /* pointing to the current accessed entry */
  unsigned int* bottom; /* pointing to the pos right after the last entry */
  unsigned int entries[1];
}Sync_Stack;

inline Sync_Stack* sync_stack_init(unsigned int num_entries)
{
  unsigned int size = ((num_entries-1) << 2) + sizeof(Sync_Stack);
  Sync_Stack* stack = (Sync_Stack*)STD_MALLOC(size);
  memset(stack, 0, size);
  stack->bottom = &(stack->entries[num_entries]);
  stack->top = stack->bottom; 
  return stack;
}

inline void sync_stack_destruct(Sync_Stack* stack)
{
  STD_FREE(stack);
  return;  
}

inline void sync_stack_iterate_init(Sync_Stack* stack)
{
  stack->cur = stack->top;
  return;
}

inline unsigned int sync_stack_iterate_next(Sync_Stack* stack)
{
  unsigned int* entry = stack->cur;
  unsigned int* new_entry = entry + 1;
  unsigned int* last_entry = stack->bottom - 1;
  while ( entry <= last_entry ){
    unsigned int* temp = (unsigned int*)atomic_casptr((volatile void**)&stack->cur, new_entry, entry);
    if(temp == entry){ /* got it */  
      return *entry;
    }
    entry = stack->cur;
    new_entry = entry + 1;    
  }  
  return 0;
}

inline unsigned int sync_stack_pop(Sync_Stack* stack)
{
  volatile unsigned int* entry = stack->top;
  unsigned int* new_entry = stack->top + 1;
  unsigned int* last_entry = stack->bottom - 1;
  while ( entry <= last_entry ){
    unsigned int* temp = (unsigned int*)atomic_casptr((volatile void**)&stack->top, new_entry, (const void*)entry);
    if(temp == entry){ /* got it */ 
      while(!*entry); /* has to have something */
      unsigned int result = *entry;
      *entry = NULL;  /* put NULL into it */
      return result;
    }
    entry = (volatile unsigned int*)stack->top;
    new_entry = (unsigned int*)(entry + 1);    
  }  
  return 0;
}

inline Boolean sync_stack_push(Sync_Stack* stack, unsigned int value)
{
  unsigned int* entry = stack->top;
  volatile unsigned int* new_entry = stack->top - 1;
  unsigned int* first_entry = stack->entries;
  while ( entry >= first_entry ){
    unsigned int* temp = (unsigned int*)atomic_casptr((volatile void**)&stack->top, (void*)new_entry, entry);
    if(temp == entry){ /* got it */  
      while(*new_entry); /* has to be NULL before filled */
      *new_entry = value;
      return TRUE;
    }
    entry = stack->top;
    new_entry = entry - 1;
  }
  return FALSE;
}

/* it does not matter whether this is atomic or not, because
   it is only invoked when there is no contention or only for rough idea */
inline unsigned int stack_entry_count(Sync_Stack* stack)
{
  return (stack->bottom - stack->top);
}

#endif /* _SYNC_STACK_H_ */
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
 
#ifndef _SYNC_STACK_H_
#define _SYNC_STACK_H_

typedef struct Sync_Stack{
  unsigned int* top; /* pointing to the first filled entry */
  unsigned int* cur; /* pointing to the current accessed entry */
  unsigned int* bottom; /* pointing to the pos right after the last entry */
  unsigned int entries[1];
}Sync_Stack;

inline Sync_Stack* sync_stack_init(unsigned int num_entries)
{
  unsigned int size = ((num_entries-1) << 2) + sizeof(Sync_Stack);
  Sync_Stack* stack = (Sync_Stack*)STD_MALLOC(size);
  memset(stack, 0, size);
  stack->bottom = &(stack->entries[num_entries]);
  stack->top = stack->bottom; 
  return stack;
}

inline void sync_stack_destruct(Sync_Stack* stack)
{
  STD_FREE(stack);
  return;  
}

inline void sync_stack_iterate_init(Sync_Stack* stack)
{
  stack->cur = stack->top;
  return;
}

inline unsigned int sync_stack_iterate_next(Sync_Stack* stack)
{
  unsigned int* entry = stack->cur;
  unsigned int* new_entry = entry + 1;
  unsigned int* last_entry = stack->bottom - 1;
  while ( entry <= last_entry ){
    unsigned int* temp = (unsigned int*)atomic_casptr((volatile void**)&stack->cur, new_entry, entry);
    if(temp == entry){ /* got it */  
      return *entry;
    }
    entry = stack->cur;
    new_entry = entry + 1;    
  }  
  return 0;
}

inline unsigned int sync_stack_pop(Sync_Stack* stack)
{
  volatile unsigned int* entry = stack->top;
  unsigned int* new_entry = stack->top + 1;
  unsigned int* last_entry = stack->bottom - 1;
  while ( entry <= last_entry ){
    unsigned int* temp = (unsigned int*)atomic_casptr((volatile void**)&stack->top, new_entry, (const void*)entry);
    if(temp == entry){ /* got it */ 
      while(!*entry); /* has to have something */
      unsigned int result = *entry;
      *entry = NULL;  /* put NULL into it */
      return result;
    }
    entry = (volatile unsigned int*)stack->top;
    new_entry = (unsigned int*)(entry + 1);    
  }  
  return 0;
}

inline Boolean sync_stack_push(Sync_Stack* stack, unsigned int value)
{
  unsigned int* entry = stack->top;
  volatile unsigned int* new_entry = stack->top - 1;
  unsigned int* first_entry = stack->entries;
  while ( entry >= first_entry ){
    unsigned int* temp = (unsigned int*)atomic_casptr((volatile void**)&stack->top, (void*)new_entry, entry);
    if(temp == entry){ /* got it */  
      while(*new_entry); /* has to be NULL before filled */
      *new_entry = value;
      return TRUE;
    }
    entry = stack->top;
    new_entry = entry - 1;
  }
  return FALSE;
}

/* it does not matter whether this is atomic or not, because
   it is only invoked when there is no contention or only for rough idea */
inline unsigned int stack_entry_count(Sync_Stack* stack)
{
  return (stack->bottom - stack->top);
}

#endif /* _SYNC_STACK_H_ */
