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
 * @version $Revision: 1.1.2.1.4.4 $
 */

#define LOG_DOMAIN "exn"
#include "clog.h"

#include "heap.h"
#include "classloader.h"
#include "exceptions.h"
#include "ini.h"
#include "interpreter.h"
#include "m2n.h"
#include "object_handles.h"
#include "vm_strings.h"
#include "open/thread.h"

// internal declaration of functions
void __stdcall set_current_thread_exception_internal(ManagedObject * exn);

void exn_throw(jthrowable exc)
{
    if (interpreter_enabled()
        || (m2n_get_frame_type(m2n_get_last_frame()) & FRAME_NON_UNWINDABLE)) {
        exn_raise_only(exc);
        return;
    }
    else {
        exn_throw_only(exc);
    }
}   // exn_throw

void exn_throw_only(jthrowable exc)
{
    assert(!tmn_is_suspend_enabled());
    // Temporary fix of incorrect using of throw_by name in the interpreter 
    if (interpreter_enabled()) {
        exn_raise_only(exc);
        return;
    }
    assert(is_unwindable());

    TRACE2("exn", ("%s", "exn_throw_only(), delegating to exn_athrow()"));

    // XXX salikh: change to unconditional thread_disable_suspend()
    // we use conditional until all of the VM 
    // is refactored to be definitely gc-safe.
    exn_athrow(exc->object, NULL, NULL, NULL);
}

void exn_raise_only(jthrowable exc)
{
    assert(!is_unwindable());
    assert(exc);

    TRACE2("exn", ("%s", "exn_raise_only(), propagating non-destructively"));

    tmn_suspend_disable_recursive();
    set_current_thread_exception_internal(exc->object);
    tmn_suspend_enable_recursive();
}

bool exn_raised()
{
    // no need to disable gc for simple null equality check
    return (NULL != p_TLS_vmthread->p_exception_object);
}

jthrowable exn_get()
{

    // we can check heap references for equality to NULL
    // without disabling gc, because GC wouldn't change 
    // null to non-null and vice versa.
    if (NULL == p_TLS_vmthread->p_exception_object) {
        return NULL;
    }

    tmn_suspend_disable_recursive();
    jobject exc = oh_allocate_local_handle();
    exc->object = (ManagedObject *) p_TLS_vmthread->p_exception_object;
    tmn_suspend_enable_recursive();
    return exc;
}   // exn_get

void exn_clear()
{
    tmn_suspend_disable_recursive();
    clear_current_thread_exception();
    tmn_suspend_enable_recursive();
}

bool is_unwindable()
{
    return !(interpreter_enabled()
        || (m2n_get_frame_type(m2n_get_last_frame()) & FRAME_NON_UNWINDABLE));
}

static Class *get_class(const char *exception_name)
{
    Global_Env *env = VM_Global_State::loader_env;
    String *exc_str = env->string_pool.lookup(exception_name);
    Class *exc_clss =
        env->bootstrap_class_loader->LoadVerifyAndPrepareClass(env, exc_str);
    if (exc_clss == NULL) return NULL;
    tmn_suspend_disable();
    class_initialize(exc_clss);
    tmn_suspend_enable();
    assert(!exn_raised());
    return exc_clss;
}

static Method *lookup_constructor(Class * exc_clss, const char *signature)
{
    Global_Env *env = VM_Global_State::loader_env;
    // Get the method for the constructor
    String *init_name = env->Init_String;
    String *init_descr = env->string_pool.lookup(signature);
    Method *exc_init = class_lookup_method(exc_clss, init_name, init_descr);
    assert(exc_init);
    return exc_init;
}

static jthrowable create_exception(const char *exception_name)
{

    assert(tmn_is_suspend_enabled());
    Class *exc_clss = get_class(exception_name);
    if (exc_clss == NULL) {
        assert(exn_raised());
        return exn_get();
    }
    tmn_suspend_disable();

    ManagedObject *e =
        class_alloc_new_object_and_run_default_constructor(exc_clss);

    if (!e) {
        tmn_suspend_enable();
        return VM_Global_State::loader_env->java_lang_OutOfMemoryError;
    }

    jobject exc = oh_allocate_local_handle();
    exc->object = e;
    tmn_suspend_enable();
    return (jthrowable) exc;
}

static jthrowable create_exception(const char *exception_name,
    jthrowable cause)
{
    assert(tmn_is_suspend_enabled());
    Class *exc_clss = get_class(exception_name);
    if (exc_clss == NULL) {
        assert(exn_raised());
        return exn_get();
    }
    Method *exc_init =
        lookup_constructor(exc_clss, "(Ljava/lang/Throwable;)V");
    // XXX salikh: change to unconditional thread_disable_suspend()
    //bool gc_has_been_disabled = tmn_suspend_disable_and_return_old_value();
    tmn_suspend_disable();
    ManagedObject *e = class_alloc_new_object(exc_clss);

    if (!e) {
        tmn_suspend_enable();
        return VM_Global_State::loader_env->java_lang_OutOfMemoryError;
    }

    jthrowable exc = oh_allocate_local_handle();
    exc->object = e;

    jvalue args[2];
    args[0].l = exc;
    args[1].l = cause;
    vm_execute_java_method_array((jmethodID) exc_init, 0, args);
    tmn_suspend_enable();
    if (exn_raised()) {
        DIE(("Exception constructor has thrown an exception"));
    }
    //if (!gc_has_been_disabled) tmn_suspend_disable();
    return exc;
}   // create_exception(const char* exception_name, jthrowable cause)

