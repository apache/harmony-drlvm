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
 * @author Pavel Pervov
 * @version $Revision: 1.1.2.3.4.4 $
 */  

#define LOG_DOMAIN "classloader"
#include "cxxlog.h"

#include "Class.h"
#include "open/jthread.h"
#include "exceptions.h"
#include "thread_manager.h"
#include "Verifier_stub.h"
#include "vm_strings.h"
#include "classloader.h"
#include "ini.h"
#include "vm_threads.h"

// Initializes a class.

static void class_initialize1(Class *clss)
{
    ASSERT_RAISE_AREA;
    assert(!exn_raised());
    assert(!hythread_is_suspend_enabled());

    // the following code implements the 11-step class initialization program
    // described in page 226, section 12.4.2 of Java Language Spec, 1996
    // ISBN 0-201-63451-1

    TRACE2("class.init", "initializing class " << clss->name->bytes);

    // ---  step 1   ----------------------------------------------------------

    assert(!hythread_is_suspend_enabled());
    jobject jlc = struct_Class_to_java_lang_Class_Handle(clss);
    jthread_monitor_enter(jlc);

    // ---  step 2   ----------------------------------------------------------
    TRACE2("class.init", "initializing class " << clss->name->bytes << " STEP 2" ); 

    while(((VM_thread *)(clss->p_initializing_thread) != p_TLS_vmthread) &&
        (clss->state == ST_Initializing) ) {
        // thread_object_wait had been expecting the only unsafe reference
        // to be its parameter, so enable_gc() should be safe here -salikh
        jthread_monitor_wait(jlc);
        if (exn_raised()) {
             jthread_monitor_exit(jlc);
            return;
        }
    }

    // ---  step 3   ----------------------------------------------------------

    if ( (VM_thread *)(clss->p_initializing_thread) == p_TLS_vmthread) {
        jthread_monitor_exit(jlc);
        return;
    }

    // ---  step 4   ----------------------------------------------------------

    if (clss->state == ST_Initialized) {
        jthread_monitor_exit(jlc);
        return;
    }

    // ---  step 5   ----------------------------------------------------------

    if (clss->state == ST_Error) {
        jthread_monitor_exit(jlc);
        tmn_suspend_enable();
        exn_raise_by_name("java/lang/NoClassDefFoundError", clss->name->bytes);
        tmn_suspend_disable();
        return;
    }

    // ---  step 6   ----------------------------------------------------------
    TRACE2("class.init", "initializing class " << clss->name->bytes << "STEP 6" ); 

    assert(clss->state == ST_Prepared);
    clss->state = ST_Initializing;
    assert(clss->p_initializing_thread == 0);
    clss->p_initializing_thread = (void *)p_TLS_vmthread;
    jthread_monitor_exit(jlc);

    // ---  step 7 ------------------------------------------------------------

    if (clss->super_class) {
        class_initialize_ex(clss->super_class);

        if (clss->super_class->state == ST_Error) { 
            jthread_monitor_enter(jlc);
            tmn_suspend_enable();
            REPORT_FAILED_CLASS_CLASS_EXN(clss->class_loader, clss,
                class_get_error_cause(clss->super_class));
            tmn_suspend_disable();
            clss->p_initializing_thread = 0;
            clss->state = ST_Error;
            assert(!hythread_is_suspend_enabled());
            jthread_monitor_notify_all(jlc);
            jthread_monitor_exit(jlc);
            return;
        }
    }

    // ---  step pre-8   ------------------------------------------------------
    // This is an extra step needed to initialize static final fields

    unsigned n_fields = clss->n_fields;

    for (unsigned i = 0; i < n_fields; i++) {
        Field *f = &clss->fields[i];
        if (f->is_static() && f->get_const_value_index()) {
            void *addr = f->get_address();
            Const_Java_Value cv = f->get_const_value();
            switch (f->get_java_type()) {
        case JAVA_TYPE_BYTE:
        case JAVA_TYPE_BOOLEAN:
            *(int8 *)addr = (int8)cv.i;
            break;
        case JAVA_TYPE_CHAR:
        case JAVA_TYPE_SHORT:
            *(int16 *)addr = (int16)cv.i;
            break;
        case JAVA_TYPE_INT:
            *(int32 *)addr = cv.i;
            break;
        case JAVA_TYPE_LONG:
            *(int64 *)addr = cv.j;
            break;
        case JAVA_TYPE_FLOAT:
            *(float *)addr = cv.f;
            break;
        case JAVA_TYPE_DOUBLE:
            *(double *)addr = cv.d;
            break;
        default:
            assert(!strcmp("Ljava/lang/String;", f->get_descriptor()->bytes));
            String *str = cv.string;
            Java_java_lang_String *jls = string_create_from_utf8(str->bytes, str->len);
            assert(jls != NULL);
            STORE_GLOBAL_REFERENCE((COMPRESSED_REFERENCE *)addr, jls);
            break;
            }
        }
    }

    // ---  step 8   ----------------------------------------------------------

    Method *meth = clss->static_initializer;
    if (meth == NULL) {
        jthread_monitor_enter(jlc);
        clss->state = ST_Initialized;
        TRACE2("classloader", "class " << clss->name->bytes << " initialized");
        clss->p_initializing_thread = 0;
        assert(!hythread_is_suspend_enabled());
        jthread_monitor_notify_all(jlc);
        jthread_monitor_exit(jlc);
        return;
    }

    TRACE2("class.init", "initializing class " << clss->name->bytes << " STEP 8" ); 
    jthrowable p_error_object;

    assert(!hythread_is_suspend_enabled());
    vm_execute_java_method_array((jmethodID) meth, 0, 0);
    p_error_object = exn_get();

    // ---  step 9   ----------------------------------------------------------
    TRACE2("class.init", "initializing class " << clss->name->bytes << " STEP 9" ); 

    if(!p_error_object) {
        jthread_monitor_enter(jlc);
        clss->state = ST_Initialized;
        TRACE2("classloader", "class " << clss->name->bytes << " initialized");
        clss->p_initializing_thread = 0;
        assert(clss->p_error == 0);
        assert(!hythread_is_suspend_enabled());
        jthread_monitor_notify_all(jlc);
        jthread_monitor_exit(jlc);
        return;
    }

    // ---  step 10  ----------------------------------------------------------

    if(p_error_object) {
        assert(!hythread_is_suspend_enabled());
        exn_clear();
        Class *p_error_class = p_error_object->object->vt()->clss;
        Class *jle = VM_Global_State::loader_env->java_lang_Error_Class;
        while(p_error_class && p_error_class != jle) {
            p_error_class = p_error_class->super_class;
        }
        assert(!hythread_is_suspend_enabled());
        if((!p_error_class) || (p_error_class != jle) ) {
#ifdef _DEBUG_REMOVED
            Class* eiie = VM_Global_State::loader_env->java_lang_ExceptionInInitializerError_Class;
            assert(eiie);
#endif
            tmn_suspend_enable();

            p_error_object = exn_create("java/lang/ExceptionInInitializerError",
                p_error_object);
            tmn_suspend_disable();
        }
        tmn_suspend_enable();
        class_set_error_cause(clss, p_error_object);
        //REPORT_FAILED_CLASS_CLASS_EXN(clss->class_loader, clss, p_error_object);
        tmn_suspend_disable();

        // ---  step 11  ----------------------------------------------------------
        assert(!hythread_is_suspend_enabled());
        jthread_monitor_enter(jlc);
        clss->state = ST_Error;
        clss->p_initializing_thread = 0;
        assert(!hythread_is_suspend_enabled());
        jthread_monitor_notify_all(jlc);
        jthread_monitor_exit(jlc);
        exn_raise_object(p_error_object);
    }
    // end of 11 step class initialization program
} //class_initialize1


