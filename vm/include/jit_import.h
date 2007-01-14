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
 * 
 * @sa vm_clone_jit
 */

VMEXPORT void vm_recompile_method(JIT_Handle jit, Method_Handle method);

/** 
 * Called by a JIT to have VM synchronously (in the same thread) compile a method
 * It is a requirement that JIT calls this routine only during compilation of 
 * other method, not during run-time.
 */

VMEXPORT JIT_Result vm_compile_method(JIT_Handle jit, Method_Handle method);

/** 
 * Creates and returns a new instance of the given JIT. The new JIT's 
 * implementation of <code>JIT_init_with_data</code> is invoked with the 
 * <code>jit_data</code> argument.
 */

VMEXPORT JIT_Handle vm_clone_jit(JIT_Handle jit, void *jit_data);

//@}
/** @name Exception-related compile-time functions
 */
//@{

/**
 * @return The number of exception handlers defined for this method in the
 *         bytecodes.
 */

VMEXPORT unsigned method_get_num_handlers(Method_Handle method);

/**
 * Deprecated.
 * 
 * @sa method_get_handler_info
 */
VMEXPORT void method_get_handler_info_full(Method_Handle method,
                                            unsigned handler_id,
                                            unsigned *begin_offset,
                                            unsigned *end_offset,
                                            unsigned *handler_offset,
                                            unsigned *handler_len,
                                            unsigned *filter_offset,
                                            unsigned *handler_class_index);

/**
 * This is a simpler version of method_get_handler_info_full() that works
 * only for JVM.
 */
VMEXPORT void method_get_handler_info(Method_Handle method,
                                       unsigned handler_id,
                                       unsigned *begin_offset,
                                       unsigned *end_offset,
                                       unsigned *handler_offset,
                                       unsigned *handler_class_index);

/**
 * Deprecated.
 *
 * @return For Java methods, it always returns <code>FALSE</code> since JVM
 *         handlers do not have a finally clause.
 */
VMEXPORT Boolean method_handler_has_finally(Method_Handle method,
                                           unsigned handler_id);

/**
 * Deprecated.
 * 
 * @return For Java methods, it always returns <code>FALSE</code> since JVM
 * handlers do not have a filters.
 */

VMEXPORT Boolean method_handler_has_filter(Method_Handle method,
                                            unsigned handler_id);

/**
 * Deprecated.
 * 
 * @return For Java methods, it always returns <code>FALSE</code> since JVM
 *         handlers do not have a fault clause.
 */

VMEXPORT Boolean method_handler_has_fault(Method_Handle method,
                                           unsigned handler_id);

/**
 * Set the number of exception handlers in the code generated by the JIT 
 * <code>j</code> for a given method. The JIT must then 
 * call method_set_target_handler_info()
 * for each of the num_handlers exception handlers.
 */

VMEXPORT void method_set_num_target_handlers(Method_Handle method,
                                              JIT_Handle j,
                                              unsigned num_handlers);

/**
 * Set the information about an exception handler in the code generated by
 * the JIT.
 */
VMEXPORT void method_set_target_handler_info(Method_Handle method,
                                              JIT_Handle j,
                                              unsigned      eh_number,
                                              void         *start_ip,
                                              void         *end_ip,
                                              void         *handler_ip,
                                              Class_Handle  catch_cl,
                                              Boolean       exc_obj_is_dead);


//@}
/** @name Method-related functions
 */
//@{

/**
 * @ingroup bytecodes
 */
///////////////////////////////////////////////////////
// begin method-related functions: bytecodes
///////////////////////////////////

/**
 * Get a pointer to the buffer containing the bytecodes for this method.
 * Bytecodes are either JVML instructions or CIL instructions.
 */
 VMEXPORT const Byte *method_get_byte_code_addr(Method_Handle method);

/**
 * Size if the buffer returned by method_get_byte_code_addr().
 */

VMEXPORT size_t method_get_byte_code_size(Method_Handle method);