static jthrowable create_exception(const char *exception_name,
    jthrowable cause, const char *message)
{
    // XXX salikh: change to unconditional thread_disable_suspend()
    assert(tmn_is_suspend_enabled());

    Class *exc_clss = get_class(exception_name);
    if (exc_clss == NULL) {
        assert(exn_raised());
        return exn_get();
    }
    Method *exc_init = lookup_constructor(exc_clss, "(Ljava/lang/String;)V");
    tmn_suspend_disable();  // ---------------------vvvvvvvvvvvvvvvvvvvvv

    ManagedObject *exc_obj = class_alloc_new_object(exc_clss);

    if (!exc_obj) {
        tmn_suspend_enable();
        return VM_Global_State::loader_env->java_lang_OutOfMemoryError;
    }

    jthrowable exc = oh_allocate_local_handle();
    exc->object = exc_obj;

    jobject arg = NULL;

    if (message != NULL) {
        ManagedObject *arg_obj =
            string_create_from_utf8(message, (unsigned) strlen(message));

        if (!arg_obj) {
            tmn_suspend_enable();
            return VM_Global_State::loader_env->java_lang_OutOfMemoryError;
        }

        arg = oh_allocate_local_handle();
        arg->object = arg_obj;
    }

    jvalue args[2];
    args[0].l = exc;
    args[1].l = arg;

    vm_execute_java_method_array((jmethodID) exc_init, 0, args);
    tmn_suspend_enable();   // ---------------------^^^^^^^^^^^^^^^^^^^^^^^^
    if (exn_raised()) {
        DIE(("Exception constructor has thrown an exception"));
    }

    if (cause != 0) {
        Method *exc_init_cause = class_lookup_method_recursive(exc_clss,
            "initCause", "(Ljava/lang/Throwable;)Ljava/lang/Throwable;");
        assert(exc_init_cause);
        jvalue args[2];
        args[0].l = exc;
        args[1].l = cause;
        jvalue ret_val;
        tmn_suspend_disable();  // ---------------------vvvvvvvvvvvvvvvvvvvvv    
        vm_execute_java_method_array((jmethodID) exc_init_cause, &ret_val,
            args);
        tmn_suspend_enable();   // ---------------------^^^^^^^^^^^^^^^^^^^^^^^

        if (exn_raised()) {
            DIE(("Exception constructor has thrown an exception"));
        }
    }
    return exc;
}   // create_exception

jthrowable exn_create(const char* exception_class)
{
    return create_exception(exception_class);
}


jthrowable exn_create(const char* exception_class, jthrowable cause)
{
    return create_exception(exception_class, cause);
}


jthrowable exn_create(const char* exception_class, const char* message)
{
    return create_exception(exception_class, NULL, message);
}


jthrowable exn_create(const char* exception_class, const char* message, jthrowable cause)
{
    return create_exception(exception_class, cause, message);
}


void exn_throw_by_name(const char *exception_name)
{
    assert(tmn_is_suspend_enabled());
    jthrowable exc = create_exception(exception_name);
    tmn_suspend_disable();
    exn_throw_only(exc);
    tmn_suspend_enable();

}   // exn_throw_by_name(const char* exception_name)

void exn_throw_by_name(const char *exception_name, const char *message)
{
    assert(tmn_is_suspend_enabled());
    jthrowable exc = create_exception(exception_name, NULL, message);
    tmn_suspend_disable();
    exn_throw_only(exc);
    tmn_suspend_enable();

}   // exn_throw_by_name(const char* exception_name, const char* message)

void exn_throw_by_name(const char *exception_name, jthrowable cause,
    const char *message)
{
    jthrowable exc = create_exception(exception_name, cause, message);
    tmn_suspend_disable();
    exn_throw_only(exc);
    tmn_suspend_enable();
}   // exn_throw_by_name(const char* exception_name, jthrowable cause, const char* message)

void exn_raise_by_name(const char *exception_name)
{
    jthrowable exc = create_exception(exception_name);
    exn_raise_only(exc);
}   // exn_raise_by_name(const char* exception_name)

void exn_raise_by_name(const char *exception_name, const char *message)
{
    jthrowable exc = create_exception(exception_name, NULL, message);
    exn_raise_only(exc);
}   // exn_raise_by_name(const char* exception_name, const char* message)

// XXX salikh: remove throw_java_exception
void throw_java_exception(const char *exception_name)
{
    exn_throw_by_name(exception_name);
}   //throw_java_exception

// XXX salikh: remove throw_java_exception
void throw_java_exception(const char *exception_name, const char *message)
{
    exn_throw_by_name(exception_name, message);
}   //throw_java_exception

// XXX salikh: remove create_chained_exception
jthrowable create_chained_exception(const char *exception_name,
    jthrowable cause)
{
    return create_exception(exception_name, cause);
}


////////////////////////////////////////////////////////////////////////
/// SOURCES BELOW HAVE NOT BEEN REFACTORED NOR CLEANED UP //-salikh/////
////////////////////////////////////////////////////////////////////////


#include <assert.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>

#include "platform.h"

#include "Class.h"
#include "classloader.h"
#include "environment.h"
#include "exception.h"
#include "exceptions.h"
#include "jit_intf.h"
#include "jit_intf_cpp.h"
#include "jni_direct.h"
#include "jni_utils.h"
#include "lil.h"
#include "lil_code_generator.h"
#include "mon_enter_exit.h"
#include "object_handles.h"
#include "vm_arrays.h"
#include "vm_stats.h"
#include "vm_strings.h"
#include "open/types.h"

#include "open/vm_util.h"

#include "stack_iterator.h"
#include "stack_trace.h"
#include "interpreter.h"

#ifdef _IPF_
#elif defined _EM64T_
#include "../m2n_em64t_internal.h"
#else
#include "../m2n_ia32_internal.h"
#endif


