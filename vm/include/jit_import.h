/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements. See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License. You may obtain a copy of the License at
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
 * These are the functions that a JIT built as a DLL may call.
 */

#ifndef _JIT_IMPORT_H
#define _JIT_IMPORT_H

#include <stdlib.h>

#include "jit_export.h"
#include "open/types.h"
#include "open/vm.h"
#include "jit_import_rt.h"
#include "vm_core_types.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef void *Method_Iterator;

/** 
 * @name Direct call-related functions
 */
//@{

/**
 * These functions allow a JIT to be notified whenever a VM data structure changes that 
 * would require code patching or recompilation. 
 *
 * Called by a JIT in order to be notified whenever the given class (or any of 
 * its subclasses?) is extended. The <code>callback_data</code> pointer will 
 * be passed back to the JIT during the callback. The callback function is 
 * <code>JIT_extended_class_callback</code>.
 */
VMEXPORT void vm_register_jit_extended_class_callback(JIT_Handle jit, Class_Handle clss, 
                                                        void *callback_data);

/**
 * Called by a JIT in order to be notified whenever the given method is 
 * overridden by a newly loaded class. The <code>callback_data</code> pointer 
 * will be passed back to the JIT during the callback. The callback function is 
 * <code>JIT_overridden_method_callback</code>.
 */
VMEXPORT void vm_register_jit_overridden_method_callback(JIT_Handle jit, Method_Handle method,
                                                           void *callback_data);

/**
 * Called by a JIT in order to be notified whenever the vtable entries for the 
 * given method are changed. This could happen, e.g., when a method is first 
 * compiled, or when it is recompiled. The <code>callback_data</code> pointer 
 * will be passed back to the JIT during the callback. The callback method is 
 * <code>JIT_recompiled_method_callback</code>.
 */
VMEXPORT void vm_register_jit_recompiled_method_callback(JIT_Handle jit, Method_Handle method,
                                                           void *callback_data);

/**
 * Called by a JIT to have the VM replace a section of executable code in a 
 * thread-safe fashion. This function does not synchronize the I- or D-caches. 
 * It may be a lot cheaper to batch up the patch requests, so we may need to 
 * extend this interface.
 */
VMEXPORT void vm_patch_code_block(Byte *code_block, Byte *new_code, size_t size);

/**
 * Called by a JIT to have the VM recompile a method using the specified JIT. After 
 * recompilation, the corresponding vtable entries will be updated, and the necessary 
 * callbacks to <code>JIT_recompiled_method_callback</code> will be made. It is a 
 * requirement that the method has not already been compiled by the given JIT; 
 * this means that multiple instances of a JIT may need to be active at the same time. 
 */

VMEXPORT void vm_recompile_method(JIT_Handle jit, Method_Handle method);

/** 
 * Called by a JIT to have VM synchronously (in the same thread) compile a method
 * It is a requirement that JIT calls this routine only during compilation of 
 * other method, not during run-time.
 */

VMEXPORT JIT_Result vm_compile_method(JIT_Handle jit, Method_Handle method);

//@}

///////////////////////////////////////////////////////
// begin method memory allocation-related functions.
/////////////////////////////////

/**
 * Retrieve the memory block allocated earlier by
 * method_allocate_code_block().
 * A triple <code><method, jit, id></code> uniquely identifies a 
 * code block.
 */

VMEXPORT Byte *method_get_code_block_addr_jit_new(Method_Handle method,
                                                   JIT_Handle j,
                                                   int id);

/**
 * Get the size of the memory block allocated earlier by
 * method_allocate_code_block().
 * A triple <code><method, jit, id></code> uniquely identifies a 
 * code block.
 */

VMEXPORT unsigned method_get_code_block_size_jit_new(Method_Handle method,
                                                      JIT_Handle j,
                                                      int id);

//////////////////////////////
// end method-related functions.
///////////////////////////////////////////////////////

/** 
 * @name Resolution-related functions
 */
//@{

/**
 * Resolve a class.
 *
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 */

VMEXPORT Class_Handle 
vm_resolve_class(Compile_Handle h, Class_Handle ch, unsigned idx);

