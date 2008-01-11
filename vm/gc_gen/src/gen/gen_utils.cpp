/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
 * @author Xiao-Feng Li, 2006/10/05
 */

#include "gen.h"

#ifndef STATIC_NOS_MAPPING
void* nos_space_adjust(Space* nos, void* new_nos_boundary, POINTER_SIZE_INT new_nos_size)
{
  if(nos->collect_algorithm == MINOR_NONGEN_SEMISPACE_POOL || nos->collect_algorithm == MINOR_GEN_SEMISPACE_POOL)
    return sspace_heap_start_adjust((Sspace*)nos, new_nos_boundary, new_nos_size);
  else if( nos->collect_algorithm == MINOR_NONGEN_FORWARD_POOL )
    return fspace_heap_start_adjust((Fspace*)nos, new_nos_boundary, new_nos_size);  
  
  assert(0);
  return NULL;
}
#endif

POINTER_SIZE_INT mos_free_space_size(Space* mos)
{
  POINTER_SIZE_INT free_size = 0;
  if( mos->collect_algorithm != MAJOR_MARK_SWEEP )
    return mspace_free_space_size((Mspace*)mos);

  assert(0);
  return free_size; 
}

POINTER_SIZE_INT nos_free_space_size(Space* nos)
{
  POINTER_SIZE_INT free_size = 0;
  if(nos->collect_algorithm == MINOR_NONGEN_SEMISPACE_POOL || nos->collect_algorithm == MINOR_GEN_SEMISPACE_POOL)
    return sspace_free_space_size((Sspace*)nos);
  else if( nos->collect_algorithm == MINOR_NONGEN_FORWARD_POOL )
    return fspace_free_space_size((Fspace*)nos);

  assert(0);
  return free_size; 
 
}

POINTER_SIZE_INT mos_used_space_size(Space* mos)
{
  POINTER_SIZE_INT free_size = 0;
  if( mos->collect_algorithm != MAJOR_MARK_SWEEP )
    return mspace_used_space_size((Mspace*)mos);

  assert(0);
  return free_size; 
}

POINTER_SIZE_INT nos_used_space_size(Space* nos)
{
  POINTER_SIZE_INT free_size = 0;
  if(nos->collect_algorithm == MINOR_NONGEN_SEMISPACE_POOL || nos->collect_algorithm == MINOR_GEN_SEMISPACE_POOL)
    return sspace_used_space_size((Sspace*)nos);
  else if( nos->collect_algorithm == MINOR_NONGEN_FORWARD_POOL )
    return fspace_used_space_size((Fspace*)nos);

  assert(0);
  return free_size; 
 
}
