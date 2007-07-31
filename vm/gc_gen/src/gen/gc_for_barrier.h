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

#ifndef _GC_FOR_BARRIER_H_
#define _GC_FOR_BARRIER_H_

#include "../jni/java_support.h"

extern Boolean gen_mode;

inline Boolean gc_is_gen_mode()
{  return gen_mode; }

inline void gc_enable_gen_mode()
{  
  gen_mode = TRUE;
  HelperClass_set_GenMode(TRUE);
}

inline void gc_disable_gen_mode()
{  
  gen_mode = FALSE; 
  HelperClass_set_GenMode(FALSE);
}

inline void gc_set_gen_mode(Boolean status)
{
  gen_mode = status; 
  HelperClass_set_GenMode(status);   
}

#endif /* _GC_FOR_BARRIER_H_ */