/**
 * Resolve a class and provide error checking if the class cannot have an
 * instance, i.e. it is abstract (or is an interface class).
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 */

VMEXPORT Class_Handle 
vm_resolve_class_new(Compile_Handle h, Class_Handle c, unsigned index);

/**
 * Resolve a reference to a non-static field.
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 * Used for getfield and putfield in JVM.
 */

VMEXPORT Field_Handle 
resolve_nonstatic_field(Compile_Handle h, Class_Handle ch, unsigned idx, unsigned putfield);

/**
 * Resolve constant pool reference to a static field.
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 * Used for getstatic and putstatic in JVM.
 */

VMEXPORT Field_Handle
resolve_static_field(Compile_Handle h, Class_Handle ch, unsigned idx, unsigned putfield);

/**
 * Resolve a method. Same as resolve_method() but the VM checks 
 * that the method can be used for a virtual dispatch.
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 */

VMEXPORT Method_Handle 
resolve_virtual_method(Compile_Handle h, Class_Handle c, unsigned index);

/**
 * Resolve a method. Same as resolve_method() but the VM checks 
 * that the method is static (i.e. it is not an instance method).
 * The <code>idx</code> parameter is interpreted as a constant pool index for 
 * JVM.
 */

VMEXPORT Method_Handle 
resolve_static_method(Compile_Handle h, Class_Handle c, unsigned index);

/** 
 * Resolve a method. Same as resolve_method() but the VM checks 
 * that the method is declared in an interface type.
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 */

VMEXPORT Method_Handle 
resolve_interface_method(Compile_Handle h, Class_Handle c, unsigned index);

//@}
/** @name Miscellaneous functions
 */
//@{

/**
 * @return The JIT handle for a the current compilation. 
 *
 * The VM keeps track of the JIT that was invoked for and can return this value at 
 * any point during the compilation.
 * (? 20030314) Does the <code>method_</code> prefix really make sense here? 
 * Perhaps we should rename this function?
 */

VMEXPORT JIT_Handle method_get_JIT_id(Compile_Handle h);

//@}
/** @name Experimental functions
 */
//@{

/**
 * These functions are currently not part of the official interface,
 * although they may be promoted in some form in the future.
 *
 * @return <code>TRUE</code> if the VM's functionality for monitorenter 
 *         and monitorexit may be inlined by the JIT; otherwise, <code>FALSE</code>.
 *         
 * If <code>TRUE</code> is returned, then the output arguments will be 
 * filled in with the synchronization parameters.
 *
 * @param thread_id_register  - the register number that holds the thread ID which
 *                              is used to identify the locking thread
 * @param sync_header_offset  - the offset in bytes of the synchronization header
 *                              from the start of the object
 * @param sync_header_width   - the width in bytes of the synchronization header
 * @param lock_owner_offset   - the offset in bytes of the lock owner field from
 *                              the start of the object
 * @param lock_owner_width    - the width in bytes of the lock owner field in the
 *                              synchronization header
 * @param jit_clears_ccv      - <code>TRUE</code> if the JIT-generated code needs 
 *                              to clear the <code>ar.ccv</code> register, 
 *                              <code>FALSE</code> if the VM ensures it is 
 *                              already cleared
 *
 * @note This is useful only for <code>monitorenter/monitorexit</code>, but not
 *       <code>monitorenter_static/monitorexit_static</code>, since the JIT doesn't 
 *       know how to map the <code>class_handle</code> to an object.
 */
VMEXPORT Boolean jit_may_inline_object_synchronization(unsigned *thread_id_register,
                                                        unsigned *sync_header_offset,
                                                        unsigned *sync_header_width,
                                                        unsigned *lock_owner_offset,
                                                        unsigned *lock_owner_width,
                                                        Boolean  *jit_clears_ccv);

typedef enum CallingConvention {
    CC_Vm,
    CC_Jrockit,
    CC_Rotor,
    CC_Stdcall,
    CC_Cdecl
} CallingConvention;

VMEXPORT CallingConvention vm_managed_calling_convention();

#ifdef __cplusplus
}
#endif

#endif // _JIT_IMPORT_H
