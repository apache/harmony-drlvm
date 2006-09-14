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
 * @author Vladimir Nenashev
 * @version $Revision$
 */  


#ifndef __STACK_DUMP_H_
#define __STACK_DUMP_H_

#include "vm_core_types.h"
#include "jni.h"

/**
 * Prints a stack trace using given register context for current thread
 */
void st_print_stack(Registers* regs);

#endif //!__STACK_DUMP_H_
