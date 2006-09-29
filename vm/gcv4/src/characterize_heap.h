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
 * @author Intel, Salikh Zakirov
 * @version $Revision: 1.1.2.1.4.3 $
 */  


#ifndef _characterize_heap_H_
#define _characterize_heap_H_


extern bool characterize_heap;

#if (!defined(PLATFORM_POSIX) && defined(_DEBUG))

extern void heapTraceInit();
extern void heapTraceBegin(bool before_gc);
extern void heapTraceAllocation(Partial_Reveal_Object* p_object, int size);
extern void heapTraceObject(Partial_Reveal_Object* p_object, int size);
extern void heapTraceEnd(bool before_gc);
extern void heapTraceFinalize();
extern bool is_long_lived (Partial_Reveal_Object *p_obj);

#else

#define heapTraceInit()
#define heapTraceBegin(before_gc)
#define heapTraceEnd(before_gc)
#define heapTraceFinalize()
#define heapTraceAllocation(p_object, size)
#define heapTraceObject(p_object, size)
#define is_long_lived (p_obj)

#endif



#endif // _characterize_heap_H_
