/*
 *  Copyright 2006 The Apache Software Foundation or its licensors, as applicabl
e.
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
 * @author Intel, Pavel Pervov
 * @version $Revision: 1.7 $
 */
#ifndef _VM_METHOD_ACCESS_H
#define _VM_METHOD_ACCESS_H

/**
 * @file
 * Part of Class Support interface related to retrieving different
 * properties of methods contained in class.
 */

/**
 * Returns the method name.
 *
 * @param method - the method handle
 *
 * @return Method name bytes.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 */
const char*
method_get_name(Open_Method_Handle method);

/**
 * Returns the method descriptor.
 *
 * @param method - the method handle
 *
 * @return Method descriptor bytes.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 */
const char*
method_get_descriptor(Open_Method_Handle method);

/**
 * Returns the signature that can be used to iterate over the agruments of the given method
 * and to query the type of the method result.
 *
 * @param method - the method handle
 *
 * @return The method signature.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 */
Method_Signature_Handle
method_get_signature(Method_Handle method);

/**
 * Returns the class where the given method is declared.
 *
 * @param method - the method handle
 *
 * @return The name of the class where the method is declared.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 */
Open_Class_Handle
method_get_class(Open_Method_Handle method);

/**
 * Returns access flags for the given method.
 *
 * @param method - the method handle
 *
 * @return Access flags.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 */
unsigned short
method_get_flags(Open_Method_Handle method);

/**
 * Checks whether the given method is protected.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the given method is protected; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 **/
unsigned
method_is_protected(Open_Method_Handle method);

/**
 *  Checks whether the given method is private.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the given method is private; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 * @ingroup Extended 
 */
Boolean
method_is_private(Open_Method_Handle method);

/**
 *  Checks whether the given method is public.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the given method is public; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 * @ingroup Extended
 */
Boolean
method_is_public(Open_Method_Handle method);

/**
 *  Checks whether the given method is static.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the given method is static; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 * @ingroup Extended 
 */
Boolean
method_is_static(Open_Method_Handle method);

/**
 *  Checks whether the given method is native.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the given method is native; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
* @ingroup Extended 
 */
Boolean
method_is_native(Open_Method_Handle method);

/**
 *  Checks whether the given method is synchronized.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the given method is synchronized; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
* @ingroup Extended 
 */
Boolean
method_is_synchronized(Open_Method_Handle method);

/**
 *  Checks whether the given method is final.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the given method is final; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
* @ingroup Extended 
 */
Boolean
method_is_final(Open_Method_Handle method);

/**
 *  Checks whether the given method is abstract.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the given method is abstract; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
* @ingroup Extended 
 */
Boolean
method_is_abstract(Open_Method_Handle method);

/**
 *  Checks whether the given method is strict.
 *
 * Java* methods can have a flag set to indicate that floating point operations
 * must be performed in the strict mode.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the <code>ACC_STRICT</code> flag is set for 
 *                the Java* method and <code>FALSE</code> if otherwise.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
* @ingroup Extended 
 */
Boolean
method_is_strict(Open_Method_Handle method);

/**
 *  Checks whether the given method has been overridden in a subclass.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the method has been overriden and <code>FALSE</code> otherwise.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 *
 * @note If this function returns <code>FALSE</code>, loading the subclass later 
 *             in the execution of the program may invalidate this condition.
*              If a JIT compiler uses this function to perform  unconditional inlining, 
                the compiler must be prepared to patch the code later. 
*
 * @see vm_register_jit_overridden_method_callback
 */
Boolean
method_is_overridden(Open_Method_Handle method);

/**
 * Checks whether the method is the <init> method for the class. 
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the method is the <init> method; otherwise, <code>FALSE</code>.
 */
Boolean
method_is_init(Open_Method_Handle method);

/**
 * Checks whether the method is the <clinit> method for the class. 
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the method is the <clinit> method for the class; otherwise, <code>FALSE</code>.
 */
Boolean
method_is_clinit(Open_Method_Handle method);

/**
 * Checks whether the JIT compiler is allowed to in-line the method.
 * 
 * The compiler can be set not to in-line a method for various reasons, for example, 
 * the JIT must not in-line native methods and Java* methods
 * loaded by a different class loader than the caller.
 *
 * @param method - the method handle
 *
 * @return <code>TRUE</code> if the JIT must not in-line the method; otherwise, <code>FALSE</code>.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 * @note Always <code>FALSE</code> for Java*.
 */
Boolean
method_is_no_inlining(Open_Method_Handle method);

/**
 * Retrieves potential side effects of the given method. 
 *
 * @param m - handle for the method, for which side effects are retrieved
 *
 * @return Enumeration value which corresponds to side effects method may have.
 * One of:
 *      MSE_Unknown         - it is not known whether this method has side effects
 *      MSE_True            - method has side effects
 *      MSE_False           - method does not have side effects
 *      MSE_True_Null_Param - 
 */
