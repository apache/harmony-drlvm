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
 * @author Euguene Ostrovsky
 * @version $Revision: 1.1.6.6 $
 */  

/**
 * @file org_apache_harmony_vm_VMStack.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of 
 * org.apache.harmony.drl.vm.VMStack class.
 */

#define LOG_DOMAIN "kernel.stack"
#include "cxxlog.h"

#include "stack_trace.h"
#include "jni_direct.h"
#include "jni_utils.h"
#include "environment.h"
#include "exceptions.h"
#include "vm_strings.h"

#include "java_lang_VMClassRegistry.h"

#include "org_apache_harmony_vm_VMStack.h"

/*
 * Class:     org_apache_harmony_vm_VMStack
 * Method:    getCallerClass
 * Signature: (I)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_org_apache_harmony_vm_VMStack_getCallerClass
  (JNIEnv * UNREF jenv, jclass, jint depth)
{
    // If depth is equal to 0 then caller of caller of this method should be
    // returned. Thus increment depth by 2.
    depth += 2;

    // obtain requested frame
    StackTraceFrame frame;
    bool res = st_get_frame(depth, &frame);

    // if no frame on specified depth
    if (!res) return NULL;

    // obtain and return class of the frame
    assert(hythread_is_suspend_enabled());
    return struct_Class_to_jclass((Class*)method_get_class(frame.method));
}

inline static bool isReflectionFrame(Method_Handle method) {
    const char *method_name = method_get_name(method);
    const char *class_name = class_get_name(method_get_class(method));
    static Method *invoke = 0;
    static Method *invokeMethod = 0;

    // if java.lang.reflect.Method.invoke()
    if (invoke) {
        if (method == invoke) return true;
    } else if (0 == strcmp(method_name, "invoke") &&
            0 == strcmp(class_name, "java/lang/reflect/Method") ) {
        invoke = method;
        return true;
    }

    // if java.lang.reflect.VMReflection.invokeMethod()
    if (invokeMethod) {
        if (method == invokeMethod) return true;
    } else if (0 == strcmp(method_name, "invokeMethod") &&
            0 == strcmp(class_name, "java/lang/reflect/VMReflection") ) {
        invokeMethod = method;
        return true;
    }

    // non reflection frame
    return false;
}

inline static bool isPrivilegedFrame(Method_Handle method) {
    static Method *doPrivileged = 0;

    if (doPrivileged) {
        return method == doPrivileged;
    }

    const char *method_name = method_get_name(method);
    const char *class_name = class_get_name(method_get_class(method));

    // if java.security.AccessController.doPrivileged()
    if (0 == strcmp(method_name, "doPrivileged") &&
            0 == strcmp(class_name, "java/security/AccessController") ) {
        doPrivileged = method;
        return true;
    }

    // non privileged frame
    return false;
}

/*
 * Class:     org_apache_harmony_vm_VMStack
 * Method:    getClasses
 * Signature: (IZ)[Ljava/lang/Class;
 */
JNIEXPORT jobjectArray JNICALL Java_org_apache_harmony_vm_VMStack_getClasses
  (JNIEnv *jenv_ext, jclass, jint signedMaxSize, jboolean considerPrivileged)
{
    JNIEnv_Internal *jenv = (JNIEnv_Internal *)jenv_ext;
    
    // if signedMaxSize is negative, maxSize will be > MAX_INT_VALUE
    unsigned maxSize = signedMaxSize;


    assert(hythread_is_suspend_enabled());
    unsigned size;
    StackTraceFrame* frames;
    st_get_trace(&size, &frames);

    // The caller of the caller of this method is stored as a first element of the array.
    // For details look at the org/apache/harmony/vm/VMStack.java file. Thus skipping 2 frames.
    unsigned skip = 2;

    // count target array length ignoring reflection frames
    unsigned length = 0, s;
    for (s = skip; s < size && length < maxSize; s++) {
        Method_Handle method = frames[s].method;

        if (isReflectionFrame(method))
            continue;

        if (considerPrivileged && isPrivilegedFrame(method) 
                && maxSize > length + 2)
            maxSize = length + 2;

        length ++;
    }

    assert(hythread_is_suspend_enabled());

    jclass ste = struct_Class_to_java_lang_Class_Handle(VM_Global_State::loader_env->JavaLangClass_Class);
    assert(ste);

    // creating java array
    jarray arr = jenv->NewObjectArray(length, ste, NULL);
    if (arr == NULL) {
        // OutOfMemoryError
        core_free(frames);
        assert(hythread_is_suspend_enabled());
        return NULL;
    }

    // filling java array
    unsigned i = 0;
    for (s = skip; s < size && i < length; s++) {
        Method_Handle method = frames[s].method;

        if (isReflectionFrame(method))
            continue;

        // obtain frame class
        jclass clz = struct_Class_to_java_lang_Class_Handle(method_get_class(method));

        jenv->SetObjectArrayElement(arr, i, clz);

        i++;
    }

    core_free(frames);
    assert(hythread_is_suspend_enabled());
    return arr;
}

/*
 * Class:     org_apache_harmony_vm_VMStack
 * Method:    getStackState
 * Signature: ()Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_org_apache_harmony_vm_VMStack_getStackState
  (JNIEnv *jenv, jclass)
{
    unsigned size;
    StackTraceFrame* frames;
    st_get_trace(&size, &frames);
    unsigned data_size = size * sizeof(StackTraceFrame);

    // pack trace into long[] array
    unsigned const elem_size = 8; // 8 bytes in each long element 
    unsigned array_length = (data_size + elem_size - 1) / elem_size; // array dimension

    // create long[] array
    jlongArray array = jenv->NewLongArray(array_length);
    if (!array) {
        return 0;
    }

    // copy data to array
    jlong* array_data = jenv->GetLongArrayElements(array, NULL);
    memcpy(array_data, frames, data_size);
    jenv->ReleaseLongArrayElements(array, array_data, 0);

    core_free(frames);

    return array;
} // Java_org_apache_harmony_vm_VMStack_getStackState

/*
 * Class:     org_apache_harmony_vm_VMStack
 * Method:    getStackTrace
 * Signature: (Ljava/lang/Object;)[Ljava/lang/StackTraceElement;
 */
