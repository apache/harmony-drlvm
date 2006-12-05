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

typedef struct Node{
  Node* next;  
}Node;

typedef struct Sync_Stack{
  Node* top; /* pointing to the first filled entry */
  Node* cur; /* pointing to the current accessed entry, only for iterator */
}Sync_Stack;

inline Sync_Stack* sync_stack_init()
{
  unsigned int size = sizeof(Sync_Stack);
  Sync_Stack* stack = (Sync_Stack*)STD_MALLOC(size);
  memset(stack, 0, size);
  stack->cur = NULL;
  stack->top = NULL; 
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

inline Node* sync_stack_iterate_next(Sync_Stack* stack)
{
  Node* entry = stack->cur;
  while ( entry != NULL ){
    Node* new_entry = entry->next;
    Node* temp = (Node*)atomic_casptr((volatile void**)&stack->cur, new_entry, entry);
    if(temp == entry){ /* got it */  
      return entry;
    }
    entry = stack->cur;
  }  
  return NULL;
}

inline Node* sync_stack_pop(Sync_Stack* stack)
{
  Node* entry = stack->top;
  while( entry != NULL ){
    Node* new_entry = entry->next;
    Node* temp = (Node*)atomic_casptr((volatile void**)&stack->top, new_entry, entry);
    if(temp == entry){ /* got it */ 
      entry->next = NULL;
      return entry;
    }
    entry = stack->top;
  }  
  return 0;
}

inline Boolean sync_stack_push(Sync_Stack* stack, Node* node)
{
  Node* entry = stack->top;
  node->next = entry;
  
  while( TRUE ){
    Node* temp = (Node*)atomic_casptr((volatile void**)&stack->top, node, entry);
    if(temp == entry){ /* got it */  
      return TRUE;
    }
    entry = stack->top;
    node->next = entry;
  }
  /* never comes here */
  return FALSE;
}

/* it does not matter whether this is atomic or not, because
   it is only invoked when there is no contention or only for rough idea */
inline Boolean stack_is_empty(Sync_Stack* stack)
{
  return (stack->top == NULL);
}

#endif /* _SYNC_STACK_H_ */