extern bool dump_stubs;

////////////////////////////////////////////////////////////////////////////
// Target_Exception_Handler

Target_Exception_Handler::Target_Exception_Handler(NativeCodePtr start_ip,
    NativeCodePtr end_ip,
    NativeCodePtr handler_ip, Class_Handle exn_class, bool exn_is_dead)
{
    _start_ip = start_ip;
    _end_ip = end_ip;
    _handler_ip = handler_ip;
    _exc = exn_class;
    _exc_obj_is_dead = exn_is_dead;
}

NativeCodePtr Target_Exception_Handler::get_start_ip()
{
    return _start_ip;
}

NativeCodePtr Target_Exception_Handler::get_end_ip()
{
    return _end_ip;
}

NativeCodePtr Target_Exception_Handler::get_handler_ip()
{
    return _handler_ip;
}

Class_Handle Target_Exception_Handler::get_exc()
{
    return _exc;
}

bool Target_Exception_Handler::is_exc_obj_dead()
{
    return _exc_obj_is_dead;
}

#ifdef POINTER64
typedef uint64 NumericNativeCodePtr;
#else
typedef uint32 NumericNativeCodePtr;
#endif

bool Target_Exception_Handler::is_in_range(NativeCodePtr ip, bool is_ip_past)
{
    NumericNativeCodePtr nip = (NumericNativeCodePtr) ip;
    NumericNativeCodePtr sip = (NumericNativeCodePtr) _start_ip;
    NumericNativeCodePtr eip = (NumericNativeCodePtr) _end_ip;

    return (is_ip_past ? sip < nip && nip <= eip : sip <= nip && nip < eip);
}   //Target_Exception_Handler::is_in_range

bool Target_Exception_Handler::is_assignable(Class_Handle exn_class)
{
    if (!_exc)
        return true;
    Class_Handle e = exn_class;
    while (e)
        if (e == _exc)
            return true;
        else
            e = class_get_super_class(e);
    return false;
}   //Target_Exception_Handler::is_assignable

void Target_Exception_Handler::update_catch_range(NativeCodePtr new_start_ip,
    NativeCodePtr new_end_ip)
{
    _start_ip = new_start_ip;
    _end_ip = new_end_ip;
}   //Target_Exception_Handler::update_catch_range

void Target_Exception_Handler::
update_handler_address(NativeCodePtr new_handler_ip)
{
    _handler_ip = new_handler_ip;
}   //Target_Exception_Handler::update_handler_address


//////////////////////////////////////////////////////////////////////////
// Current thread exception

void __stdcall set_current_thread_exception_internal(ManagedObject * exn)
{
    assert(!tmn_is_suspend_enabled());
    p_TLS_vmthread->p_exception_object = (volatile ManagedObject *) exn;
    p_TLS_vmthread->ti_exception_callback_pending = true;
}

VMEXPORT    // temporary solution for interpreter unplug
    ManagedObject * get_current_thread_exception()
{
    return (ManagedObject *) p_TLS_vmthread->p_exception_object;
}   //get_current_thread_exception

VMEXPORT    // temporary solution for interpreter unplug
void __stdcall set_current_thread_exception(ManagedObject * exn)
{
    assert(!is_unwindable());
    set_current_thread_exception_internal(exn);
}   //set_current_thread_exception

VMEXPORT    // temporary solution for interpreter unplug
void clear_current_thread_exception()
{
    // function should be only called from suspend disabled mode
    // it changes enumeratable reference to zero which is not
    // gc safe operation.
    assert(!tmn_is_suspend_enabled());
    p_TLS_vmthread->p_exception_object = NULL;
}   //clear_current_thread_exception

void rethrow_current_thread_exception()
{
    ManagedObject *exn = get_current_thread_exception();
    assert(exn);
    clear_current_thread_exception();
    exn_athrow(exn, NULL);
}   //rethrow_current_thread_exception

void rethrow_current_thread_exception_if_pending()
{
    ManagedObject *exn = get_current_thread_exception();
    if (exn) {
        clear_current_thread_exception();
        exn_athrow(exn, NULL);
    }
}   //rethrow_current_thread_exception_if_pending


//////////////////////////////////////////////////////////////////////////
// Java Stack Trace Utilities
#define STF_AS_JLONG 5

// prints stackTrace via java
inline void exn_java_print_stack_trace(FILE * UNREF f, ManagedObject * exn)
{
    // finds java environment
    JNIEnv_Internal *jenv = jni_native_intf;

    // creates exception object
    jthrowable exc = (jthrowable) oh_allocate_local_handle();
    ((ObjectHandle) exc)->object = exn;

    // finds class of Throwable
    jclass throwableClazz = FindClass(jenv, VM_Global_State::loader_env->JavaLangThrowable_String);

    // tries to print stackTrace via java
    jmethodID printStackTraceID =
        GetMethodID(jenv, throwableClazz, "printStackTrace", "()V");
    CallVoidMethod(jenv, exc, printStackTraceID);

    // free exception object
    oh_discard_local_handle((ObjectHandle) exc);
}