/**
 * Maximum depth of the evaluation stack in this method.
 */

VMEXPORT unsigned method_get_max_stack(Method_Handle method);
//////////////////////////
// end method-related functions: bytecodes
///////////////////////////////////////////////////////


///////////////////////////////////////////////////////
// begin method-related functions: compiled code
////////////////////////////
/**
 * @return The address where the code pointer for a given method is.
 * 
 * A simple JIT that doesn't support recompilation (see e.g. 
 * <code>vm_register_jit_recompiled_method_callback</code>) can only 
 * generate code with indirect branches through the address provided 
 * by method_get_indirect_address().
 */

VMEXPORT void *method_get_indirect_address(Method_Handle method);

/**
 * @return The offset in bytes from the start of the vtable to the entry for
 *         a given method.
 */
VMEXPORT unsigned method_get_offset(Method_Handle method);
///////////////////////////
// end method-related functions: compiled code
///////////////////////////////////////////////////////


///////////////////////////////////////////////////////
// begin method memory allocation-related functions.
/////////////////////////////////

/**
 * Allocate the "read-write" data block for this method. This memory block
 * cannot be retrieved later. The intention is to use the data block for data
 * that may be needed during the program execution (e.g. tables for
 * switch statements).
 *
 * Separation of data allocated by method_allocate_data_block() and
 * method_allocate_info_block() may help improve locality of 
 * references to data accessed during execution of compiled code and data 
 * accessed during stack uwinding.
 *
 * @sa method_allocate_info_block
 */

VMEXPORT Byte *method_allocate_data_block(Method_Handle method,
                                           JIT_Handle j,
                                           size_t size,
                                           size_t alignment);

/**
 * Allocated a "read-only" data block.
 *
 * (? 20030314) This function is deprecated. In all new code, use
 * method_allocate_data_block() only. At some point, we 
 * will revisit this interface to have more control over the layout 
 * of various memory blocks allocated by the VM.
 */

VMEXPORT Byte *method_allocate_jit_data_block(Method_Handle method,
                                               JIT_Handle j,
                                               size_t size,
                                               size_t alignment);


/**
 * The following values should be used as the "heat" argument for 
 * calls like method_allocate_code_block() or 
 * <code>malloc_fixed_code_for_jit()</code>.
 */

#define CODE_BLOCK_HEAT_COLD 0
#define CODE_BLOCK_HEAT_DEFAULT 1
#define CODE_BLOCK_HEAT_MAX 20

/**
 * @sa method_allocate_code_block
 */

typedef enum Code_Allocation_ActionEnum {
    CAA_Simulate,
    CAA_Allocate
}Code_Allocation_Action;
 

/**
 * This function allows allocation of multiple chunks of code with different
 * heat values. The JIT is responsible for specifying ids that are unique
 * within the same method.
 * The first instruction of the chunk with <code>id=0</code> is the entry point 
 * of the method.
 * 
 * Deprecated.
 *
 * @return If the <code>CAA_Allocate</code> argument is specified, memory is 
 *         allocated and a pointer to it is returned. If the 
 *         <code>CAA_Simulate</code> argument is specified, no memory is
 *         allocated - the same as pass parameter size = 0 - function returns 
 *         only current address for allocation in pool but no memory is allocated.  
 */

VMEXPORT Byte *
method_allocate_code_block(Method_Handle m,
                           JIT_Handle j,
                           size_t size,
                           size_t alignment,
                           unsigned heat,
                           int id,
                           Code_Allocation_Action action);


VMEXPORT void
method_set_relocatable(Method_Handle m, JIT_Handle j, NativeCodePtr code_address, Boolean is_relocatable);


/**
 * Allocate an info block for this method. An info block can be later
 * retrieved by the JIT. The JIT may for instance store GC maps for
 * root set enumeration and stack unwinding in the onfo block.
 *
 * @sa method_allocate_data_block
 */
VMEXPORT Byte *method_allocate_info_block(Method_Handle method,
                                           JIT_Handle j,
                                           size_t size);


