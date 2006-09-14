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
 * @author Intel, Gregory Shimansky
 * @version $Revision: 1.1.2.2.4.3 $
 */  


#define LOG_DOMAIN "jni.method"
#include "cxxlog.h"

#include <assert.h>

#include "jni.h"
#include "jni_utils.h"
#include "jni_direct.h"

#include "Class.h"
#include "environment.h"
#include "exceptions.h"
#include "object_handles.h"
#include "open/vm_util.h"
#include "vm_threads.h"

#include "ini.h"
#include "nogc.h"


jmethodID JNICALL GetMethodID(JNIEnv *env,
                              jclass clazz,
                              const char *name,
                              const char *descr)
{
    TRACE2("jni", "GetMethodID called");
    assert(hythread_is_suspend_enabled());
    assert(clazz);

    Class* clss = jclass_to_struct_Class(clazz);
    Method *method;
    if ('<' == *name) {
        if (!strcmp(name + 1, "init>")) {
            method = class_lookup_method_init(clss, descr);
        } else {
            ThrowNew_Quick(env, "java/lang/NoSuchMethodError", name);
            return NULL;
        }
    } else {
        method = class_lookup_method_recursive(clss, name, descr);
    }

    if(!method || method->is_static()) {
        ThrowNew_Quick(env, "java/lang/NoSuchMethodError", name);
        return NULL;
    }
    TRACE2("jni", "GetMethodID " << clss->name->bytes << "." << name << " " << descr << " = " << (jmethodID)method);

    return (jmethodID)method;
} //GetMethodID



jmethodID JNICALL GetStaticMethodID(JNIEnv *env,
                                    jclass clazz,
                                    const char *name,
                                    const char *descr)
{
    TRACE2("jni", "GetStaticMethodID called");
    assert(hythread_is_suspend_enabled());
    Class* clss = jclass_to_struct_Class(clazz);

    Method *method;
    if ('<' == *name) {
        if (!strcmp(name + 1, "clinit>") && !strcmp(descr, "()V")) {
            method = class_lookup_method_clinit(clss);
        } else {
            ThrowNew_Quick(env, "java/lang/NoSuchMethodError", name);
            return NULL;
        }
    } else {
        method = class_lookup_method_recursive(clss, name, descr);
    }

    if(!method || !method->is_static()) {
        ThrowNew_Quick(env, "java/lang/NoSuchMethodError", name);
        return NULL;
    }
    TRACE2("jni", "GetStaticMethodID " << clss->name->bytes << "." << name << " " << descr << " = " << (jmethodID)method);

    return (jmethodID)method;
} //GetStaticMethodID

static Method *object_lookup_method(jobject obj, const String* name, const String* desc) 
{
    ObjectHandle h = (ObjectHandle)obj;

    tmn_suspend_disable(); // v----------
    VTable *vtable = ((ManagedObject *)h->object)->vt();
    tmn_suspend_enable();  // ^----------

    assert(vtable);

    Method *method = class_lookup_method_recursive(vtable->clss, name, desc);
    if (method == 0) {
        DIE(" Can't find method " << name->bytes << " " 
            << desc->bytes);
    }

    return method;
}


/////////////////////////////////////////////////////////////////////////////
// begin Call<Type>MethodA functions