// prints stackTrace via jni
inline void exn_jni_print_stack_trace(FILE * f, jthrowable exc)
{
    assert(tmn_is_suspend_enabled());
    // finds java environment
    JNIEnv_Internal *jenv = jni_native_intf;

    // finds class of Throwable
    jclass throwableClazz = FindClass(jenv, VM_Global_State::loader_env->JavaLangThrowable_String);

    // print exception message
    if (ExceptionCheck(jenv))
        return;
    //if(get_current_thread_exception()) return;
    jmethodID getMessageId = GetMethodID(jenv, throwableClazz, "getMessage",
        "()Ljava/lang/String;");
    jstring message = CallObjectMethod(jenv, exc, getMessageId);
    if (ExceptionCheck(jenv))
        return;
    tmn_suspend_disable();
    const char *exceptionNameChars = exc->object->vt()->clss->name->bytes;
    tmn_suspend_enable();
    const char *messageChars = GetStringUTFChars(jenv, message, false);
    fprintf(f, "\n%s : %s\n", exceptionNameChars, messageChars);

    // gets stack trace to print it
    jmethodID getStackTraceID =
        GetMethodID(jenv, throwableClazz, "getStackTrace",
        "()[Ljava/lang/StackTraceElement;");
    jobjectArray stackTrace = CallObjectMethod(jenv, exc, getStackTraceID);
    if (ExceptionCheck(jenv) || !stackTrace)
        return;
    int stackTraceLenth = GetArrayLength(jenv, stackTrace);

    // finds all required JNI IDs from StackTraceElement to avoid finding in cycle
    jclass stackTraceElementClazz = struct_Class_to_java_lang_Class_Handle(
        VM_Global_State::loader_env->java_lang_StackTraceElement_Class);
    jmethodID getClassNameId =
        GetMethodID(jenv, stackTraceElementClazz, "getClassName",
        "()Ljava/lang/String;");
    jmethodID getMethodNameId =
        GetMethodID(jenv, stackTraceElementClazz, "getMethodName",
        "()Ljava/lang/String;");
    jmethodID getFileNameId =
        GetMethodID(jenv, stackTraceElementClazz, "getFileName",
        "()Ljava/lang/String;");
    jmethodID getLineNumberId =
        GetMethodID(jenv, stackTraceElementClazz, "getLineNumber", "()I");
    jmethodID isNativeMethodId =
        GetMethodID(jenv, stackTraceElementClazz, "isNativeMethod", "()Z");

    // prints stack trace line by line
    // Afremov Pavel 20050120 it's necessary to skip some line sof stack trace ganaration
    for (int itemIndex = 0; itemIndex < stackTraceLenth; itemIndex++) {
        // gets stack trace element (one line in stack trace)
        jobject stackTraceElement =
            GetObjectArrayElement(jenv, stackTrace, itemIndex);

        // prints begin of stack trace line
        fprintf(f, " at ");

        // gets and prints information about class and method
        jstring className =
            CallObjectMethod(jenv, stackTraceElement, getClassNameId);
        if (ExceptionCheck(jenv))
            return;
        jstring methodName =
            CallObjectMethod(jenv, stackTraceElement, getMethodNameId);
        if (ExceptionCheck(jenv))
            return;
        const char *classNameChars =
            GetStringUTFChars(jenv, className, false);
        fprintf(f, "%s.", classNameChars);
        const char *methodNameChars =
            GetStringUTFChars(jenv, methodName, false);
        fprintf(f, "%s", methodNameChars);

        // gets information about java file name
        jstring fileName =
            CallObjectMethod(jenv, stackTraceElement, getFileNameId);
        if (ExceptionCheck(jenv))
            return;

        // if it's known source ...
        if (fileName) {
            // gets line number and prints it after file name
            const char *fileNameChars =
                GetStringUTFChars(jenv, fileName, false);
            jint sourceLineNumber =
                CallIntMethod(jenv, stackTraceElement, getLineNumberId);
            if (ExceptionCheck(jenv))
                return;
            fprintf(f, " (%s:", fileNameChars);
            fprintf(f, " %d)", sourceLineNumber);
        }
        // if it's unknown source
        else {
            jboolean isNative =
                CallBooleanMethod(jenv, stackTraceElement, isNativeMethodId);
            if (ExceptionCheck(jenv))
                return;

            // if it's native
            if (isNative) {
                fprintf(f, " (Native Method)");
            }
            // or not
            else {
                fprintf(f, " (Unknown Source)");
            }
        }

        // prints end of stack trace line
        fprintf(f, "\n");
    }

    // gets caused exception
    jmethodID getCauseId =
        GetMethodID(jenv, throwableClazz, "getCause",
        "()Ljava/lang/Throwable;");
    jobject causedExc = CallObjectMethod(jenv, exc, getCauseId);
    if (ExceptionCheck(jenv))
        return;

    if (causedExc) {
        // if there is caused exception ...
        tmn_suspend_disable();  // -----------------vvv
        bool same_exception = false;
        if (causedExc->object == exc->object)
            same_exception = true;
        tmn_suspend_enable();   // ------------------^^^
        if (!same_exception) {
            // tries to print it
            fprintf(f, "caused by\n");
            exn_jni_print_stack_trace(f, causedExc);
        }
    }

    assert(tmn_is_suspend_enabled());
}

inline void exn_native_print_stack_trace(FILE * f, ManagedObject * exn)
{
//Afremov Pavel 20050119 Should be changed when classpath will raplaced by DRL
    assert(gid_throwable_traceinfo);

    // ? 20030428: This code should be elsewhere!
    unsigned field_offset = ((Field *) gid_throwable_traceinfo)->get_offset();
    ManagedObject **field_addr =
        (ManagedObject **) ((Byte *) exn + field_offset);
    Vector_Handle stack_trace =
        (Vector_Handle) get_raw_reference_pointer(field_addr);
    if (stack_trace) {
#if defined (__INTEL_COMPILER)
#pragma warning( push )
#pragma warning (disable:1683)  // to get rid of remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type
#endif
        unsigned depth =
            (unsigned) *get_vector_element_address_int64(stack_trace, 0);

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif

        StackTraceFrame *stf =
            (StackTraceFrame *) get_vector_element_address_int64(stack_trace,
            1);

        ExpandableMemBlock buf;
        for (unsigned i = 0; i < depth; i++)
            st_print_frame(&buf, stf + i);
        fprintf(f, "%s", buf.toString());
    }
}