// Alexei
// migrating to C interfaces
#if (defined __cplusplus) && (defined PLATFORM_POSIX)
extern "C" {
#endif
void class_initialize_from_jni(Class *clss)
{
    ASSERT_RAISE_AREA;
    assert(hythread_is_suspend_enabled());

    // check verifier constraints
    if(!class_verify_constraints(VM_Global_State::loader_env, clss)) {
        if (!exn_raised()) {
            tmn_suspend_disable();
            exn_raise_object(class_get_error(clss->class_loader, clss->name->bytes));
            tmn_suspend_enable();
        }
        return;
    }

    tmn_suspend_disable();
    if (class_needs_initialization(clss)) {
        class_initialize1(clss);
    }
    tmn_suspend_enable();
} // class_initialize_from_jni
#if (defined __cplusplus) && (defined PLATFORM_POSIX)
}
#endif

// VMEXPORT
void class_initialize(Class *clss)
{
    ASSERT_RAISE_AREA;
    class_initialize_ex(clss);
}

void class_initialize_ex(Class *clss)
{
    ASSERT_RAISE_AREA;
    assert(!hythread_is_suspend_enabled());

    // check verifier constraints
    tmn_suspend_enable();
    if(!class_verify_constraints(VM_Global_State::loader_env, clss)) {
        if (!exn_raised()) {
            tmn_suspend_disable();
            exn_raise_object(class_get_error(clss->class_loader, clss->name->bytes));
        }
        return;
    }
    tmn_suspend_disable();
    
    if ( class_needs_initialization(clss)) {
        class_initialize1(clss);
    }
} //class_initialize
