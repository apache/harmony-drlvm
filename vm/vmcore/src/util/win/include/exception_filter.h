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
 * @author Euguene Ostrovsky
 * @version $Revision: 1.1.2.1.4.5 $
 */  

#ifndef nt_exception_filter_h
#define nt_exception_filter_h

#include "platform_lowlevel.h"
#include "vm_core_types.h"

#ifdef __cplusplus
extern "C" {
#endif

LONG NTAPI vectored_exception_handler(LPEXCEPTION_POINTERS nt_exception);

// Internal exception handler
// Is used when vectored_exception_handler is assembler wrapper
LONG NTAPI vectored_exception_handler_internal(LPEXCEPTION_POINTERS nt_exception);

// Function to throw exception
void __cdecl c_exception_handler(Registers* regs, Class* exn_class, bool in_java);

// exception catch callback to restore stack after Stack Overflow Error
void __cdecl exception_catch_callback_wrapper();
// exception catch support for JVMTI
void __cdecl jvmti_exception_catch_callback_wrapper();

#ifdef __cplusplus
} // extern "C"
#endif


// Prints register state
void print_reg_state(Registers* regs);


#endif // nt_exception_filter_h