// prints stack trace using 3 ways: via java, via jni, and native
void exn_print_stack_trace(FILE * f, ManagedObject * exn)
{
    assert(!tmn_is_suspend_enabled());
    // saves curent thread exception and clear to allow java to work
    Java_java_lang_Throwable *cte = get_current_thread_exception();
    jthrowable j = oh_allocate_local_handle();
    j->object = cte;
    /*
       Afremov Pavel 20050120
       FIXME:Don't wor under JIT, Fix requred
       // 1 way -> tries to print stacktrace via java 
       exn_java_print_stack_trace(f, exn);

       // if everything ok ...
       if (!get_current_thread_exception())
       {
       // restores curent thread exception and returns
       set_current_thread_exception_internal(j->object);
       return;
       }
     */
    // clears exception to allow java to work
    clear_current_thread_exception();

    {
        NativeObjectHandles lhs;
        jobject exc = oh_allocate_local_handle();
        exc->object = exn;

        // 2 way -> tries to print using jni access to class method
        tmn_suspend_enable();
        exn_jni_print_stack_trace(f, exc);
        tmn_suspend_disable();
        exn = exc->object;
    }

/*
Afremov Pavel 20050120
FIXME:Don't wor under ClassPath, Fix requred
    // if everything OK ...
    if (!get_current_thread_exception())
    {
        // restores curent thread exception and returns
        set_current_thread_exception_internal(j->object);
        return;
    }
*/

    // restore curent thread exception
    set_current_thread_exception_internal(j->object);

    exn_native_print_stack_trace(f, exn);
    fflush(f);
}


//////////////////////////////////////////////////////////////////////////
// Uncaught Exceptions

void print_uncaught_exception_message(FILE * f, char *context_message,
    ManagedObject * exn)
{
    assert(!tmn_is_suspend_enabled());
    fprintf(f, "** During %s uncaught exception: %s\n", context_message,
        exn->vt()->clss->name->bytes);
    exn_print_stack_trace(f, exn);
}


//////////////////////////////////////////////////////////////////////////
// Lazy Exception Utilities

static ManagedObject *create_lazy_exception(StackIterator * UNREF throw_si,
    Class_Handle exn_class, Method_Handle exn_constr, uint8 * exn_constr_args)
{
    assert(!tmn_is_suspend_enabled());
    volatile ManagedObject *exn_obj = 0;
    assert(!tmn_is_suspend_enabled());
    exn_obj =
        class_alloc_new_object_and_run_constructor((Class *) exn_class,
        (Method *) exn_constr, exn_constr_args);
    return (ManagedObject *) exn_obj;
}   //create_object_lazily

//////////////////////////////////////////////////////////////////////////
// Main Exception Propogation Function

// This function propagates an exception to its handler.
// It can only be called for the current thread.
// The input stack iterator provides the starting point for propogation.  If the top frame is an M2nFrame, it is ignored.
// Let A be the current frame, let B be the most recent M2nFrame prior to A.
// The exception is propagated to the first managed frame between A and B that has a handler for the exception,
// or to the native code that managed frame immediately after B if no such managed frame exists.
// If exn_obj is nonnull then it is the exception, otherwise the exception is an instance of
// exn_class created using the given constructor and arguments (a null exn_constr indicates the default constructor).
// The stack iterator is mutated to represent the context that should be resumed.
// The client should either use si_transfer_control to resume it, or use an OS context mechanism
// copied from the final stack iterator.