VMEXPORT Method_Side_Effects method_get_side_effects(Open_Method_Handle m);

/**
 * Sets potential side effects of the given method. 
 *
 * @param m - method handle for which side effects are set
 * @param mse - side effect of this method (see method_get_side_effects for details)
 */
VMEXPORT void method_set_side_effects(Open_Method_Handle m, Method_Side_Effects mse);

/**
 * Sets the number of exception handlers in the code generated by the JIT compiler 
 * <i>jit</i> for the given method. 
 *
 * @param method        - the method handle
 * @param jit           - the JIT handle
 * @param num_handlers  - the number of exception handlers
 *
 * @note The JIT compiler must then call the <i>method_set_target_handler_info</i> function
 *             for each of the <i>num_handlers</i> exception handlers.
 */
void
method_set_num_target_handlers(Open_Method_Handle method,
                               Open_JIT_Handle jit,
                               unsigned short num_handlers);

/**
 *  Checks whether the local variable is pinned.
 *
 * @param method    - method handle
 * @param index     - the local variable index
 *
 * @return <i>TRUE</i> if the local variable is pinned; otherwise, <code>FALSE</code>.
 *
 * @note Replaces the method_vars_is_pinned function.
* @ingroup Extended 
* @note Not used.
 */
Boolean
method_local_is_pinned(Open_Method_Handle method, unsigned short index);

/**
 * Returns the handle for an accessible method overriding <i>method</i> in <i>klass</i> 
 * or in its closest superclass that overrides <i>method</i>.
 *
 * @param klass     - the class handle
 * @param method    - the method handle
 *
 * @return The method handle for an accessible method overriding <i>method</i>; otherwise, <code>FALSE</code>.
 */
Open_Method_Handle
method_find_overridden_method(Open_Class_Handle klass,
                              Open_Method_Handle method);

/**
 * Returns type information of the local variable.
 *
 * @param method    - the method handle
 * @param index     - the local variable number
 *
 * @return The type information of the local variable.
 *
 * @note Because local variables are not typed in Java*, this function
 *             always returns <code>NULL</code> for Java* methods.
 * @note Replaces the method_vars_get_type_info function. 
 */
Open_Type_Info_Handle
method_local_get_type_info(Open_Method_Handle method, unsigned short index);

/**
 * Returns the number of arguments defined for the method.
 * This number automatically includes the pointer to this (if present).
 *
 * @param method_signature  - method signature handle
 *
 * @return The number of arguments defined for the method.
 */
unsigned char
method_args_get_number(Method_Signature_Handle method_signature);

/**
 * Initializes the ChaMethodIterator <i>iterator</i> to iterate over all
 * methods that match the method signature and descend from <i>klass</i>
 * (including <i>klass</i> itself).
 *
 * @param iterator  - the class iterator
 * @param method    - the method handle
 * @param klass     - the class handle
 *
 * @return <code>TRUE</code> if iteration is supported over <i>klass</i>,
 *                <code>FALSE</code> if it is not.
 *
 * @note Reference to internal type ChaMethodIterator.
 */
Boolean
method_iterator_initialize(ChaMethodIterator* iterator,
                           Open_Method_Handle method,
                           Open_Class_Handle klass);

/**
 * Advances the iterator.
 *
 * @param iterator  - class iterator
 *
 * @note Reference to internal type ChaMethodIterator.
 */
void
method_iterator_advance(ChaMethodIterator* iterator);

/**
 * Returns the current method of the iterator.
 *
 * @param iterator  - the class iterator
 *
 * @return The current method of the iterator or <code>NULL</code> if no more methods are left.
 *
 * @note Reference to the internal type <i>ChaMethodIterator</i>.
 */
Method_Handle
method_iterator_get_current(ChaMethodIterator* iterator);

/**
 * Returns the number of exceptions that the given method can throw.
 *
 * @param method    - the method handle
 *
 * @return The number of exceptions.
 *
 * @note Replaces method_number_throws function.
 */
unsigned short
method_get_num_throws(Open_Method_Handle method);

/**
 * Returns the exception class a given method can throw.
 *
 * @param method    - the method handle
 * @param index     - the exception number
 *
 * @return the exception class a given method can throw.
 */
Open_Class_Handle
method_get_throws(Open_Method_Handle method, unsigned short index);

/**
 * Returns the number of method exception handlers.
 *
 * @param method    - the method handle
 *
 * @return The number of method exception handlers.
 *
 * @note An assertion is raised if method equals to <code>NULL</code>. 
 * @note Replaces method_get_num_handlers function.
 */
unsigned short
method_get_exc_handler_number(Open_Method_Handle method);

/**
 * Obtains the method exception handle information.
 *
 * @param method         - the method handle
 * @param index            - the exception handle index number
 * @param start_pc       - the resulting pointer to the exception handle start program count
 * @param end_pc         - the resulting pointer to the exception handle end program count
 * @param handler_pc  - the resulting pointer to the exception handle program count
 * @param catch_type  - the resulting pointer to the constant pool entry index
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code> or
 *             if the exception handle index is out of range or if any pointer equals to <code>NULL</code>. 
 * @note Replaces the method_get_handler_info function.
 */
