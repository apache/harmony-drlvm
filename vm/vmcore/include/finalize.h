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
 * @author Intel, Salikh Zakirov
 * @version $Revision: 1.1.2.1.4.3 $
 */  






#ifndef _FINALIZE_H_
#define _FINALIZE_H_

#include "platform.h"

#include "open/types.h"

#ifdef USE_GC_STATIC
extern int running_finalizers_deferred;
#endif

#ifndef USE_GC_STATIC
VMEXPORT
#endif
void vm_run_pending_finalizers();
int vm_do_finalization(int quantity);
int vm_get_finalizable_objects_quantity();

#ifndef USE_GC_STATIC
VMEXPORT
#endif
void vm_enumerate_objects_to_be_finalized();
void vm_enumerate_references_to_enqueue();


void vm_enqueue_references();

#endif