static void exn_propagate_exception(StackIterator * si,
    ManagedObject ** exn_obj, Class_Handle exn_class,
    Method_Handle exn_constr, uint8 * exn_constr_args)
{
    assert(!tmn_is_suspend_enabled());
    ASSERT_NO_INTERPRETER assert(*exn_obj || exn_class);

    // Save the throw context
    StackIterator *throw_si = si_dup(si);

    // Determine the type of the exception for the type tests below.
    if (*exn_obj)
        exn_class = (*exn_obj)->vt()->clss;

#ifdef VM_STATS
    ((Class *) exn_class)->num_throws++;
    vm_stats_total.num_exceptions++;
#endif // VM_STATS

    // Skip first frame if it is an M2nFrame (which is always a transition from managed to the throw code).
    // The M2nFrame will be removed from the thread's M2nFrame list but transfer control or copy to registers.
    if (si_is_native(si)) {
        si_goto_previous(si);
    }

    Method *interrupted_method = NULL;
    NativeCodePtr interrupted_method_location = NULL;
    JIT *interrupted_method_jit = NULL;

    if (!si_is_native(si))
    {
        CodeChunkInfo *interrupted_cci = si_get_code_chunk_info(si);
        assert(interrupted_cci);
        interrupted_method = interrupted_cci->get_method();
        interrupted_method_location = si_get_ip(si);
        interrupted_method_jit = interrupted_cci->get_jit();
    }

    bool same_frame = true;
    while (!si_is_past_end(si) && !si_is_native(si)) {
        CodeChunkInfo *cci = si_get_code_chunk_info(si);
        assert(cci);
        Method *method = cci->get_method();
        JIT *jit = cci->get_jit();
        assert(method && jit);
        NativeCodePtr ip = si_get_ip(si);
        bool is_ip_past = !!si_get_jit_context(si)->is_ip_past;

#ifdef VM_STATS
        cci->num_throws++;
#endif // VM_STATS

        // Examine this frame's exception handlers looking for a match
        unsigned num_handlers = cci->get_num_target_exception_handlers();
        for (unsigned i = 0; i < num_handlers; i++) {
            Target_Exception_Handler_Ptr handler =
                cci->get_target_exception_handler_info(i);
            if (!handler)
                continue;
            if (handler->is_in_range(ip, is_ip_past)
                && handler->is_assignable(exn_class)) {
                // Found a handler that catches the exception.
#ifdef VM_STATS
                cci->num_catches++;
                if (same_frame)
                    vm_stats_total.num_exceptions_caught_same_frame++;
                if (handler->is_exc_obj_dead())
                    vm_stats_total.num_exceptions_dead_object++;
#endif // VM_STATS
                // Setup handler context
                jit->fix_handler_context(method, si_get_jit_context(si));
                si_set_ip(si, handler->get_handler_ip(), false);

                // Create exception if necessary
                if (!*exn_obj) {
                    if (handler->is_exc_obj_dead()) {
#ifdef VM_STATS
                        vm_stats_total.num_exceptions_object_not_created++;
#endif // VM_STATS
                    }
                    else {
                        *exn_obj =
                            create_lazy_exception(throw_si, exn_class,
                            exn_constr, exn_constr_args);
                    }
                }

                // Reload exception object pointer because it could have
                // moved while calling JVMTI callback
                *exn_obj = jvmti_jit_exception_event_callback_call(*exn_obj,
                    interrupted_method_jit, interrupted_method,
                    interrupted_method_location,
                    jit, method, handler->get_handler_ip());

                TRACE2("exn", ("setting return pointer to %d", exn_obj));

                si_set_return_pointer(si, (void **) exn_obj);
                si_free(throw_si);
                return;
            }
        }

        // No appropriate handler found, undo synchronization
        if (method->is_synchronized()) {
            if (method->is_static()) {
                assert(!tmn_is_suspend_enabled());
                vm_monitor_exit(struct_Class_to_java_lang_Class(method->
                        get_class()));
            }
            else {
                void **p_this =
                    (void **) jit->get_address_of_this(method,
                    si_get_jit_context(si));
                vm_monitor_exit((ManagedObject *) * p_this);
            }
        }

        jvalue ret_val = {(jlong)0};
        jvmti_process_method_exit_event(reinterpret_cast<jmethodID>(method),
            JNI_TRUE, ret_val);

        // Goto previous frame
        si_goto_previous(si);
        same_frame = false;
    }

    // Exception propagates to the native code

    // The current thread exception is set to the exception and we return 0/NULL to the native code
    if (*exn_obj == NULL) {
        *exn_obj =
            create_lazy_exception(throw_si, exn_class, exn_constr,
            exn_constr_args);
    }
    assert(!tmn_is_suspend_enabled());

    CodeChunkInfo *catch_cci = si_get_code_chunk_info(si);
    Method *catch_method = NULL;
    if (catch_cci)
        catch_method = catch_cci->get_method();

    // Reload exception object pointer because it could have
    // moved while calling JVMTI callback
    *exn_obj = jvmti_jit_exception_event_callback_call(*exn_obj,
        interrupted_method_jit, interrupted_method, interrupted_method_location,
        NULL, NULL, NULL);

    set_current_thread_exception_internal(*exn_obj);

    *exn_obj = NULL;
    si_set_return_pointer(si, (void **) exn_obj);
    si_free(throw_si);
}   //exn_propagate_exception

#ifndef _IPF_
// Alexei
// Check if we could proceed with destructive stack unwinding,
// i. e. the last GC frame is created before the last m2n frame.
// We use here a knowledge that the newer stack objects
// have smaller addresses for ia32 and em64t architectures.
static bool UNUSED is_gc_frame_before_m2n_frame()
{
    if (p_TLS_vmthread->gc_frames) {
        POINTER_SIZE_INT m2n_address =
            (POINTER_SIZE_INT) m2n_get_last_frame();
        POINTER_SIZE_INT gc_frame_address =
            (POINTER_SIZE_INT) p_TLS_vmthread->gc_frames;
        // gc frame is created before the last m2n frame
        return m2n_address < gc_frame_address;
    }
    else {
        return true;    // no gc frames - nothing to be broken
    }
}
#endif // _IPF_

// Throw an exception in the current thread.
// Must be called with an M2nFrame on the top of the stack, and throws to the previous managed
// frames or the previous M2nFrame.
// Exception defined as in previous function.
// Does not return.

void exn_athrow(ManagedObject * exn_obj, Class_Handle exn_class,
    Method_Handle exn_constr, uint8 * exn_constr_args)
{
/*
 * !!!! NO LOGGER IS ALLOWED IN THIS FUNCTION !!!
 * !!!! RELEASE BUILD WILL BE BROKEN          !!!
 * !!!! NO TRACE2, INFO, WARN, ECHO, ASSERT, ...
 */
    assert(!tmn_is_suspend_enabled());
    //TRACE2("exn","exn_athrow");
    ASSERT_NO_INTERPRETER if ((exn_obj == NULL) && (exn_class == NULL)) {
        Global_Env *env = VM_Global_State::loader_env;
        exn_class = env->java_lang_NullPointerException_Class;
    }
    ManagedObject *local_exn_obj = exn_obj;
    StackIterator *si = si_create_from_native();

#ifndef _IPF_
    assert(is_gc_frame_before_m2n_frame());
    /*ASSERT(is_gc_frame_before_m2n_frame(), \
       "Last GC frame (" \
       << (POINTER_SIZE_INT) p_TLS_vmthread->gc_frames \
       << ") is below M2N frame (" \
       << (POINTER_SIZE_INT) m2n_get_last_frame() \
       << ") and would be broken during " \
       "destructive stack unwinding"); */
#endif // _IPF_


    //TRACE2("exn", "unwinding stack");
    if (si_is_past_end(si)) {
        set_current_thread_exception_internal(local_exn_obj);
        return;
    }

    //
    //TRACE2("exn", "transferring registers");
    si_transfer_all_preserved_registers(si);
    //TRACE2("exn", "propagating exception");
    exn_propagate_exception(si, &local_exn_obj, exn_class, exn_constr,
        exn_constr_args);
    //
    //TRACE2("exn", "transferring control");
    si_transfer_control(si);
}   //exn_athrow