void
method_get_exc_handler_info(Open_Method_Handle method,
                            unsigned short index,
                            unsigned short* start_pc,
                            unsigned short* end_pc,
                            unsigned short* handler_pc,
                            unsigned short* catch_type);

/**
 * Returns type information for the return value.
 *
 * @param method_signature  - the method signature handle
 *
 * @return The type information for the return value.
 */
Type_Info_Handle
method_ret_type_get_type_info(Method_Signature_Handle method_signature);

/**
 *  Checks whether the given argument is a managed pointer.
 *
 * @param method_signature  - the method signature handle
 * @param index             - the argument index
 *
 * @return <code>TRUE</code> if the argument is a managed pointer; otherwise, <code>FALSE</code>.
 */
Boolean
method_args_is_managed_pointer(Method_Signature_Handle method_signature, unsigned index);

/**
 * Returns type information for the given argument.
 *
 * @param method_signature  - the method signature handle
 * @param index             - the argument index
 *
 * @return Type information for the argument.
 */
Type_Info_Handle
method_args_get_type_info(Method_Signature_Handle method_signature, unsigned index);

/**
 * Returns the method bytecode size.
 *
 * @param method - the method handle
 *
 * @return The method bytecode size.
 *
 * @note Replaces the method_get_code_length function.
 */
jint
method_get_bytecode_size(Open_Method_Handle method);

/**
 * Returns the method bytecode array.
 *
 * @param method - the method handle
 *
 * @return The method bytecode array.
 *
 * @note An assertion is raised if the method equals to <code>NULL</code>. 
 * @note Reference to type <i>Byte</i>.
 * @note Replaces the method_get_bytecode function.
 */
const char*
method_get_bytecode_addr(Open_Method_Handle method);

/**
 * Returns the maximum number of local variables for the given method.
 *
 * @param method - the method handle
 *
 * @return The maximum number of local variables for the method.
 *
 * @note An assertion is raised if the method equals to <code>NULL</code>. 
 * @note Replaces functions method_get_max_local and method_vars_get_number.
 */
jint
method_get_max_locals(Open_Method_Handle method);

/**
 * Returns the maximum stack depth for the given method.
 *
 * @param method - the method handle
 *
 * @return The maximum stack depth for the method.
 *
 * @note An assertion is raised if <i>method</i> equals to <code>NULL</code>. 
 */
jint
method_get_max_stack(Open_Method_Handle method);

/**
 * Returns the offset from the start of the vtable to the entry for the given method
 *
 * @param method - the method handle
 *
 * @return The offset from the start of the vtable to the entry for the method, in bytes. 
 */
unsigned
method_get_offset(Open_Method_Handle method);

/**
 * Gets the address of the code pointer for the given method.
 * 
 * A straight-forward JIT compiler that does not support recompilation can only generate
 * code with indirect branches through the address provided by this function.
 *
 * @param method - the method handle
 *
 * @return the address where the code pointer for a given method is.
 *
 *  @see vm_register_jit_recompiled_method_callback
 */
Open_Native_Code_Ptr
method_get_indirect_address(Open_Method_Handle method);

/**
 * Allocates an information block for the given method.
 * 
 * An <i>info block</i> can be later retrieved by the JIT compiler for various operations. 
 * For example, the JIT can store GC maps for root set enumeration and stack
 * unwinding in the onfo block.
 *
 * @param method    - the method handle
 * @param jit       - the JIT handle
 * @param size      - the size of the allocated code block
 *
 * @return The pointer to the allocated info block.
 *
 * @note Reference to type <i>Byte</i>.
 *
 * @see method_allocate_data_block
 */
Byte*
method_allocate_info_block(Open_Method_Handle method,
                           JIT_Handle jit,
                           size_t size);

/**
 * Retrieves the memory block allocated earlier by the
 * <i>method_allocate_data_block</i> function.
* The pair of parameters <method, jit> uniquely identifies the JIT compiler information block.
 *
 * @param method    - the method handle
 * @param jit       - the JIT handle
 *
 * @return The pointer to the allocated data block.
 *
 * @note Reference to type <i>Byte</i>.
 */
Byte*
method_get_data_block_jit(Method_Handle method, JIT_Handle jit);

/**
 * Retrieves the memory block allocated earlier by the
 * <i>method_allocate_info_block</i> function.
 * The pair of parameters <method, jit> uniquely identifies the JIT compiler information block.
 *
 * @param method    - the method handle
 * @param jit       - the JIT handle
 *
 * @return The pointer to the allocated info block.
 *
 * @note Reference to type <i>Byte</i>.
 */
Byte*
method_get_info_block_jit(Method_Handle method, JIT_Handle jit);

#endif // _VM_METHOD_ACCESS_H
