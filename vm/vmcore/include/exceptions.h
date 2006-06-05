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
 * @author Intel, Pavel Afremov
 * @version $Revision: 1.1.2.1.4.3 $
 */  
#ifndef _EXCEPTIONS_H_
#define _EXCEPTIONS_H_


#include "jni.h"
#include "open/types.h"

/**
@file
\ref exceptions
*/

/**
@page exceptions Exceptions subsystem

\section exn_introduction Introduction
The functions to work with exceptions are described in exceptions.h.

\section exn_issues Issues

\li Interaction with JIT and runtime helpers? -salikh
\li Interaction with JIT is implemented via rth_wrap_exn_throw stubs. -pavel.n.afremov
\li Existing interface is currently included.

*/

/**
 * Throws an exception, and chooses the appropriate method
 * (destructive/non-destructive) automatically.
 * May not return in case of destructive exception propagation.
 *
 * @note The other methods which throws exception is removed 
 * from first version of exception interface. It's necessary 
 * crfeate additional component "conveniences"
 */
VMEXPORT void exn_throw(jthrowable exception);

/**
 * Throws an exception, and check that frame type is throwble.
 */
void exn_throw_only(jthrowable exception);

/**
 * Rise an exception, and check that frame type is unthrowble.
 */
void exn_raise_only(jthrowable exception);

/**
 * Returns the thread-local exception object
 * or NULL if no exception occured.
 *
 * @note rename of get_current_thread_exception(). It may be eliminated if
 * all other componets can use combination of exn_try() and exn_catch()
 * instead it.
 */
VMEXPORT jthrowable exn_get();

/**
 * Returns true if the thread-local exception object is set.
 */
VMEXPORT bool exn_raised();

/**
 * Clears the thread-local exception object.
 *
 * @note rename of clear_current_thread_exception(). It may be eliminated if
 * exn_catch() will be used and will clean thread exception.
 */
VMEXPORT void exn_clear();

/**
 * Creates exception object.
 */
jthrowable exn_create(const char* exception_class);
jthrowable exn_create(const char* exception_class, jthrowable cause);
jthrowable exn_create(const char* exception_class, const char* message);
jthrowable exn_create(const char* exception_class, const char* message, jthrowable cause);

/**
 * Returns true if frame is unwindable and false if isn't. 
 */
VMEXPORT bool is_unwindable();

/**
 * Creates and throws an exception.
 * Uses exn_throw_only() internally.
 * May not return in case of destructive exception propagation.
 *
 * @note internal convenience function, may not be exposed to VMI interface.
 */
void exn_throw_by_name(const char* exception_name);

/**
 * Creates and throws an exception.
 * Uses exn_throw_only() internally.
 * May not return in case of destructive exception propagation.
 *
 * @note internal convenience function, may not be exposed to VMI interface.
 */
void exn_throw_by_name(const char* exception_name, const char* message);

/**
 * Creates and throws an exception.
 * Uses exn_throw_only() internally.
 * May not return in case of destructive exception propagation.
 *
 * @note internal convenience function, may not be exposed to VMI interface.
 */
VMEXPORT void exn_throw_by_name(const char* exception_name, jthrowable cause, const char* message);

/**
 * Creates exceptions and sets it
 * as a thread local exception object.
 *
 * @note internal convenience function, may not be exposed to VMI interface.
 * @note explicit non-destructive semantics should be deduced from context.
 */
void exn_raise_by_name(const char* exception_name);

/**
 * Creates exceptions and sets it
 * as a thread local exception object.
 *
 * @note internal convenience function, may not be exposed to VMI interface.
 * @note explicit non-destructive semantics should be deduced from context.
 */
void exn_raise_by_name(const char* exception_name, const char* message);

/**
 * Pushes dummy non-unwindable stack frame in order to prevent stack unwinding.
 * After this returns true. If unwinding is happnened control coming back into
 * this function, and after this it returns false.
 *
 * @note experimental
 */
bool exn_function_try();

/**
 * pops dummy non-unwindable stack frame
 *
 * returns the current thread exception object
 * or NULL if no exception occured.
 *
 * @note experimental
 */