// Throw an exception in the current thread.
// Must be called with the current thread "suspended" in managed code and regs holds the suspended values.
// Exception defined as in previous two functions.
// Mutates the regs value, which should be used to "resume" the managed code.

void exn_athrow_regs(Registers * regs, Class_Handle exn_class)
{
    assert(exn_class);
#ifndef _IPF_
    M2nFrame *m2nf = m2n_push_suspended_frame(regs);
    StackIterator *si = si_create_from_native();
    ManagedObject *local_exn_obj = NULL;
    exn_propagate_exception(si, &local_exn_obj, exn_class, NULL, NULL);
    si_copy_to_registers(si, regs);
    si_free(si);
    STD_FREE(m2nf);
#endif
}   //exn_athrow_regs


//////////////////////////////////////////////////////////////////////////
// Runtime Exception Support

// rt_throw takes an exception and throws it
NativeCodePtr exn_get_rth_throw()
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    LilCodeStub *cs = lil_parse_code_stub("entry 0:managed:ref:void;"
        "push_m2n 0, 0;"
        "m2n_save_all;" "out platform:ref,pint,pint,pint:void;");
    assert(cs);

    if (VM_Global_State::loader_env->compress_references)
        cs = lil_parse_onto_end(cs,
            "jc i0=%0i:ref,%n;"
            "o0=i0;" "j %o;" ":%g;" "o0=0:ref;" ":%g;", Class::heap_base);
    else
        cs = lil_parse_onto_end(cs, "o0=i0;");
    assert(cs);

    lil_parse_onto_end(cs,
        "o1=0;" "o2=0;" "o3=0;" "call.noret %0i;", exn_athrow);
    assert(cs);

    assert(lil_is_valid(cs));
    addr =
        LilCodeGenerator::get_platform()->compile(cs, "rth_throw",
        dump_stubs);
    lil_free_code_stub(cs);
    return addr;
}   //exn_get_rth_throw


static void rth_throw_lazy(Method * exn_constr)
{
#if defined(_IPF_) || defined(_EM64T_)
    ABORT("Not supported on this platform");
#else
    uint8 *args = (uint8 *) (m2n_get_args(m2n_get_last_frame()) + 1);   // +1 to skip constructor
    args += exn_constr->get_num_arg_bytes() - 4;
    exn_athrow(NULL, *(Class_Handle *) args, exn_constr, args);
#endif
}   //rth_throw_lazy


// rt_throw_lazy takes a constructor, the class for that constructor, and arguments for that constructor.
// it throws a (lazily created) instance of that class using that constructor and arguments.
NativeCodePtr exn_get_rth_throw_lazy()
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    LilCodeStub *cs = lil_parse_code_stub("entry 0:managed:pint:void;"
        "push_m2n 0, 0;"
        "m2n_save_all;" "in2out platform:void;" "call.noret %0i;",
        rth_throw_lazy);
    assert(lil_is_valid(cs));
    addr =
        LilCodeGenerator::get_platform()->compile(cs, "rth_throw_lazy",
        dump_stubs);
    lil_free_code_stub(cs);
    return addr;
}   //exn_get_rth_throw_lazy


// rt_throw_lazy_trampoline takes an exception class as first standard place
// and throws a (lazily created) instance of that class using the default constructor
NativeCodePtr exn_get_rth_throw_lazy_trampoline()
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    LilCodeStub *cs = lil_parse_code_stub("entry 1:managed::void;"
        "push_m2n 0, 0;"
        "m2n_save_all;"
        "out platform:ref,pint,pint,pint:void;"
        "o0=0:ref;" "o1=sp0;" "o2=0;" "o3=0;" "call.noret %0i;",
        exn_athrow);
    assert(lil_is_valid(cs));
    addr =
        LilCodeGenerator::get_platform()->compile(cs,
        "rth_throw_lazy_trampoline", dump_stubs);
    lil_free_code_stub(cs);
    return addr;
}   //exn_get_rth_throw_lazy_trampoline


// rth_throw_null_pointer throws a null pointer exception (lazily)
NativeCodePtr exn_get_rth_throw_null_pointer()
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    Class *exn_clss =
        VM_Global_State::loader_env->java_lang_NullPointerException_Class;
    LilCodeStub *cs =
        lil_parse_code_stub("entry 0:managed::void;" "std_places 1;"
        "sp0=%0i;" "tailcall %1i;",
        exn_clss,
        lil_npc_to_fp(exn_get_rth_throw_lazy_trampoline()));
    assert(lil_is_valid(cs));
    addr =
        LilCodeGenerator::get_platform()->compile(cs,
        "rth_throw_null_pointer", dump_stubs);
    lil_free_code_stub(cs);

    return addr;
}   //exn_get_rth_throw_null_pointer


// rth_throw_array_index_out_of_bounds throws an array index out of bounds exception (lazily)
NativeCodePtr exn_get_rth_throw_array_index_out_of_bounds()
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    Global_Env *env = VM_Global_State::loader_env;
    Class *exn_clss = env->java_lang_ArrayIndexOutOfBoundsException_Class;
    LilCodeStub *cs = lil_parse_code_stub("entry 0:managed::void;"
        "std_places 1;" "sp0=%0i;" "tailcall %1i;",
        exn_clss,
        lil_npc_to_fp(exn_get_rth_throw_lazy_trampoline()));
    assert(lil_is_valid(cs));
    addr =
        LilCodeGenerator::get_platform()->compile(cs,
        "rth_throw_array_index_out_of_bounds", dump_stubs);
    lil_free_code_stub(cs);

    return addr;
}   //exn_get_rth_throw_array_index_out_of_bounds