/**
 * Retrieve the memory block allocated earlier by 
 * method_allocate_code_block().
 * A pair <code><method, jit></code> uniquely identifies a code block.
 */

VMEXPORT Byte *method_get_code_block_addr_jit(Method_Handle method,
                                               JIT_Handle j);

/**
 * Get the size of the memory block allocated earlier by
 * method_allocate_code_block().
 */

VMEXPORT unsigned method_get_code_block_size_jit(Method_Handle method,
                                                  JIT_Handle j);

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

/**
 * Retrieve the memory block allocated earlier by 
 * method_allocate_info_block().
 * A pair <code><method, jit></code> uniquely identifies a JIT info block.
 */ 

VMEXPORT Byte *method_get_info_block_jit(Method_Handle method,
                                          JIT_Handle j);

/**
 * Get the size of the memory block allocated earlier by
 * method_allocate_info_block().
 */

VMEXPORT unsigned method_get_info_block_size_jit(Method_Handle method,
                                                  JIT_Handle j);

///////////////////////////////////////////////////////
// begin functions for iterating over methods compiled by a given JIT.
/////////////////////////////////////

#define METHOD_JIT_ITER_END 0

/**
 * Here are the obvious three functions to iterate over all methods
 * compiled by a given JIT.
 */

VMEXPORT Method_Iterator method_get_first_method_jit(JIT_Handle j);
VMEXPORT Method_Iterator method_get_next_method_jit(Method_Iterator mi);
VMEXPORT Method_Handle   method_get_method_jit(Method_Iterator mi);

/////////////////////////////////////////
// end functions for iterating over methods compiled by a given JIT.
///////////////////////////////////////////////////////
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
 * Resolve a method.
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 */ 
VMEXPORT Method_Handle 
resolve_method(Compile_Handle h, Class_Handle ch, unsigned idx);


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
 * @return A UTF8 representation of a string declared in a class.
 *
 * The <code>idx</code> parameter is interpreted as a constant pool 
 * index for JVM.
 * <code>class_get_const_string</code> is generally only for JIT internal use,
 * e.g. printing a string pool constant in a bytecode disassembler.
 * The resulting const char* should of course not be inserted into
 * the jitted code.
 */

VMEXPORT const char *class_get_const_string(Class_Handle ch, unsigned idx);


/**
 * @return The address where the interned version of the string
 *         is stored. 
 * 
 * Calling <code>class_get_const_string_intern_addr</code> has
 * a side-effect of interning the string, so that the JIT can
 * load a reference to the interned string without checking if
 * it is null.
 */

VMEXPORT void *class_get_const_string_intern_addr(Class_Handle ch, unsigned idx);

/**
 * @return The type of a compile-time constant.
 *
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 */

 VMEXPORT VM_Data_Type class_get_const_type(Class_Handle ch, unsigned idx);

/**
 * @return The signature for field or (interface) method in constant pool entry.
 * The <code>cp_index</code> parameter is interpreted as a constant pool index 
 * for JVM.
 */

VMEXPORT const char* class_get_cp_entry_signature(Class_Handle src_class,
                                                  unsigned short cp_index);

/**
 * @return The data type for field in constant pool entry.
 * 
 * The <code>cp_index</code> parameter is interpreted as a constant pool index 
 * for JVM.
 */

VMEXPORT VM_Data_Type class_get_cp_field_type(Class_Handle src_class,
                                              unsigned short cp_index);

/**
 * @return A pointer to the location where the constant is stored.
 *
 * The <code>idx</code> parameter is interpreted as a constant pool index for JVM.
 * This function shouldn't be called for constant strings. Instead, either:<br>
 * <ul><li>The jitted code should get the string object at runtime by calling
 *         <code>VM_RT_LDC_STRING</code>, or
 *     <li>Use class_get_const_string_intern_addr().
 *</ul>
 */

VMEXPORT const void  *class_get_const_addr(Class_Handle ch, unsigned idx);

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
