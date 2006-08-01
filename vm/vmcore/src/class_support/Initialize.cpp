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
#include "open/thread.h"
#include "mon_enter_exit.h"
#include "exceptions.h"
#include "thread_manager.h"
#include "Verifier_stub.h"
#include "vm_strings.h"
#include "classloader.h"
#include "ini.h"


// Initializes a class.

static void class_initialize1(Class *clss)
{
    assert(!exn_raised());
    assert(!tmn_is_suspend_enabled());

    // the following code implements the 11-step class initialization program
    // described in page 226, section 12.4.2 of Java Language Spec, 1996
    // ISBN 0-201-63451-1

    TRACE2("class.init", "initializing class " << clss->name->bytes);

    // ---  step 1   ----------------------------------------------------------

    assert(!tmn_is_suspend_enabled());
    vm_monitor_enter_slow(struct_Class_to_java_lang_Class(clss));

#ifdef _DEBUG
    if (clss->p_initializing_thread){
        tmn_suspend_enable();
        tm_iterator_t * iterator = tm_iterator_create();
        tmn_suspend_disable();
        volatile VM_thread *p_scan = tm_iterator_next(iterator);
        while (p_scan) {
            if (p_scan == (VM_thread *)clss->p_initializing_thread) break;
            p_scan = tm_iterator_next(iterator);
        }
        tm_iterator_release(iterator);
        assert(p_scan);     // make sure p_initializer_thread is legit
    }
#endif  

    // ---  step 2   ----------------------------------------------------------
    TRACE2("class.init", "initializing class " << clss->name->bytes << " STEP 2" ); 

    jobject jlc = struct_Class_to_java_lang_Class_Handle(clss);
    while(((VM_thread *)(clss->p_initializing_thread) != p_TLS_vmthread) &&
        (clss->state == ST_Initializing) ) {
        // thread_object_wait had been expecting the only unsafe reference
        // to be its parameter, so enable_gc() should be safe here -salikh
        tmn_suspend_enable();
        thread_object_wait(jlc, 0);
        tmn_suspend_disable();
        jthrowable exc = exn_get();
        if (exc) {
            vm_monitor_exit(struct_Class_to_java_lang_Class(clss));
            return;
        }
    }

    // ---  step 3   ----------------------------------------------------------

    if ( (VM_thread *)(clss->p_initializing_thread) == p_TLS_vmthread) {
        vm_monitor_exit(struct_Class_to_java_lang_Class(clss));
        return;
    }

    // ---  step 4   ----------------------------------------------------------

    if (clss->state == ST_Initialized) {
        vm_monitor_exit(struct_Class_to_java_lang_Class(clss));
        return;
    }

    // ---  step 5   ----------------------------------------------------------

    if (clss->state == ST_Error) {
        vm_monitor_exit(struct_Class_to_java_lang_Class(clss));
        tmn_suspend_enable();
        jthrowable exn = exn_create("java/lang/NoClassDefFoundError",
            clss->name->bytes);
        tmn_suspend_disable();
        exn_raise_only(exn);
        return;
    }

    // ---  step 6   ----------------------------------------------------------
    TRACE2("class.init", "initializing class " << clss->name->bytes << "STEP 6" ); 

    assert(clss->state == ST_Prepared);
    clss->state = ST_Initializing;
    assert(clss->p_initializing_thread == 0);
    clss->p_initializing_thread = (void *)p_TLS_vmthread;
    vm_monitor_exit(struct_Class_to_java_lang_Class(clss));

    // ---  step 7 ------------------------------------------------------------

    if (clss->super_class) {
        class_initialize_ex(clss->super_class);

        if (clss->super_class->state == ST_Error) { 
            vm_monitor_enter_slow(struct_Class_to_java_lang_Class(clss));
            tmn_suspend_enable();
            REPORT_FAILED_CLASS_CLASS_EXN(clss->class_loader, clss,
                class_get_error_cause(clss->super_class));
            tmn_suspend_disable();
            clss->p_initializing_thread = 0;
            assert(!tmn_is_suspend_enabled());
            thread_object_notify_all(struct_Class_to_java_lang_Class_Handle(clss));
            vm_monitor_exit(struct_Class_to_java_lang_Class(clss));
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
        vm_monitor_enter_slow(struct_Class_to_java_lang_Class(clss));
        clss->state = ST_Initialized;
        TRACE2("classloader", "class " << clss->name->bytes << " initialized");
        clss->p_initializing_thread = 0;
        assert(!tmn_is_suspend_enabled());
        thread_object_notify_all (struct_Class_to_java_lang_Class_Handle(clss));
        vm_monitor_exit(struct_Class_to_java_lang_Class(clss));
        return;
    }

    TRACE2("class.init", "initializing class " << clss->name->bytes << " STEP 8" ); 
    jthrowable p_error_object;
    int tmn_suspend_disable_count();   
    assert(tmn_suspend_disable_count()==1);

    assert(!tmn_is_suspend_enabled());
    vm_execute_java_method_array((jmethodID) meth, 0, 0);
    p_error_object = exn_get();

    // ---  step 9   ----------------------------------------------------------
    TRACE2("class.init", "initializing class " << clss->name->bytes << " STEP 9" ); 

    if(!p_error_object) {
        vm_monitor_enter_slow(struct_Class_to_java_lang_Class(clss));
        clss->state = ST_Initialized;
        TRACE2("classloader", "class " << clss->name->bytes << " initialized");
        clss->p_initializing_thread = 0;
        assert(clss->p_error == 0);
        assert(!tmn_is_suspend_enabled());
        thread_object_notify_all (struct_Class_to_java_lang_Class_Handle(clss));
        vm_monitor_exit(struct_Class_to_java_lang_Class(clss));
        return;
    }

    // ---  step 10  ----------------------------------------------------------

    if(p_error_object) {
        assert(!tmn_is_suspend_enabled());
        clear_current_thread_exception();
        Class *p_error_class = p_error_object->object->vt()->clss;
        Class *jle = VM_Global_State::loader_env->java_lang_Error_Class;
        while(p_error_class && p_error_class != jle) {
            p_error_class = p_error_class->super_class;
        }
        assert(!tmn_is_suspend_enabled());
        if((!p_error_class) || (p_error_class != jle) ) {
#ifdef _DEBUG
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
        assert(!tmn_is_suspend_enabled());
        vm_monitor_enter_slow(struct_Class_to_java_lang_Class(clss));
        clss->state = ST_Error;
        clss->p_initializing_thread = 0;
        assert(!tmn_is_suspend_enabled());
        thread_object_notify_all(struct_Class_to_java_lang_Class_Handle(clss));
        vm_monitor_exit(struct_Class_to_java_lang_Class(clss));
        exn_raise_only(p_error_object);
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
    assert(tmn_is_suspend_enabled());

    // check verifier constraints
    if(!class_verify_constraints(VM_Global_State::loader_env, clss)) {
        if (!exn_raised()) {
            tmn_suspend_disable();
            exn_throw(class_get_error(clss->class_loader, clss->name->bytes));
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
    class_initialize_ex(clss);
}



void class_initialize_ex(Class *clss)
{
    assert(!tmn_is_suspend_enabled());

    // check verifier constraints
    tmn_suspend_enable();
    if(!class_verify_constraints(VM_Global_State::loader_env, clss)) {
        if (!exn_raised()) {
            tmn_suspend_disable();
            exn_throw(class_get_error(clss->class_loader, clss->name->bytes));
        }
        return;
    }
    tmn_suspend_disable();
    
    if ( class_needs_initialization(clss)) {
        class_initialize1(clss);
    }
} //class_initialize