JNIEXPORT jobjectArray JNICALL Java_org_apache_harmony_vm_VMStack_getStackTrace
  (JNIEnv * jenv_ext, jclass, jobject state)
{
    if (NULL == state)
        return NULL;

    Global_Env* genv = VM_Global_State::loader_env;

    JNIEnv_Internal *jenv = (JNIEnv_Internal *)jenv_ext;
     // state object contains raw data as long array
    jlongArray array = (jlongArray)state;
    assert(array);

    // copy data to array
    jlong* array_data = jenv->GetLongArrayElements(array, NULL);

    StackTraceFrame* frames = (StackTraceFrame*) array_data;
    unsigned size = jenv->GetArrayLength(array) * 8 / sizeof(StackTraceFrame);

    static Method_Handle fillInStackTrace = class_lookup_method(
        genv->java_lang_Throwable_Class,
            "fillInStackTrace", "()Ljava/lang/Throwable;");

    unsigned skip;

    // skip frames up to one with fillInStackTrace method and remember this
    // pointer
    for (skip = 0; skip < size; skip++) {
        Method_Handle method = frames[skip].method;
        assert(method);

        if (method == fillInStackTrace) {
            skip++;
            break;
        }
    }

    if (skip < size) {
        Method *method = frames[skip].method;
        // skip Throwable constructor
        if (method->is_init() && method->get_class() ==
                VM_Global_State::loader_env->java_lang_Throwable_Class) {
            void *old_this = frames[skip].outdated_this;
            skip++;

            // skip all exception constructors for this exception
            for (;skip < size; skip++) {
                Method *method = frames[skip].method;
                assert(method);

                if (!method->is_init()
                        || old_this != frames[skip].outdated_this) {
                    break;
                }
            }
        }
    }
    // skip Thread.runImpl()
    size--;

    // skip the VMStart$MainThread if one exits from the bottom of the stack
    // along with 2 reflection frames used to invoke method main
    static String* starter_String = genv->string_pool.lookup("java/lang/VMStart$MainThread");
    Method_Handle method = frames[size].method;
    assert(method);
    // skip only for main application thread
    if (!strcmp(method_get_name(method), "runImpl")
        && method->get_class()->name == starter_String) {
        int rem = size - skip-1;
        size -= rem < 3 ? rem : 3;
    }

    ASSERT(size >= skip, "Trying to skip " << skip 
        << " frames but there are only "
        << size << " frames in stack");
    
    assert(hythread_is_suspend_enabled());
    jclass ste = struct_Class_to_java_lang_Class_Handle(genv->java_lang_StackTraceElement_Class);
    assert(ste);

    static jmethodID init = (jmethodID) class_lookup_method(
        genv->java_lang_StackTraceElement_Class, genv->Init_String, 
        genv->string_pool.lookup("(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V"));
   
    jarray arr = jenv->NewObjectArray(size - skip, ste, NULL);
    if (!arr) {
        assert(exn_raised());
        return NULL;
    }

    tmn_suspend_disable();
    ObjectHandle strMethodName = oh_allocate_local_handle();
    ObjectHandle strClassName = oh_allocate_local_handle();
    tmn_suspend_enable();

    for(unsigned i = skip; i < size; i++) {
        Method_Handle method = frames[i].method;
        NativeCodePtr ip = frames[i].ip;
        int lineNumber;
        const char* fileName;

        get_file_and_line(method, ip, &fileName, &lineNumber);
        if (fileName == NULL) fileName = "";

        jstring strFileName = jenv->NewStringUTF(fileName);
        if (!strFileName) {
            assert(exn_raised());
            return NULL;
        }
     
        tmn_suspend_disable();
        // class name
        String* className = class_get_java_name(method->get_class(), 
            VM_Global_State::loader_env);
        strClassName->object = vm_instantiate_cp_string_resolved(className);
        if (!strClassName->object) {
            tmn_suspend_enable();
            assert(exn_raised());
            return NULL;
        }
        // method name
        strMethodName->object = vm_instantiate_cp_string_resolved(method->get_name());
        if (!strMethodName->object) {
            tmn_suspend_enable();
            assert(exn_raised());
            return NULL;
        }
        tmn_suspend_enable();

        // creating StackTraceElement object
        jobject obj = jenv->NewObject(ste, init, strClassName, strMethodName, 
                strFileName, lineNumber);
        if (!obj) {
            assert(exn_raised());
            return NULL;
        }

        jenv->SetObjectArrayElement(arr, i - skip, obj);
    }

    jenv->ReleaseLongArrayElements(array, array_data, JNI_ABORT);
    return arr;
}

/*
 * Class:     org_apache_harmony_vm_VMStack
 * Method:    getClassLoader
 * Signature: (Ljava/lang/Class;)Ljava/lang/ClassLoader;
 */
JNIEXPORT jobject JNICALL Java_org_apache_harmony_vm_VMStack_getClassLoader
  (JNIEnv *jenv, jclass, jclass clazz)
{
    // reuse similar method in VMClassRegistry
    return Java_java_lang_VMClassRegistry_getClassLoader(jenv, NULL, clazz);
}