static void call_method_no_ref_result(JNIEnv *env,
                                      jobject obj,
                                      jmethodID methodID,
                                      jvalue *args,
                                      jvalue *result,
                                      int non_virtual)
{
    assert(hythread_is_suspend_enabled());
    Method *method = (Method *)methodID;

    if ( !non_virtual && !method_is_private(method) ) {
        // lookup the underlying "real" (e.g. abstract) method
        method = object_lookup_method(obj, method->get_name(), method->get_descriptor());
    }

    // Check method is not abstract
    // Alternative solution is to restore exception stubs in 
    // class_parse_methods() in Class_File_Loader.cpp, 
    // and to add similar functionality to interpreter
    if (method->is_abstract()) {
        ThrowNew_Quick (env, "java/lang/AbstractMethodError", 
                "attempt to invoke abstract method");
        return;
    }

    if (!ensure_initialised(env, method->get_class()))
        return;

    unsigned num_args = method->get_num_args();
    jvalue *all_args = (jvalue*) STD_ALLOCA(num_args * sizeof(jvalue));

    all_args[0].l = obj;
    memcpy(all_args + 1, args, (num_args - 1) * sizeof(jvalue));

    tmn_suspend_disable(); // v----------
    vm_execute_java_method_array((jmethodID)method, result, all_args);
    tmn_suspend_enable();  // ^----------
} //call_method_no_ref_result



void JNICALL CallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    TRACE2("jni", "CallVoidMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    CallVoidMethodV(env, obj, methodID, args);
} //CallVoidMethod



void JNICALL CallVoidMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallVoidMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    CallVoidMethodA(env, obj, methodID, jvalue_args);
    STD_FREE(jvalue_args);
} //CallVoidMethodV