// Return the type of negative array size exception
Class_Handle exn_get_negative_array_size_exception_type()
{
    assert(tmn_is_suspend_enabled());
    Class *exn_clss;


    Global_Env *env = VM_Global_State::loader_env;
    String *exc_str =
        env->string_pool.lookup("java/lang/NegativeArraySizeException");
    exn_clss =
        env->bootstrap_class_loader->LoadVerifyAndPrepareClass(env, exc_str);
    assert(exn_clss);


    return exn_clss;
}

// rth_throw_negative_array_size throws a negative array size exception (lazily)
NativeCodePtr exn_get_rth_throw_negative_array_size()
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    LilCodeStub *cs = lil_parse_code_stub("entry 0:managed::void;"
        "std_places 1;" "sp0=%0i;" "tailcall %1i;",
        exn_get_negative_array_size_exception_type(),
        lil_npc_to_fp(exn_get_rth_throw_lazy_trampoline()));
    assert(lil_is_valid(cs));
    addr =
        LilCodeGenerator::get_platform()->compile(cs,
        "rth_throw_negative_array_size", dump_stubs);
    lil_free_code_stub(cs);

    return addr;
}   //exn_get_rth_throw_negative_array_size


// rth_throw_array_store throws an array store exception (lazily)
NativeCodePtr exn_get_rth_throw_array_store()
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    Global_Env *env = VM_Global_State::loader_env;
    LilCodeStub *cs = lil_parse_code_stub("entry 0:managed::void;"
        "std_places 1;" "sp0=%0i;" "tailcall %1i;",
        env->java_lang_ArrayStoreException_Class,
        lil_npc_to_fp(exn_get_rth_throw_lazy_trampoline()));
    assert(lil_is_valid(cs));
    addr =
        LilCodeGenerator::get_platform()->compile(cs, "rth_throw_array_store",
        dump_stubs);
    lil_free_code_stub(cs);

    return addr;
}   //exn_get_rth_throw_array_store


// rth_throw_arithmetic throws an arithmetic exception (lazily)
NativeCodePtr exn_get_rth_throw_arithmetic()
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    Global_Env *env = VM_Global_State::loader_env;
    LilCodeStub *cs = lil_parse_code_stub("entry 0:managed::void;"
        "std_places 1;" "sp0=%0i;" "tailcall %1i;",
        env->java_lang_ArithmeticException_Class,
        lil_npc_to_fp(exn_get_rth_throw_lazy_trampoline()));
    assert(lil_is_valid(cs));
    addr =
        LilCodeGenerator::get_platform()->compile(cs, "rth_throw_arithmetic",
        dump_stubs);
    lil_free_code_stub(cs);

    return addr;
}   //exn_get_rth_throw_arithmetic


// Return the type of class cast exception
Class_Handle exn_get_class_cast_exception_type()
{
    assert(tmn_is_suspend_enabled());
    Class *exn_clss;

    Global_Env *env = VM_Global_State::loader_env;
    String *exc_str = env->string_pool.lookup("java/lang/ClassCastException");
    exn_clss =
        env->bootstrap_class_loader->LoadVerifyAndPrepareClass(env, exc_str);
    assert(exn_clss);

    return exn_clss;
}

// rth_throw_class_cast_exception throws a class cast exception (lazily)
NativeCodePtr exn_get_rth_throw_class_cast_exception()
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    LilCodeStub *cs = lil_parse_code_stub("entry 0:managed::void;"
        "std_places 1;" "sp0=%0i;" "tailcall %1i;",
        exn_get_class_cast_exception_type(),
        lil_npc_to_fp(exn_get_rth_throw_lazy_trampoline()));
    assert(cs && lil_is_valid(cs));
    addr =
        LilCodeGenerator::get_platform()->compile(cs,
        "rth_throw_class_cast_exception", dump_stubs);
    lil_free_code_stub(cs);
    return addr;
}   //exn_get_rth_throw_class_cast_exception


// Return the type of incompatible class change exception
Class_Handle exn_get_incompatible_class_change_exception_type()
{
    assert(tmn_is_suspend_enabled());
    Class *exn_clss;

    Global_Env *env = VM_Global_State::loader_env;
    String *exc_str =
        env->string_pool.lookup("java/lang/IncompatibleClassChangeError");
    exn_clss =
        env->bootstrap_class_loader->LoadVerifyAndPrepareClass(env, exc_str);
    assert(exn_clss);


    return exn_clss;
}

// rth_throw_incompatible_class_change_exception throws an incompatible class change exception (lazily)
NativeCodePtr exn_get_rth_throw_incompatible_class_change_exception()
{
    static NativeCodePtr addr = NULL;
    if (addr) {
        return addr;
    }

    LilCodeStub *cs = lil_parse_code_stub("entry 0:managed::void;"
        "std_places 1;" "sp0=%0i;" "tailcall %1i;",
        exn_get_incompatible_class_change_exception_type(),
        lil_npc_to_fp(exn_get_rth_throw_lazy_trampoline()));
    assert(cs && lil_is_valid(cs));
    addr =
        LilCodeGenerator::get_platform()->compile(cs,
        "rth_throw_incompatible_class_change_exception", dump_stubs);
    lil_free_code_stub(cs);
    return addr;
}   //exn_get_rth_throw_incompatible_class_change_exception