jthrowable exn_function_catch();

/**
 * Wrapper for exn_function_try.
 */
#define exn_try (if (exn_function_try()))

/**
 * Wrapper for exn_function_catch.
 */
#define exn_catch (th) (if ( th = exn_function_catch()))


////////////////////////////////////////////////////////////////////////
// FUNCTIONS below are from old VM implementation //////////////////////
////////////////////////////////////////////////////////////////////////

#include "open/vm_util.h"

struct ManagedObject;

//**** Stack Trace support

// Print the stack trace stored in the exception object to the given file.
void exn_print_stack_trace(FILE* f, ManagedObject* exn);

void print_uncaught_exception_message(FILE *f, char* context_message, Java_java_lang_Throwable *exc);

//**** Main exception propogation entry points

// Throw an exception in the current thread.
// Must be called with an M2nFrame on the top of the stack, and throws to the previous managed
// frames or the previous M2nFrame.
// If exn_obj is nonnull then it is the exception, otherwise the exception is an instance of
// exn_class created using the given constructor and arguments (a null exn_constr indicates the default constructor).
// Does not return.
void exn_athrow(ManagedObject* exn_obj, Class_Handle exn_class, Method_Handle exn_constr=NULL, uint8* exn_constr_args=NULL);

// Throw an exception in the current thread.
// Must be called with the current thread "suspended" in managed code and regs holds the suspended values.
// Exception defined as in previous two functions.
// Mutates the regs value, which should be used to "resume" the managed code.
void exn_athrow_regs(Registers* regs, Class_Handle exn_class);

//**** Runtime exception support

// rth_throw takes an exception and throws it
NativeCodePtr exn_get_rth_throw();

// rth_throw_lazy takes a constructor, the class for that constructor, and arguments for that constructor.
// it throws a (lazily created) instance of that class using that constructor and arguments.
NativeCodePtr exn_get_rth_throw_lazy();

// rth_throw_lazy_trampoline takes an exception class in the first standard place
// and throws a (lazily created) instance of that class using the default constructor
NativeCodePtr exn_get_rth_throw_lazy_trampoline();

// rth_throw_null_pointer throws a null pointer exception (lazily)
NativeCodePtr exn_get_rth_throw_null_pointer();

// rth_throw_array_index_out_of_bounds throws an array index out of bounds exception (lazily)
NativeCodePtr exn_get_rth_throw_array_index_out_of_bounds();

// rth_throw_negative_array_size throws a negative array size exception (lazily)
NativeCodePtr exn_get_rth_throw_negative_array_size();

// rth_throw_array_store throws an array store exception (lazily)
NativeCodePtr exn_get_rth_throw_array_store();

// rth_throw_arithmetic throws an arithmetic exception (lazily)
NativeCodePtr exn_get_rth_throw_arithmetic();

// rth_throw_class_cast_exception throws a class cast exception (lazily)
NativeCodePtr exn_get_rth_throw_class_cast_exception();

// rth_throw_incompatible_class_change_exception throws an incompatible class change exception (lazily)
NativeCodePtr exn_get_rth_throw_incompatible_class_change_exception();

//**** Various standard exception types

Class_Handle exn_get_class_cast_exception_type();

//**** Native code exception support

VMEXPORT // temporary solution for interpreter unplug
void clear_current_thread_exception();
void rethrow_current_thread_exception();
void rethrow_current_thread_exception_if_pending();

// These functions are depreciated and should not be called under any circumstance
VMEXPORT void throw_java_exception(const char *exception_name);
VMEXPORT void throw_java_exception(const char *exception_name, const char *message);

VMEXPORT jthrowable create_chained_exception(const char *exception_name, jthrowable);

// Per-thread exception object
VMEXPORT // temporary solution for interpreter unplug
ManagedObject* get_current_thread_exception();
VMEXPORT // temporary solution for interpreter unplug
void __stdcall set_current_thread_exception(ManagedObject* obj);

#endif // _EXCEPTIONS_H_