void JNICALL CallVoidMethodA(JNIEnv *env,
                             jobject obj,
                             jmethodID methodID,
                             jvalue *args)
{
    TRACE2("jni", "CallVoidMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    call_method_no_ref_result(env, obj, methodID, args, 0, FALSE);
} //CallVoidMethodA



jobject JNICALL CallObjectMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    TRACE2("jni", "CallObjectMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallObjectMethodV(env, obj, methodID, args);
} //CallObjectMethod



jobject JNICALL CallObjectMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallObjectMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jobject result = CallObjectMethodA(env, obj, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallObjectMethodV



jobject JNICALL CallObjectMethodA(JNIEnv *env,
                                  jobject obj,
                                  jmethodID methodID,
                                  jvalue *args)
{
    TRACE2("jni", "CallObjectMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    Method *method = (Method *)methodID; // resolve to actual vtable entry below

    if (! method_is_private(method)) {
        // lookup the underlying "real" (e.g. abstract) method
        method = object_lookup_method(obj, method->get_name(), method->get_descriptor());
    }

    // Check method is not abstract
    // Alternative solution is to restore exception stubs in 
    // class_parse_methods() in Class_File_Loader.cpp, 
    // and to add similar functionality to interpreter
    if (method->is_abstract()) {
        ThrowNew_Quick (env, "java/lang/AbstractMethodError", 
                "attempt to invoke abstract method");
        return NULL;
    }

    if (!ensure_initialised(env, method->get_class()))
        return NULL;

    unsigned num_args = method->get_num_args();
    jvalue *all_args = (jvalue*) STD_ALLOCA(num_args * sizeof(jvalue));

    all_args[0].l = obj;
    memcpy(all_args + 1, args, (num_args - 1) * sizeof(jvalue));

    tmn_suspend_disable(); // v----------
    vm_execute_java_method_array((jmethodID)method, &result, all_args);
    tmn_suspend_enable();  // ^----------

    return result.l;
} //CallObjectMethodA



jboolean JNICALL CallBooleanMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    TRACE2("jni", "CallBooleanMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallBooleanMethodV(env, obj, methodID, args);
} //CallBooleanMethod



jboolean JNICALL CallBooleanMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallBooleanMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jboolean result = CallBooleanMethodA(env, obj, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallBooleanMethodV



jboolean JNICALL CallBooleanMethodA(JNIEnv *env,
                                    jobject obj,
                                    jmethodID methodID,
                                    jvalue *args)
{
    TRACE2("jni", "CallBooleanMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, FALSE);
    return result.z;
} //CallBooleanMethodA



jbyte JNICALL CallByteMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    TRACE2("jni", "CallByteMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallByteMethodV(env, obj, methodID, args);
} //CallByteMethod



jbyte JNICALL CallByteMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallByteMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jbyte result = CallByteMethodA(env, obj, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallByteMethodV



jbyte JNICALL CallByteMethodA(JNIEnv *env,
                              jobject obj,
                              jmethodID methodID,
                              jvalue *args)
{
    TRACE2("jni", "CallByteMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, FALSE);
    return result.b;
} //CallByteMethodA




jchar JNICALL CallCharMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    TRACE2("jni", "CallCharMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallCharMethodV(env, obj, methodID, args);
} //CallCharMethod



jchar JNICALL CallCharMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallCharMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jchar result = CallCharMethodA(env, obj, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallCharMethodV



jchar JNICALL CallCharMethodA(JNIEnv *env,
                              jobject obj,
                              jmethodID methodID,
                              jvalue *args)
{
    TRACE2("jni", "CallCharMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, FALSE);
    return result.c;
} //CallCharMethodA




jshort JNICALL CallShortMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    TRACE2("jni", "CallShortMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallShortMethodV(env, obj, methodID, args);
} //CallShortMethod



jshort JNICALL CallShortMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallShortMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jshort result = CallShortMethodA(env, obj, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallShortMethodV



jshort JNICALL CallShortMethodA(JNIEnv *env,
                                jobject obj,
                                jmethodID methodID,
                                jvalue *args)
{
    TRACE2("jni", "CallShortMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, FALSE);
    return result.s;
} //CallShortMethodA




jint JNICALL CallIntMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    TRACE2("jni", "CallIntMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallIntMethodV(env, obj, methodID, args);
} //CallIntMethod



jint JNICALL CallIntMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallIntMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jint result = CallIntMethodA(env, obj, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallIntMethodV



jint JNICALL CallIntMethodA(JNIEnv *env,
                              jobject obj,
                              jmethodID methodID,
                              jvalue *args)
{
    TRACE2("jni", "CallIntMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, FALSE);
    return result.i;
} //CallIntMethodA




jlong JNICALL CallLongMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    TRACE2("jni", "CallLongMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallLongMethodV(env, obj, methodID, args);
} //CallLongMethod



jlong JNICALL CallLongMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallLongMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jlong result = CallLongMethodA(env, obj, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallLongMethodV



jlong JNICALL CallLongMethodA(JNIEnv *env,
                              jobject obj,
                              jmethodID methodID,
                              jvalue *args)
{
    TRACE2("jni", "CallLongMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, FALSE);
    return result.j;
} //CallLongMethodA




jfloat JNICALL CallFloatMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    TRACE2("jni", "CallFloatMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallFloatMethodV(env, obj, methodID, args);
} //CallFloatMethod



jfloat JNICALL CallFloatMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallFloatMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jfloat result = CallFloatMethodA(env, obj, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallFloatMethodV



jfloat JNICALL CallFloatMethodA(JNIEnv *env,
                                jobject obj,
                                jmethodID methodID,
                                jvalue *args)
{
    TRACE2("jni", "CallFloatMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, FALSE);
    return result.f;
} //CallFloatMethodA




jdouble JNICALL CallDoubleMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...)
{
    TRACE2("jni", "CallDoubleMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallDoubleMethodV(env, obj, methodID, args);
} //CallDoubleMethod



jdouble JNICALL CallDoubleMethodV(JNIEnv *env, jobject obj, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallDoubleMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jdouble result = CallDoubleMethodA(env, obj, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallDoubleMethodV



jdouble JNICALL CallDoubleMethodA(JNIEnv *env,
                                  jobject obj,
                                  jmethodID methodID,
                                  jvalue *args)
{
    TRACE2("jni", "CallDoubleMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, FALSE);
    return result.d;
} //CallDoubleMethodA




// end Call<Type>MethodA functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
// begin CallNonvirtual<Type>MethodA functions


void JNICALL CallNonvirtualVoidMethod(JNIEnv *env,
                                       jobject obj,
                                       jclass clazz,
                                       jmethodID methodID,
                                       ...)
{
    TRACE2("jni", "CallNonvirtualVoidMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    CallNonvirtualVoidMethodV(env, obj, clazz, methodID, args);
} //CallNonvirtualVoidMethod



void JNICALL CallNonvirtualVoidMethodV(JNIEnv *env,
                                       jobject obj,
                                       jclass clazz,
                                       jmethodID methodID,
                                       va_list args)
{
    TRACE2("jni", "CallNonvirtualVoidMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    CallNonvirtualVoidMethodA(env, obj, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
} //CallNonvirtualVoidMethodV



void JNICALL CallNonvirtualVoidMethodA(JNIEnv *env,
                                       jobject obj,
                                       jclass UNREF clazz,
                                       jmethodID methodID,
                                       jvalue *args)
{
    TRACE2("jni", "CallNonvirtualVoidMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    call_method_no_ref_result(env, obj, methodID, args, 0, TRUE);
} //CallNonvirtualVoidMethodA



jobject JNICALL CallNonvirtualObjectMethod(JNIEnv *env,
                                           jobject obj,
                                           jclass clazz,
                                           jmethodID methodID,
                                           ...)
{
    TRACE2("jni", "CallNonvirtualObjectMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallNonvirtualObjectMethodV(env, obj, clazz, methodID, args);
} //CallNonvirtualObjectMethod



jobject JNICALL CallNonvirtualObjectMethodV(JNIEnv *env,
                                            jobject obj,
                                            jclass clazz,
                                            jmethodID methodID,
                                            va_list args)
{
    TRACE2("jni", "CallNonvirtualObjectMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jobject result = CallNonvirtualObjectMethodA(env, obj, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallNonvirtualObjectMethodV



jobject JNICALL CallNonvirtualObjectMethodA(JNIEnv *env,
                                            jobject obj,
                                            jclass UNREF clazz,
                                            jmethodID methodID,
                                            jvalue *args)
{
    TRACE2("jni", "CallNonvirtualObjectMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    Method *method = (Method *)methodID;
    if (!ensure_initialised(env, method->get_class()))
        return NULL;
    unsigned num_args = method->get_num_args();
    jvalue *all_args = (jvalue*) STD_ALLOCA(num_args * sizeof(jvalue));


    all_args[0].l = obj;
    memcpy(all_args + 1, args, (num_args - 1) * sizeof(jvalue));
    tmn_suspend_disable();
    vm_execute_java_method_array(methodID, &result, all_args);
   tmn_suspend_enable();
 
    return result.l;
} //CallNonvirtualObjectMethodA



jboolean JNICALL CallNonvirtualBooleanMethod(JNIEnv *env,
                                             jobject obj,
                                             jclass clazz,
                                             jmethodID methodID,
                                             ...)
{
    TRACE2("jni", "CallNonvirtualBooleanMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallNonvirtualBooleanMethodV(env, obj, clazz, methodID, args);
} //CallNonvirtualBooleanMethod



jboolean JNICALL CallNonvirtualBooleanMethodV(JNIEnv *env,
                                              jobject obj,
                                              jclass clazz,
                                              jmethodID methodID,
                                              va_list args)
{
    TRACE2("jni", "CallNonvirtualBooleanMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jboolean result = CallNonvirtualBooleanMethodA(env, obj, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallNonvirtualBooleanMethodV



jboolean JNICALL CallNonvirtualBooleanMethodA(JNIEnv *env,
                                              jobject obj,
                                              jclass UNREF clazz,
                                              jmethodID methodID,
                                              jvalue *args)
{
    TRACE2("jni", "CallNonvirtualBooleanMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, TRUE);
    return result.z;
} //CallNonvirtualBooleanMethodA



jbyte JNICALL CallNonvirtualByteMethod(JNIEnv *env,
                                       jobject obj,
                                       jclass clazz,
                                       jmethodID methodID,
                                       ...)
{
    TRACE2("jni", "CallNonvirtualByteMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallNonvirtualByteMethodV(env, obj, clazz, methodID, args);
} //CallNonvirtualByteMethod



jbyte JNICALL CallNonvirtualByteMethodV(JNIEnv *env,
                                        jobject obj,
                                        jclass clazz,
                                        jmethodID methodID,
                                        va_list args)
{
    TRACE2("jni", "CallNonvirtualByteMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jbyte result = CallNonvirtualByteMethodA(env, obj, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallNonvirtualByteMethodV



jbyte JNICALL CallNonvirtualByteMethodA(JNIEnv *env,
                                        jobject obj,
                                        jclass UNREF clazz,
                                        jmethodID methodID,
                                        jvalue *args)
{
    TRACE2("jni", "CallNonvirtualByteMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, TRUE);
    return result.b;
} //CallNonvirtualByteMethodA



jchar JNICALL CallNonvirtualCharMethod(JNIEnv *env,
                                       jobject obj,
                                       jclass clazz,
                                       jmethodID methodID,
                                       ...)
{
    TRACE2("jni", "CallNonvirtualCharMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallNonvirtualCharMethodV(env, obj, clazz, methodID, args);
} //CallNonvirtualCharMethod



jchar JNICALL CallNonvirtualCharMethodV(JNIEnv *env,
                                        jobject obj,
                                        jclass clazz,
                                        jmethodID methodID,
                                        va_list args)
{
    TRACE2("jni", "CallNonvirtualCharMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jchar result = CallNonvirtualCharMethodA(env, obj, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallNonvirtualCharMethodV



jchar JNICALL CallNonvirtualCharMethodA(JNIEnv *env,
                                        jobject obj,
                                        jclass UNREF clazz,
                                        jmethodID methodID,
                                        jvalue *args)
{
    TRACE2("jni", "CallNonvirtualCharMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, TRUE);
    return result.c;
} //CallNonvirtualCharMethodA



jshort JNICALL CallNonvirtualShortMethod(JNIEnv *env,
                                         jobject obj,
                                         jclass clazz,
                                         jmethodID methodID,
                                         ...)
{
    TRACE2("jni", "CallNonvirtualShortMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallNonvirtualShortMethodV(env, obj, clazz, methodID, args);
} //CallNonvirtualShortMethod



jshort JNICALL CallNonvirtualShortMethodV(JNIEnv *env,
                                          jobject obj,
                                          jclass clazz,
                                          jmethodID methodID,
                                          va_list args)
{
    TRACE2("jni", "CallNonvirtualShortMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jshort result = CallNonvirtualShortMethodA(env, obj, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallNonvirtualShortMethodV



jshort JNICALL CallNonvirtualShortMethodA(JNIEnv *env,
                                          jobject obj,
                                          jclass UNREF clazz,
                                          jmethodID methodID,
                                          jvalue *args)
{
    TRACE2("jni", "CallNonvirtualShortMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, TRUE);
    return result.s;
} //CallNonvirtualShortMethodA



jint JNICALL CallNonvirtualIntMethod(JNIEnv *env,
                                     jobject obj,
                                     jclass clazz,
                                     jmethodID methodID,
                                     ...)
{
    TRACE2("jni", "CallNonvirtualIntMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallNonvirtualIntMethodV(env, obj, clazz, methodID, args);
} //CallNonvirtualIntMethod



jint JNICALL CallNonvirtualIntMethodV(JNIEnv *env,
                                      jobject obj,
                                      jclass clazz,
                                      jmethodID methodID,
                                      va_list args)
{
    TRACE2("jni", "CallNonvirtualIntMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jint result = CallNonvirtualIntMethodA(env, obj, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallNonvirtualIntMethodV



jint JNICALL CallNonvirtualIntMethodA(JNIEnv *env,
                                      jobject obj,
                                      jclass UNREF clazz,
                                      jmethodID methodID,
                                      jvalue *args)
{
    TRACE2("jni", "CallNonvirtualIntMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result,  TRUE);
    return result.i;
} //CallNonvirtualIntMethodA



jlong JNICALL CallNonvirtualLongMethod(JNIEnv *env,
                                       jobject obj,
                                       jclass clazz,
                                       jmethodID methodID,
                                       ...)
{
    TRACE2("jni", "CallNonvirtualLongMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallNonvirtualLongMethodV(env, obj, clazz, methodID, args);
} //CallNonvirtualLongMethod



jlong JNICALL CallNonvirtualLongMethodV(JNIEnv *env,
                                        jobject obj,
                                        jclass clazz,
                                        jmethodID methodID,
                                        va_list args)
{
    TRACE2("jni", "CallNonvirtualLongMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jlong result = CallNonvirtualLongMethodA(env, obj, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallNonvirtualLongMethodV



jlong JNICALL CallNonvirtualLongMethodA(JNIEnv *env,
                                        jobject obj,
                                        jclass UNREF clazz,
                                        jmethodID methodID,
                                        jvalue *args)
{
    TRACE2("jni", "CallNonvirtualLongMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, TRUE);
    return result.j;
} //CallNonvirtualLongMethodA



jfloat JNICALL CallNonvirtualFloatMethod(JNIEnv *env,
                                         jobject obj,
                                         jclass clazz,
                                         jmethodID methodID,
                                         ...)
{
    TRACE2("jni", "CallNonvirtualFloatMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallNonvirtualFloatMethodV(env, obj, clazz, methodID, args);
} //CallNonvirtualFloatMethod



jfloat JNICALL CallNonvirtualFloatMethodV(JNIEnv *env,
                                          jobject obj,
                                          jclass clazz,
                                          jmethodID methodID,
                                          va_list args)
{
    TRACE2("jni", "CallNonvirtualFloatMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jfloat result = CallNonvirtualFloatMethodA(env, obj, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallNonvirtualFloatMethodV



jfloat JNICALL CallNonvirtualFloatMethodA(JNIEnv *env,
                                            jobject obj,
                                            jclass UNREF clazz,
                                            jmethodID methodID,
                                            jvalue *args)
{
    TRACE2("jni", "CallNonvirtualFloatMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, TRUE);
    return result.f;
} //CallNonvirtualFloatMethodA



jdouble JNICALL CallNonvirtualDoubleMethod(JNIEnv *env,
                                           jobject obj,
                                           jclass clazz,
                                           jmethodID methodID,
                                           ...)
{
    TRACE2("jni", "CallNonvirtualDoubleMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallNonvirtualDoubleMethodV(env, obj, clazz, methodID, args);
} //CallNonvirtualDoubleMethod



jdouble JNICALL CallNonvirtualDoubleMethodV(JNIEnv *env,
                                            jobject obj,
                                            jclass clazz,
                                            jmethodID methodID,
                                            va_list args)
{
    TRACE2("jni", "CallNonvirtualDoubleMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jdouble result = CallNonvirtualDoubleMethodA(env, obj, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallNonvirtualDoubleMethodV



jdouble JNICALL CallNonvirtualDoubleMethodA(JNIEnv *env,
                                            jobject obj,
                                            jclass UNREF clazz,
                                            jmethodID methodID,
                                            jvalue *args)
{
    TRACE2("jni", "CallNonvirtualDoubleMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_method_no_ref_result(env, obj, methodID, args, &result, TRUE);
    return result.d;
} //CallNonvirtualDoubleMethodA



// end CallNonvirtual<Type>MethodA functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
// begin CallStatic<Type>MethodA functions


static void call_static_method_no_ref_result(JNIEnv *env,
                                             jclass UNREF clazz,
                                             jmethodID methodID,
                                             jvalue *args,
                                             jvalue *result)
{
    assert(hythread_is_suspend_enabled());
    Method *method = (Method *)methodID;
    if (!ensure_initialised(env, method->get_class()))
        return;
    tmn_suspend_disable();
    vm_execute_java_method_array(methodID, result, args);
    tmn_suspend_enable();
} //call_static_method_no_ref_result



jobject JNICALL CallStaticObjectMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    TRACE2("jni", "CallStaticObjectMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallStaticObjectMethodV(env, clazz, methodID, args);
} //CallStaticObjectMethod



jobject JNICALL CallStaticObjectMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallStaticObjectMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jobject result = CallStaticObjectMethodA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallStaticObjectMethodV



jobject JNICALL CallStaticObjectMethodA(JNIEnv *env,
                                        jclass UNREF clazz,
                                        jmethodID methodID,
                                        jvalue *args)
{
    TRACE2("jni", "CallStaticObjectMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    Method *method = (Method *)methodID;
    if (!ensure_initialised(env, method->get_class()))
        return NULL;
    unsigned num_args = method->get_num_args();
    jvalue *all_args = (jvalue*) STD_ALLOCA(num_args * sizeof(jvalue));

    memcpy(all_args, args, num_args * sizeof(jvalue));
    tmn_suspend_disable();
    vm_execute_java_method_array(methodID, &result, all_args);
    tmn_suspend_enable();
 
    return result.l;
} //CallStaticObjectMethodA



jboolean JNICALL CallStaticBooleanMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    TRACE2("jni", "CallStaticBooleanMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallStaticBooleanMethodV(env, clazz, methodID, args);
} //CallStaticBooleanMethod



jboolean JNICALL CallStaticBooleanMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallStaticBooleanMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jboolean result = CallStaticBooleanMethodA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallStaticBooleanMethodV



jboolean JNICALL CallStaticBooleanMethodA(JNIEnv *env,
                                          jclass clazz,
                                          jmethodID methodID,
                                          jvalue *args)
{
    TRACE2("jni", "CallStaticBooleanMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_static_method_no_ref_result(env, clazz, methodID, args, &result);
    return result.z;
} //CallStaticBooleanMethodA



jbyte JNICALL CallStaticByteMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    TRACE2("jni", "CallStaticByteMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallStaticByteMethodV(env, clazz, methodID, args);
} //CallStaticByteMethod



jbyte JNICALL CallStaticByteMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallStaticByteMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jbyte result = CallStaticByteMethodA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallStaticByteMethodV



jbyte JNICALL CallStaticByteMethodA(JNIEnv *env,
                                    jclass clazz,
                                    jmethodID methodID,
                                    jvalue *args)
{
    TRACE2("jni", "CallStaticByteMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_static_method_no_ref_result(env, clazz, methodID, args, &result);
    return result.b;
} //CallStaticByteMethodA



jchar JNICALL CallStaticCharMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    TRACE2("jni", "CallStaticCharMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallStaticCharMethodV(env, clazz, methodID, args);
} //CallStaticCharMethod



jchar JNICALL CallStaticCharMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallStaticCharMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jchar result = CallStaticCharMethodA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallStaticCharMethodV



jchar JNICALL CallStaticCharMethodA(JNIEnv *env,
                                    jclass clazz,
                                    jmethodID methodID,
                                    jvalue *args)
{
    TRACE2("jni", "CallStaticCharMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_static_method_no_ref_result(env, clazz, methodID, args, &result);
    return result.c;
} //CallStaticCharMethodA



jshort JNICALL CallStaticShortMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    TRACE2("jni", "CallStaticShortMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallStaticShortMethodV(env, clazz, methodID, args);
} //CallStaticShortMethod



jshort JNICALL CallStaticShortMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallStaticShortMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jshort result = CallStaticShortMethodA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallStaticShortMethodV



jshort JNICALL CallStaticShortMethodA(JNIEnv *env,
                                      jclass clazz,
                                      jmethodID methodID,
                                      jvalue *args)
{
    TRACE2("jni", "CallStaticShortMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_static_method_no_ref_result(env, clazz, methodID, args, &result);
    return result.s;
} //CallStaticShortMethodA



jint JNICALL CallStaticIntMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    TRACE2("jni", "CallStaticIntMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallStaticIntMethodV(env, clazz, methodID, args);
} //CallStaticIntMethod



jint JNICALL CallStaticIntMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallStaticIntMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jint result = CallStaticIntMethodA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallStaticIntMethodV



jint JNICALL CallStaticIntMethodA(JNIEnv *env,
                                  jclass clazz,
                                  jmethodID methodID,
                                  jvalue *args)
{
    TRACE2("jni", "CallStaticIntMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_static_method_no_ref_result(env, clazz, methodID, args, &result);
    return result.i;
} //CallStaticIntMethodA



jlong JNICALL CallStaticLongMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    TRACE2("jni", "CallStaticLongMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallStaticLongMethodV(env, clazz, methodID, args);
} //CallStaticLongMethod



jlong JNICALL CallStaticLongMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallStaticLongMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jlong result = CallStaticLongMethodA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallStaticLongMethodV



jlong JNICALL CallStaticLongMethodA(JNIEnv *env,
                                    jclass clazz,
                                    jmethodID methodID,
                                    jvalue *args)
{
    TRACE2("jni", "CallStaticLongMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_static_method_no_ref_result(env, clazz, methodID, args, &result);
    return result.j;
} //CallStaticLongMethodA



jfloat JNICALL CallStaticFloatMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    TRACE2("jni", "CallStaticFloatMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallStaticFloatMethodV(env, clazz, methodID, args);
} //CallStaticFloatMethod



jfloat JNICALL CallStaticFloatMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallStaticFloatMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jfloat result = CallStaticFloatMethodA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallStaticFloatMethodV



jfloat JNICALL CallStaticFloatMethodA(JNIEnv *env,
                                      jclass clazz,
                                      jmethodID methodID,
                                      jvalue *args)
{
    TRACE2("jni", "CallStaticFloatMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_static_method_no_ref_result(env, clazz, methodID, args, &result);
    return result.f;
} //CallStaticFloatMethodA



jdouble JNICALL CallStaticDoubleMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    TRACE2("jni", "CallStaticDoubleMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return CallStaticDoubleMethodV(env, clazz, methodID, args);
} //CallStaticDoubleMethod



jdouble JNICALL CallStaticDoubleMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallStaticDoubleMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jdouble result = CallStaticDoubleMethodA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //CallStaticDoubleMethodV



jdouble JNICALL CallStaticDoubleMethodA(JNIEnv *env,
                                        jclass clazz,
                                        jmethodID methodID,
                                        jvalue *args)
{
    TRACE2("jni", "CallStaticDoubleMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue result;
    call_static_method_no_ref_result(env, clazz, methodID, args, &result);
    return result.d;
} //CallStaticDoubleMethodA



void JNICALL CallStaticVoidMethod(JNIEnv *env, jclass clazz, jmethodID methodID, ...)
{
    TRACE2("jni", "CallStaticVoidMethod called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    CallStaticVoidMethodV(env, clazz, methodID, args);
} //CallStaticVoidMethod



void JNICALL CallStaticVoidMethodV(JNIEnv *env, jclass clazz, jmethodID methodID, va_list args)
{
    TRACE2("jni", "CallStaticVoidMethodV called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    CallStaticVoidMethodA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
} //CallStaticVoidMethodV



void JNICALL CallStaticVoidMethodA(JNIEnv *env,
                                   jclass clazz,
                                   jmethodID methodID,
                                   jvalue *args)
{
    TRACE2("jni", "CallStaticVoidMethodA called, id = " << methodID);
    assert(hythread_is_suspend_enabled());
    call_static_method_no_ref_result(env, clazz, methodID, args, 0);
} //CallStaticVoidMethodA



// end CallStatic<Type>MethodA functions
/////////////////////////////////////////////////////////////////////////////
