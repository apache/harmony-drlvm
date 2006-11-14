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
 * @author Intel, Gregory Shimansky
 * @version $Revision: 1.1.2.2.4.5 $
 */  

#define LOG_DOMAIN "jni"
#include "cxxlog.h"

#include <port_atomic.h>
#include <apr_pools.h>
#include <apr_thread_mutex.h>

#include "open/types.h"
#include "open/hythread.h"
#include "open/jthread.h"
#include "open/vm_util.h"

#include "platform.h"
#include "lock_manager.h"
#include "Class.h"
#include "classloader.h"
#include "environment.h"
#include "object_handles.h"
#include "vm_threads.h"
#include "vm_synch.h"
#include "exceptions.h"
#include "reflection.h"
#include "ini.h"
#include "vm_arrays.h"
#include "vm_strings.h"
#include "stack_trace.h"
#include "m2n.h"
#include "nogc.h"
#include "init.h"
#include "jni_utils.h"
#include "jit_runtime_support.h"
#include "jvmti_direct.h"

#ifdef _IPF_
#include "stub_code_utils.h"
#endif // _IPF_

static void JNICALL UnimpStub(JNIEnv*);
jint JNICALL GetVersion(JNIEnv *);

jobject JNICALL NewDirectByteBuffer(JNIEnv* , void* , jlong );
void* JNICALL GetDirectBufferAddress(JNIEnv* , jobject );
jlong JNICALL GetDirectBufferCapacity(JNIEnv* , jobject );

struct JNINativeInterface_ jni_vtable = {
            
    (void*)UnimpStub,
    (void*)UnimpStub,
    (void*)UnimpStub,
    (void*)UnimpStub,
    
    GetVersion,
            
    DefineClass,
    FindClass,
    FromReflectedMethod,
    FromReflectedField,
    ToReflectedMethod,
    GetSuperclass,
    IsAssignableFrom,

    ToReflectedField,
            
    Throw,
    ThrowNew,
    ExceptionOccurred,
    ExceptionDescribe,
    ExceptionClear,
    FatalError,
    PushLocalFrame,
    PopLocalFrame,
            
    NewGlobalRef,
    DeleteGlobalRef,
    DeleteLocalRef,
    IsSameObject,
    NewLocalRef,
    EnsureLocalCapacity,
            
    AllocObject,
    NewObject,
    NewObjectV,
    NewObjectA,
            
    GetObjectClass,
    IsInstanceOf,
            
    GetMethodID,
            
    CallObjectMethod,
    CallObjectMethodV,
    CallObjectMethodA,
    CallBooleanMethod,
    CallBooleanMethodV,
    CallBooleanMethodA,
    CallByteMethod,
    CallByteMethodV,
    CallByteMethodA,
    CallCharMethod,
    CallCharMethodV,
    CallCharMethodA,
    CallShortMethod,
    CallShortMethodV,
    CallShortMethodA,
    CallIntMethod,
    CallIntMethodV,
    CallIntMethodA,
    CallLongMethod,
    CallLongMethodV,
    CallLongMethodA,
    CallFloatMethod,
    CallFloatMethodV,
    CallFloatMethodA,
    CallDoubleMethod,
    CallDoubleMethodV,
    CallDoubleMethodA,
    CallVoidMethod,
    CallVoidMethodV,
    CallVoidMethodA,
            
    CallNonvirtualObjectMethod,
    CallNonvirtualObjectMethodV,
    CallNonvirtualObjectMethodA,
    CallNonvirtualBooleanMethod,
    CallNonvirtualBooleanMethodV,
    CallNonvirtualBooleanMethodA,
    CallNonvirtualByteMethod,
    CallNonvirtualByteMethodV,
    CallNonvirtualByteMethodA,
    CallNonvirtualCharMethod,
    CallNonvirtualCharMethodV,
    CallNonvirtualCharMethodA,
    CallNonvirtualShortMethod,
    CallNonvirtualShortMethodV,
    CallNonvirtualShortMethodA,
    CallNonvirtualIntMethod,
    CallNonvirtualIntMethodV,
    CallNonvirtualIntMethodA,
    CallNonvirtualLongMethod,
    CallNonvirtualLongMethodV,
    CallNonvirtualLongMethodA,
    CallNonvirtualFloatMethod,
    CallNonvirtualFloatMethodV,
    CallNonvirtualFloatMethodA,
    CallNonvirtualDoubleMethod,
    CallNonvirtualDoubleMethodV,
    CallNonvirtualDoubleMethodA,
    CallNonvirtualVoidMethod,
    CallNonvirtualVoidMethodV,
    CallNonvirtualVoidMethodA,
            
    GetFieldID,
            
    GetObjectField,
    GetBooleanField,
    GetByteField,
    GetCharField,
    GetShortField,
    GetIntField,
    GetLongField,
    GetFloatField,
    GetDoubleField,
    SetObjectField,
    SetBooleanField,
    SetByteField,
    SetCharField,
    SetShortField,
    SetIntField,
    SetLongField,
    SetFloatField,
    SetDoubleField,
            
    GetStaticMethodID,
            
    CallStaticObjectMethod,
    CallStaticObjectMethodV,
    CallStaticObjectMethodA,
    CallStaticBooleanMethod,
    CallStaticBooleanMethodV,
    CallStaticBooleanMethodA,
    CallStaticByteMethod,
    CallStaticByteMethodV,
    CallStaticByteMethodA,
    CallStaticCharMethod,
    CallStaticCharMethodV,
    CallStaticCharMethodA,
    CallStaticShortMethod,
    CallStaticShortMethodV,
    CallStaticShortMethodA,
    CallStaticIntMethod,
    CallStaticIntMethodV,
    CallStaticIntMethodA,
    CallStaticLongMethod,
    CallStaticLongMethodV,
    CallStaticLongMethodA,
    CallStaticFloatMethod,
    CallStaticFloatMethodV,
    CallStaticFloatMethodA,
    CallStaticDoubleMethod,
    CallStaticDoubleMethodV,
    CallStaticDoubleMethodA,
    CallStaticVoidMethod,
    CallStaticVoidMethodV,
    CallStaticVoidMethodA,
            
    GetStaticFieldID,
            
    GetStaticObjectField,
    GetStaticBooleanField,
    GetStaticByteField,
    GetStaticCharField,
    GetStaticShortField,
    GetStaticIntField,
    GetStaticLongField,
    GetStaticFloatField,
    GetStaticDoubleField,
            
    SetStaticObjectField,
    SetStaticBooleanField,
    SetStaticByteField,
    SetStaticCharField,
    SetStaticShortField,
    SetStaticIntField,
    SetStaticLongField,
    SetStaticFloatField,
    SetStaticDoubleField,
            
    NewString,
    GetStringLength,
    GetStringChars,
    ReleaseStringChars,
            
    NewStringUTF,
    GetStringUTFLength,
    GetStringUTFChars,
    ReleaseStringUTFChars,
            
    GetArrayLength,
            
    NewObjectArray,
    GetObjectArrayElement,
    SetObjectArrayElement,
            
    NewBooleanArray,
    NewByteArray,
    NewCharArray,
    NewShortArray,
    NewIntArray,
    NewLongArray,
    NewFloatArray,
    NewDoubleArray,
            
    GetBooleanArrayElements,
    GetByteArrayElements,
    GetCharArrayElements,
    GetShortArrayElements,
    GetIntArrayElements,
    GetLongArrayElements,
    GetFloatArrayElements,
    GetDoubleArrayElements,
            
    ReleaseBooleanArrayElements,
    ReleaseByteArrayElements,
    ReleaseCharArrayElements,
    ReleaseShortArrayElements,
    ReleaseIntArrayElements,
    ReleaseLongArrayElements,
    ReleaseFloatArrayElements,
    ReleaseDoubleArrayElements,
            
    GetBooleanArrayRegion,
    GetByteArrayRegion,
    GetCharArrayRegion,
    GetShortArrayRegion,
    GetIntArrayRegion,
    GetLongArrayRegion,
    GetFloatArrayRegion,
    GetDoubleArrayRegion,
    SetBooleanArrayRegion,
    SetByteArrayRegion,
    SetCharArrayRegion,
    SetShortArrayRegion,
    SetIntArrayRegion,
    SetLongArrayRegion,
    SetFloatArrayRegion,
    SetDoubleArrayRegion,

    RegisterNatives,
    UnregisterNatives,
            
    MonitorEnter,
    MonitorExit,
            
    GetJavaVM,

    GetStringRegion,
    GetStringUTFRegion,

    GetPrimitiveArrayCritical,
    ReleasePrimitiveArrayCritical,
    
    GetStringCritical,
    ReleaseStringCritical,

    NewWeakGlobalRef,
    DeleteWeakGlobalRef,

    ExceptionCheck,

    NewDirectByteBuffer,
    GetDirectBufferAddress,
    GetDirectBufferCapacity
};
            
const struct JNIInvokeInterface_ java_vm_vtable = {
    (void*)UnimpStub,
    (void*)UnimpStub,
    (void*)UnimpStub,

    DestroyJavaVM,

    AttachCurrentThread,
    DetachCurrentThread,

    GetEnv,

    AttachCurrentThreadAsDaemon
};

/**
 * List of all running in the current process.
 */ 
APR_RING_HEAD(JavaVM_Internal_T, JavaVM_Internal) GLOBAL_VMS;

/**
 * Memory pool to keep global data.
 */
apr_pool_t * GLOBAL_POOL = NULL;

/**
 * Used to synchronize VM creation and destruction.
 */
apr_thread_mutex_t * GLOBAL_LOCK = NULL;

static jboolean & get_init_status() {
    static jboolean init_status = JNI_FALSE;
    return init_status;
}
/**
 * Initializes JNI module.
 * Should  be called before creating first VM.
 */
static jint jni_init()
{
    jint status;

    if (get_init_status() == JNI_FALSE) {
         status = apr_initialize();
        if (status != APR_SUCCESS) return JNI_ERR;

        if (port_atomic_cas8((volatile uint8 *)&get_init_status(), JNI_TRUE, JNI_FALSE) == JNI_FALSE) {
            APR_RING_INIT(&GLOBAL_VMS, JavaVM_Internal, link);
            status = apr_pool_create(&GLOBAL_POOL, 0);
            if (status != APR_SUCCESS) return JNI_ERR;

            status = apr_thread_mutex_create(&GLOBAL_LOCK, APR_THREAD_MUTEX_DEFAULT, GLOBAL_POOL);
            if (status != APR_SUCCESS) {
                apr_pool_destroy(GLOBAL_POOL);
                return JNI_ERR;
            }
        }
    }
    return JNI_OK;
}

/*    BEGIN: List of directly exported functions.    */

JNIEXPORT jint JNICALL JNI_GetDefaultJavaVMInitArgs(void * args)
{
    // TODO: current implementation doesn't support JDK1_1InitArgs.
    if (((JavaVMInitArgs *)args)->version == JNI_VERSION_1_1) {
        return JNI_EVERSION;
    }
    ((JavaVMInitArgs *)args)->version = JNI_VERSION_1_4;
    return JNI_OK;
}

JNIEXPORT jint JNICALL JNI_GetCreatedJavaVMs(JavaVM ** vmBuf,
                                               jsize bufLen,
                                               jsize * nVMs)
{

    jint status = jni_init();
    if (status != JNI_OK) {
        return status;
    }

    apr_thread_mutex_lock(GLOBAL_LOCK);

    *nVMs = 0;
    if (!APR_RING_EMPTY(&GLOBAL_VMS, JavaVM_Internal, link)) {
            JavaVM_Internal * current_vm = APR_RING_FIRST(&GLOBAL_VMS);
            while (current_vm) {
                if (*nVMs < bufLen) {
                    vmBuf[*nVMs] = (JavaVM *)current_vm;
                }
                ++(*nVMs);
                current_vm = APR_RING_NEXT(current_vm, link);
            }

    }
    apr_thread_mutex_unlock(GLOBAL_LOCK);
    return JNI_OK;
}

JNIEXPORT jint JNICALL JNI_CreateJavaVM(JavaVM ** p_vm, JNIEnv ** p_jni_env,
                                          void * args) {
    jboolean daemon = JNI_FALSE;
    char * name = "main";
    JNIEnv * jni_env;
    JavaVMInitArgs * vm_args;
    JavaVM_Internal * java_vm;
    Global_Env * vm_env;
    apr_pool_t * vm_global_pool;
    jthread java_thread;
    jint status;

    status = jni_init();        
    
    if (status != JNI_OK) return status;

    apr_thread_mutex_lock(GLOBAL_LOCK);

    // TODO: only one VM instance can be created in the process address space.
    if (!APR_RING_EMPTY(&GLOBAL_VMS, JavaVM_Internal, link)) {
        status = JNI_ERR;
        goto done;
    }

    // Create global memory pool.
    status = apr_pool_create(&vm_global_pool, NULL);
    if (status != APR_SUCCESS) {
        TRACE2("jni", "Unable to create memory pool for VM");
        status = JNI_ENOMEM;
        goto done;
    }

    // TODO: current implementation doesn't support JDK1_1InitArgs.
    if (((JavaVMInitArgs *)args)->version == JNI_VERSION_1_1) {
        status = JNI_EVERSION;
        goto done;
    }

    vm_args = (JavaVMInitArgs *)args;
    // Create JavaVM_Internal.
    java_vm = (JavaVM_Internal *) apr_palloc(vm_global_pool, sizeof(JavaVM_Internal));
    if (java_vm == NULL) {
        status = JNI_ENOMEM;
        goto done;
    }

    // Create Global_Env.
    vm_env = new(vm_global_pool) Global_Env(vm_global_pool);
    if (vm_env == NULL) {
        status = JNI_ENOMEM;
        goto done;
    }

    java_vm->functions = &java_vm_vtable;
    java_vm->pool = vm_global_pool;
    java_vm->vm_env = vm_env;
    java_vm->reserved = (void *)0x1234abcd;
    *p_vm = java_vm;
        
    status = vm_init1(java_vm, vm_args);
    if (status != JNI_OK) {
        goto done;
    }

    // Attaches main thread to VM.
    status = vm_attach_internal(&jni_env, &java_thread, java_vm, NULL, name, daemon);
    if (status != JNI_OK) goto done;

    // Attaches main thread to TM.
    status = jthread_attach(jni_env, java_thread, daemon);
    if (status != TM_ERROR_NONE) {
        status = JNI_ERR;
        goto done;
    }
    assert(jthread_self() != NULL);
    *p_jni_env = jni_env;

    // Now JVMTIThread keeps global reference. Discared temporary global reference.
   jni_env->DeleteGlobalRef(java_thread);

    // Send VM start event. JNI services are available now.
    // JVMTI services permited in the start phase are available as well.
    jvmti_send_vm_start_event(vm_env, jni_env);

    status = vm_init2(jni_env);
    if (status != JNI_OK) {
        goto done;
    }

    // Send VM init event.
    jvmti_send_vm_init_event(vm_env);

    // Thread start event for the main thread should be sent after VMInit callback has finished.
    jvmti_send_thread_start_end_event(1);

    // Register created VM.
    APR_RING_INSERT_TAIL(&GLOBAL_VMS, java_vm, JavaVM_Internal, link);

    status  = JNI_OK;
done:
    apr_thread_mutex_unlock(GLOBAL_LOCK);
    return status;
}

/*    END: List of directly exported functions.    */

static void JNICALL UnimpStub(JNIEnv* UNREF env)
{
    // If we ever get here, we are in an implemented JNI function
    // By looking at the call stack and assembly it should be clear which one
    ABORT("Not implemented");
}


jint JNICALL GetVersion(JNIEnv * UNREF env)
{
    TRACE2("jni", "GetVersion called");
    return JNI_VERSION_1_4;
} //GetVersion

jclass JNICALL DefineClass(JNIEnv *jenv,
                           const char *name,
                           jobject loader,
                           const jbyte *buf,
                           jsize len)
{
    TRACE2("jni", "DefineClass called, name = " << name);
    assert(hythread_is_suspend_enabled());
    Global_Env* env = VM_Global_State::loader_env;
    ClassLoader* cl;
    if (loader != NULL)
    {
        if(name)
        {
            jclass clazz = GetObjectClass(jenv, loader);
            jmethodID meth_id = GetMethodID(jenv, clazz, "findLoadedClass", "(Ljava/lang/String;)Ljava/lang/Class;");
            if (NULL == meth_id)
            {
                jobject exn = exn_get();
                tmn_suspend_disable();
                Class *exn_class = exn->object->vt()->clss;
                assert(exn_class);
                bool f = exn_class != env->java_lang_OutOfMemoryError_Class;
                tmn_suspend_enable();
                if(f)
                    DIE("Fatal: an access to findLoadedClass method of java.lang.Class is not provided");
                // OutOfMemoryError should be rethrown after this JNI method exits
                return 0;
            }
            jstring str = NewStringUTF(jenv, name);
            if (NULL == str)
            {
                // the only OutOfMemoryError can be a reason to be here; it will be rethrown after this JNI method exits
                return 0;
            }
            jobject obj = CallObjectMethod(jenv, loader, meth_id, str);
            if(obj)
            {
                jthrowable exn;
                if(exn_raised())
                {
                    // pending exception will be rethrown after the JNI method exits
                    return 0;
                }
                else
                {
                    static const char* mess_templ = "duplicate class definition: ";
                    static unsigned mess_templ_len = strlen(mess_templ);
                    unsigned mess_size = mess_templ_len + strlen(name) + 1; // 1 is for trailing '\0'
                    char* err_mess = (char*)STD_ALLOCA(mess_size);
                    sprintf(err_mess, "%s%s", mess_templ, name);
                    exn = exn_create("java/lang/LinkageError", err_mess);
                }

                assert(exn);
                jint UNUSED ok = Throw(jenv, exn);
                assert(ok == 0);
                return 0;
            }
        }
        cl = class_loader_lookup(loader);
    }
    else
        cl = env->bootstrap_class_loader;

    if(name)
    {
        //Replace '.' with '/'
        char *p = (char*)name;
        while(*p)
        {
            if(*p == '.')*p = '/';
            p++;
        }
    }

    const String* res_name;
    Class* clss = cl->DefineClass(env, name, (uint8 *)buf, 0, len, &res_name);

    bool ld_result;
    if(clss != NULL)
        ld_result = clss->verify(env) && clss->prepare(env);

    if(clss && ld_result)
    {
        assert(hythread_is_suspend_enabled());
        return struct_Class_to_jclass(clss);
    }
    else
    {
        if (exn_raised()) {
            return 0;
        } else if(res_name) {
            jthrowable exn = (jthrowable)class_get_error(cl, res_name->bytes);
            exn_raise_object(exn);
        } else {
            exn_raise_by_name("java/lang/LinkageError",
                "defineClass failed for unnamed class", NULL);
        }
        return 0;
    }
} //DefineClass

jclass JNICALL FindClass(JNIEnv* env_ext,
                         const char* name)
{
    if (exn_raised()) return NULL;
    TRACE2("jni", "FindClass called, name = " << name);

    char *ch = strchr(name, '.');
    if (NULL != ch)
    {
        ThrowNew_Quick(env_ext,
            VM_Global_State::loader_env->JavaLangNoClassDefFoundError_String->bytes,
            name);
        return NULL;
    }

    String* cls_name = VM_Global_State::loader_env->string_pool.lookup(name);
    return FindClass(env_ext, cls_name);
    
} //FindClass

jclass JNICALL GetSuperclass(JNIEnv * UNREF env, jclass clazz)
{
    TRACE2("jni", "GetSuperclass called");
    assert(hythread_is_suspend_enabled());
    Class* clss = jclass_to_struct_Class(clazz);
    if(clss) {
        if(clss->is_interface()) {
            return 0;
        } else {
            Class* super_class = clss->get_super_class();
            if(super_class) {
                assert(hythread_is_suspend_enabled());
                return struct_Class_to_jclass(super_class);
            } else {
                return 0;
            }
        }
    } else {
        return 0;
    }
} //GetSuperclass

jboolean JNICALL IsAssignableFrom(JNIEnv * UNREF env,
                                  jclass clazz1,
                                  jclass clazz2)
{
    TRACE2("jni", "IsAssignableFrom called");
    assert(hythread_is_suspend_enabled());
    Class* clss1 = jclass_to_struct_Class(clazz1);
    Class* clss2 = jclass_to_struct_Class(clazz2);

    Boolean isAssignable = class_is_subtype(clss1, clss2);
    if (isAssignable) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
} //IsAssignableFrom

jint JNICALL Throw(JNIEnv * UNREF env, jthrowable obj)
{
    TRACE2("jni", "Throw called");
    assert(hythread_is_suspend_enabled());
    if(obj) {
        exn_raise_object(obj);
        return 0;
    } else {
        return -1;
    }
} // Throw

jint JNICALL ThrowNew(JNIEnv *env, jclass clazz, const char *message)
{
    TRACE2("jni", "ThrowNew called, message = " << (message ? message : "<no message>"));
    assert(hythread_is_suspend_enabled());
    if (!clazz) return -1;

    jstring str = (jstring)0;
    if(message) {
        str = NewStringUTF(env, message);
        if(!str) {
            return -1;
        }
    }

    jmethodID init_id = GetMethodID(env, clazz, "<init>", "(Ljava/lang/String;)V");
    jvalue args[1];
    args[0].l = str;
    jobject obj = NewObjectA(env, clazz, init_id, args);
    env->DeleteLocalRef(str);
    if(obj && !ExceptionOccurred(env)) {
        exn_raise_object(obj);
        return 0;
    } else {
        return -1;
    }
} //ThrowNew

jint JNICALL ThrowNew_Quick(JNIEnv *env, const char *classname, const char *message)
{
    jclass exclazz = FindClass(env, classname);
    if(!exclazz) {
        return -1;
    }

    jint result = ThrowNew(env, exclazz, message);
    env->DeleteLocalRef(exclazz);
    return result;
} // ThrowNew_Quick

//FIXME LAZY EXCEPTION (2006.05.15)
// chacks usage of this function and replace by lazy
jthrowable JNICALL ExceptionOccurred(JNIEnv * UNREF env)
{
    TRACE2("jni", "ExceptionOccurred called");
    assert(hythread_is_suspend_enabled());
#ifdef _DEBUG
    tmn_suspend_disable();
    if (!exn_raised())
    {
        TRACE2("jni", "Exception occured, no exception");
    }
    else
    {
        TRACE2("jni", "Exception occured, class = " << exn_get_name());
    }
    tmn_suspend_enable();
#endif

    if(exn_raised()) {
        return exn_get();
    } else {
        return 0;
    }
} //ExceptionOccurred

void JNICALL ExceptionDescribe(JNIEnv * UNREF env)
{
    TRACE2("jni", "ExceptionDescribe called");
    assert(hythread_is_suspend_enabled());
    if (exn_raised()) {
        exn_print_stack_trace(stderr, exn_get());
    }
} //ExceptionDescribe

void JNICALL ExceptionClear(JNIEnv * UNREF env)
{
    TRACE2("jni", "ExceptionClear called");
    assert(hythread_is_suspend_enabled());
    tmn_suspend_disable();
#ifdef _DEBUG
    if (!exn_raised())
    {
        TRACE2("jni", "Exception clear, no exception");
    }
    else
    {
        TRACE2("jni", "Exception clear, class = " << exn_get_name());
    }
#endif
    exn_clear();
    tmn_suspend_enable();
} //ExceptionClear

void JNICALL FatalError(JNIEnv * UNREF env, const char *msg)
{
    TRACE2("jni", "FatalError called");
    assert(hythread_is_suspend_enabled());
    DIE("\nFATAL error occurred in a JNI native method:\n\t" << msg);
} //FatalError

jobject JNICALL NewGlobalRef(JNIEnv * UNREF env, jobject obj)
{
    TRACE2("jni", "NewGlobalRef called");
    assert(hythread_is_suspend_enabled());
    if(!obj) {
        return 0;
    }

    assert(hythread_is_suspend_enabled());
    ObjectHandle new_handle = oh_allocate_global_handle();
    ObjectHandle old_handle = (ObjectHandle)obj;

    tmn_suspend_disable();       //---------------------------------v

    new_handle->object = old_handle->object;
    TRACE2("jni", "NewGlobalRef class = " << old_handle->object->vt()->clss);

    tmn_suspend_enable();        //---------------------------------^

    return (jobject)new_handle;
} //NewGlobalRef

void JNICALL DeleteGlobalRef(JNIEnv * UNREF env, jobject globalRef)
{
    TRACE2("jni", "DeleteGlobalRef called");
    assert(hythread_is_suspend_enabled());
#ifdef _DEBUG
    tmn_suspend_disable();
    ObjectHandle h = (ObjectHandle)globalRef;
    TRACE2("jni", "DeleteGlobalRef class = " << h->object->vt()->clss);
    tmn_suspend_enable();
#endif
    if (globalRef != NULL) {
        oh_deallocate_global_handle((ObjectHandle)globalRef);
    }
} //DeleteGlobalRef

jobject JNICALL NewLocalRef(JNIEnv *env, jobject ref)
{
    TRACE2("jni", "NewLocalRef called");
    assert(hythread_is_suspend_enabled());
    if (NULL == ref)
        return NULL;

    tmn_suspend_disable();
    ObjectHandle h = oh_allocate_local_handle();
    if (NULL == h)
    {
        tmn_suspend_enable();
        exn_raise_object(
            (jthrowable)(((JNIEnv_Internal*)env)->vm->vm_env->java_lang_OutOfMemoryError));
        return NULL;
    }

    h->object= ((ObjectHandle)ref)->object;
    TRACE2("jni", "NewLocalRef class = " << h->object->vt()->clss);
    tmn_suspend_enable();
    return (jobject)h;
}

void JNICALL DeleteLocalRef(JNIEnv * UNREF env, jobject localRef)
{
    TRACE2("jni", "DeleteLocalRef called");
    assert(hythread_is_suspend_enabled());
#ifdef _DEBUG
    tmn_suspend_disable();
    ObjectHandle h = (ObjectHandle)localRef;
    TRACE2("jni", "DeleteLocalRef class = " << h->object->vt()->clss);
    tmn_suspend_enable();
#endif
    if (localRef != NULL) {
        oh_discard_local_handle((ObjectHandle)localRef);
    }
} //DeleteLocalRef

jboolean JNICALL IsSameObject(JNIEnv * UNREF env,
                              jobject ref1,
                              jobject ref2)
{
    TRACE2("jni-same", "IsSameObject called");
    assert(hythread_is_suspend_enabled());
    if(ref1 == ref2) {
        return JNI_TRUE;
    }

    if(!(ref1 && ref2)) {
        // One reference is null and the other is not.
        return JNI_FALSE;
    }

    ObjectHandle h1 = (ObjectHandle)ref1;
    ObjectHandle h2 = (ObjectHandle)ref2;

    tmn_suspend_disable();       //---------------------------------v

    ManagedObject *java_ref1 = h1->object;
    ManagedObject *java_ref2 = h2->object;
    TRACE2("jni-same", "IsSameObject: Obj1 = " << java_ref1->vt()->clss->get_name()->bytes <<
        " Obj2 = " << java_ref2->vt()->clss->get_name()->bytes <<
        " objects are " << ((java_ref1 == java_ref2) ? "same" : "different"));
    jboolean result = (jboolean)((java_ref1 == java_ref2) ? JNI_TRUE : JNI_FALSE);

    tmn_suspend_enable();        //---------------------------------^

    return result;
} //IsSameObject

VMEXPORT jint JNICALL PushLocalFrame(JNIEnv * UNREF env, jint UNREF cap)
{
    TRACE2("jni", "PushLocalFrame called");
    assert(hythread_is_suspend_enabled());
    // 20020904: Not good for performance, but we can ignore local frames for now
    return 0;
}

VMEXPORT jobject JNICALL PopLocalFrame(JNIEnv * UNREF env, jobject res)
{
    TRACE2("jni", "PopLocalFrame called");
    assert(hythread_is_suspend_enabled());
    // 20020904: Not good for performance, but we can ignore local frames for now
    return res;
}


jint JNICALL EnsureLocalCapacity(JNIEnv* UNREF env, jint UNREF cap)
{
    TRACE2("jni", "EnsureLocalCapacity called");
    assert(hythread_is_suspend_enabled());
    return 0;
}


jobject JNICALL AllocObject(JNIEnv *env,
                            jclass clazz)
{
    TRACE2("jni", "AllocObject called");
    assert(hythread_is_suspend_enabled());
    assert(clazz);
    Class* clss = jclass_to_struct_Class(clazz);

    if(clss->is_interface() || clss->is_abstract()) {
        // Can't instantiate interfaces and abtract classes.
        ThrowNew_Quick(env, "java/lang/InstantiationException", clss->get_name()->bytes);
        return 0;
    }

    tmn_suspend_disable();       //---------------------------------v
    ObjectHandle new_handle = oh_allocate_local_handle();
    ManagedObject *new_obj = (ManagedObject *)class_alloc_new_object(clss);
    if (new_obj == NULL) {
        tmn_suspend_enable();
        return 0;

    }
    new_handle->object = (ManagedObject *)new_obj;
    tmn_suspend_enable();        //---------------------------------^

    return (jobject)new_handle;
} //AllocObject



jobject JNICALL NewObject(JNIEnv *env,
                          jclass clazz,
                          jmethodID methodID,
                          ...)
{
    TRACE2("jni", "NewObject called");
    assert(hythread_is_suspend_enabled());
    va_list args;
    va_start(args, methodID);
    return NewObjectV(env, clazz, methodID, args);
} //NewObject

jobject JNICALL NewObjectV(JNIEnv *env,
                           jclass clazz,
                           jmethodID methodID,
                           va_list args)
{
    TRACE2("jni", "NewObjectV called");
    assert(hythread_is_suspend_enabled());
    jvalue *jvalue_args = get_jvalue_arg_array((Method *)methodID, args);
    jobject result = NewObjectA(env, clazz, methodID, jvalue_args);
    STD_FREE(jvalue_args);
    return result;
} //NewObjectV

jobject JNICALL NewObjectA(JNIEnv *env,
                           jclass clazz,
                           jmethodID methodID,
                           jvalue *args)
{
    TRACE2("jni", "NewObjectA called");
    assert(hythread_is_suspend_enabled());
    // Allocate the object.
    jobject new_handle = AllocObject(env, clazz);

    if(!new_handle) {
        // Couldn't allocate the object.
        return 0;
    }

    CallNonvirtualVoidMethodA(env, new_handle, clazz, methodID, args);
    if (ExceptionCheck(env))
        return NULL;
    else
        return new_handle;
} //NewObjectA

jclass JNICALL GetObjectClass(JNIEnv * UNREF env,
                              jobject obj)
{
    TRACE2("jni", "GetObjectClass called");
    assert(hythread_is_suspend_enabled());
    // The spec guarantees that the reference is not null.
    assert(obj);
    ObjectHandle h = (ObjectHandle)obj;

    tmn_suspend_disable();       //---------------------------------v

    ObjectHandle new_handle = oh_allocate_local_handle();
    ManagedObject *jlo = h->object;
    assert(jlo);
    assert(jlo->vt());
    Class *clss = jlo->vt()->clss;
    TRACE2("jni", "GetObjectClass: class = " << clss->get_name()->bytes);
    new_handle->object= struct_Class_to_java_lang_Class(clss);
    tmn_suspend_enable();        //---------------------------------^

    return (jobject)new_handle;
} //GetObjectClass

jboolean JNICALL IsInstanceOf(JNIEnv *env,
                              jobject obj,
                              jclass clazz)
{
    TRACE2("jni", "IsInstanceOf called");
    assert(hythread_is_suspend_enabled());
    if(!obj) {
        return JNI_TRUE;
    }

    jclass obj_class = GetObjectClass(env, obj);

    Class* clss = jclass_to_struct_Class(clazz);
    Class* obj_clss = jclass_to_struct_Class(obj_class);

    Boolean isInstance = obj_clss->is_instanceof(clss);

    if(isInstance) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
} //IsInstanceOf

static bool
check_is_jstring_class(jstring string)
{
#ifndef NDEBUG
    ObjectHandle h = (ObjectHandle)string;

    tmn_suspend_disable();       //---------------------------------v

    ObjectHandle new_handle = oh_allocate_local_handle();
    ManagedObject *jlo = h->object;
    assert(jlo);
    assert(jlo->vt());
    Class *clss = jlo->vt()->clss;
    tmn_suspend_enable();        //---------------------------------^
    return clss == VM_Global_State::loader_env->JavaLangString_Class;
#else
    return true;
#endif // !NDEBUG
}

jstring JNICALL NewString(JNIEnv * UNREF env,
                          const jchar *unicodeChars,
                          jsize length)
{
    TRACE2("jni", "NewString called, length = " << length);
    assert(hythread_is_suspend_enabled());
    return (jstring)string_create_from_unicode_h(unicodeChars, length);
} //NewString

jsize JNICALL GetStringLength(JNIEnv * UNREF env,
                              jstring string)
{
    TRACE2("jni", "GetStringLength called");
    assert(hythread_is_suspend_enabled());
    if(!string)
        return 0;
    assert(check_is_jstring_class(string));
    return string_get_length_h((ObjectHandle)string);
} //GetStringLength

const jchar *JNICALL GetStringChars(JNIEnv * UNREF env,
                                    jstring string,
                                    jboolean *isCopy)
{
    TRACE2("jni", "GetStringChars called");
    assert(hythread_is_suspend_enabled());
    if(!string)
        return 0;
    assert(check_is_jstring_class(string));

    tmn_suspend_disable();
    ManagedObject* str = ((ObjectHandle)string)->object;
    uint16* chars = (uint16*)string_get_unicode_chars(str);
    tmn_suspend_enable();
    if (isCopy) *isCopy = JNI_TRUE;
    return chars;
} //GetStringChars

void JNICALL ReleaseStringChars(JNIEnv * UNREF env,
                                jstring string,
                                const jchar *chars)
{
    TRACE2("jni", "ReleaseStringChars called");
    if(!string)
        return;
    assert(check_is_jstring_class(string));
    assert(hythread_is_suspend_enabled());
    STD_FREE((void*)chars);
} //ReleaseStringChars

jstring JNICALL NewStringUTF(JNIEnv * UNREF env,
                             const char *bytes)
{
    TRACE2("jni", "NewStringUTF called, bytes = " << bytes);
    assert(hythread_is_suspend_enabled());
    return (jstring)string_create_from_utf8_h(bytes, (unsigned)strlen(bytes));
} //NewStringUTF

jsize JNICALL GetStringUTFLength(JNIEnv * UNREF env,
                                 jstring string)
{
    TRACE2("jni", "GetStringUTFLength called");
    if(!string)
        return 0;
    assert(check_is_jstring_class(string));
    assert(hythread_is_suspend_enabled());
    return string_get_utf8_length_h((ObjectHandle)string);
} //GetStringUTFLength

const char *JNICALL GetStringUTFChars(JNIEnv * UNREF env,
                                      jstring string,
                                      jboolean *isCopy)
{
    TRACE2("jni", "GetStringUTFChars called");
    if(!string)
        return 0;
    assert(check_is_jstring_class(string));
    assert(hythread_is_suspend_enabled());
    const char* res = string_get_utf8_chars_h((ObjectHandle)string);
    if (isCopy) *isCopy = JNI_TRUE;
    return res;
} //GetStringUTFChars

void JNICALL ReleaseStringUTFChars(JNIEnv * UNREF env,
                                   jstring string,
                                   const char *utf)
{
    TRACE2("jni", "ReleaseStringUTFChars called");
    if(!string)
        return;
    assert(check_is_jstring_class(string));
    assert(hythread_is_suspend_enabled());
    STD_FREE((void*)utf);
} //ReleaseStringUTFChars

jint JNICALL RegisterNatives(JNIEnv * UNREF env,
                             jclass clazz,
                             const JNINativeMethod *methods,
                             jint nMethods)
{
    TRACE2("jni", "RegisterNatives called");
    assert(hythread_is_suspend_enabled());
    Class_Handle clss = jclass_to_struct_Class(clazz);
    class_register_methods(clss, methods, nMethods);
    return class_register_methods(clss, methods, nMethods) ? -1 : 0;
} //RegisterNatives

jint JNICALL UnregisterNatives(JNIEnv * UNREF env, jclass clazz)
{
    TRACE2("jni", "UnregisterNatives called");
    assert(hythread_is_suspend_enabled());
    Class* clss = jclass_to_struct_Class(clazz);
    return class_unregister_methods(clss) ? -1 : 0;
} //UnregisterNatives

jint JNICALL MonitorEnter(JNIEnv * UNREF env, jobject obj)
{
    TRACE2("jni", "MonitorEnter called");
    assert(hythread_is_suspend_enabled());
    jthread_monitor_enter(obj);
    return exn_raised() ? -1 : 0;
} //MonitorEnter



jint JNICALL MonitorExit(JNIEnv * UNREF env, jobject obj)
{
    ASSERT_RAISE_AREA;

    TRACE2("jni", "MonitorExit called");
    assert(hythread_is_suspend_enabled());
    jthread_monitor_exit(obj);
    return exn_raised() ? -1 : 0;
} //MonitorExit



jint JNICALL GetJavaVM(JNIEnv *env_ext, JavaVM **vm)
{
    TRACE2("jni", "GetJavaVM called");
    assert(hythread_is_suspend_enabled());
    JNIEnv_Internal *env = (JNIEnv_Internal *)env_ext;
    *vm = env->vm;
    return JNI_OK;
} //GetJavaVM


void JNICALL GetStringRegion(JNIEnv * UNREF env, jstring s, jsize off, jsize len, jchar *b)
{
    TRACE2("jni", "GetStringRegion called");
    assert(hythread_is_suspend_enabled());
    assert(s);

    string_get_unicode_region_h((ObjectHandle)s, off, len, b);
}

void JNICALL GetStringUTFRegion(JNIEnv * UNREF env, jstring s, jsize off, jsize len, char *b)
{
    TRACE2("jni", "GetStringUTFRegion called");
    assert(hythread_is_suspend_enabled());
    string_get_utf8_region_h((ObjectHandle)s, off, len, b);
}

VMEXPORT void* JNICALL GetPrimitiveArrayCritical(JNIEnv* jenv, jarray array, jboolean* isCopy)
{
    TRACE2("jni", "GetPrimitiveArrayCritical called");
    assert(hythread_is_suspend_enabled());
    tmn_suspend_disable();
    Class* array_clss = ((ObjectHandle)array)->object->vt()->clss;
    tmn_suspend_enable();
    assert(array_clss->get_name()->bytes[0]=='[');

    TRACE2("jni.pin", "pinning array " << array->object);
    gc_pin_object((Managed_Object_Handle*)array);
    switch (array_clss->get_name()->bytes[1]) {
    case 'B':  return GetByteArrayElements(jenv, array, isCopy);
    case 'C':  return GetCharArrayElements(jenv, array, isCopy);
    case 'D':  return GetDoubleArrayElements(jenv, array, isCopy);
    case 'F':  return GetFloatArrayElements(jenv, array, isCopy);
    case 'I':  return GetIntArrayElements(jenv, array, isCopy);
    case 'J':  return GetLongArrayElements(jenv, array, isCopy);
    case 'S':  return GetShortArrayElements(jenv, array, isCopy);
    case 'Z':  return GetBooleanArrayElements(jenv, array, isCopy);
    default:   ABORT("Wrong array type descriptor"); return NULL;
    }
}

VMEXPORT void JNICALL ReleasePrimitiveArrayCritical(JNIEnv* jenv, jarray array, void* carray, jint mode)
{
    TRACE2("jni", "ReleasePrimitiveArrayCritical called");
    assert(hythread_is_suspend_enabled());
    tmn_suspend_disable();
    Class* array_clss = ((ObjectHandle)array)->object->vt()->clss;
    tmn_suspend_enable();
    assert(array_clss->get_name()->bytes[0]=='[');
    switch (array_clss->get_name()->bytes[1]) {
    case 'B':  ReleaseByteArrayElements(jenv, array, (jbyte*)carray, mode); break;
    case 'C':  ReleaseCharArrayElements(jenv, array, (jchar*)carray, mode); break;
    case 'D':  ReleaseDoubleArrayElements(jenv, array, (jdouble*)carray, mode); break;
    case 'F':  ReleaseFloatArrayElements(jenv, array, (jfloat*)carray, mode); break;
    case 'I':  ReleaseIntArrayElements(jenv, array, (jint*)carray, mode); break;
    case 'J':  ReleaseLongArrayElements(jenv, array, (jlong*)carray, mode); break;
    case 'S':  ReleaseShortArrayElements(jenv, array, (jshort*)carray, mode); break;
    case 'Z':  ReleaseBooleanArrayElements(jenv, array, (jboolean*)carray, mode); break;
    default:   ABORT("Wrong array type descriptor"); break;
    }
    if (mode != JNI_COMMIT) {
        TRACE2("jni.pin", "unpinning array " << array->object);
        gc_unpin_object((Managed_Object_Handle*)array);
    }
}

const jchar* JNICALL GetStringCritical(JNIEnv *env, jstring s, jboolean* isCopy)
{
    TRACE2("jni", "GetStringCritical called");
    assert(hythread_is_suspend_enabled());
    return GetStringChars(env, s, isCopy);
}

void JNICALL ReleaseStringCritical(JNIEnv *env, jstring s, const jchar* cstr)
{
    TRACE2("jni", "ReleaseStringCritical called");
    assert(hythread_is_suspend_enabled());
    ReleaseStringChars(env, s, cstr);
}

VMEXPORT jweak JNICALL NewWeakGlobalRef(JNIEnv *env, jobject obj)
{
    TRACE2("jni", "NewWeakGlobalRef called");
    assert(hythread_is_suspend_enabled());
    return NewGlobalRef(env, obj);
}

VMEXPORT void JNICALL DeleteWeakGlobalRef(JNIEnv *env, jweak obj)
{
    TRACE2("jni", "DeleteWeakGlobalRef called");
    assert(hythread_is_suspend_enabled());
    DeleteGlobalRef(env, obj);
}

jboolean JNICALL ExceptionCheck(JNIEnv * UNREF env)
{
    TRACE2("jni", "ExceptionCheck called, exception status = " << exn_raised());
    assert(hythread_is_suspend_enabled());
    if (exn_raised())
        return JNI_TRUE;
    else
        return JNI_FALSE;
}

VMEXPORT jmethodID JNICALL FromReflectedMethod(JNIEnv *env, jobject method)
{
    TRACE2("jni", "FromReflectedMethod called");
    Class* clss = jobject_to_struct_Class(method);

    if (clss == VM_Global_State::loader_env->java_lang_reflect_Constructor_Class) 
    {
        static jmethodID m = (jmethodID)class_lookup_method(clss, "getId", "()J");
        return (jmethodID) ((POINTER_SIZE_INT) CallLongMethodA(env, method, m, 0));
    } 
    else if (clss == VM_Global_State::loader_env->java_lang_reflect_Method_Class)
    {
        static jmethodID m = (jmethodID)class_lookup_method(clss, "getId", "()J");
        return (jmethodID) ((POINTER_SIZE_INT) CallLongMethodA(env, method, m, 0));
    }
    else
        return NULL;
}

VMEXPORT jfieldID JNICALL FromReflectedField(JNIEnv *env, jobject field)
{
    TRACE2("jni", "FromReflectedField called");
    Class* clss = jobject_to_struct_Class(field);

    if (clss == VM_Global_State::loader_env->java_lang_reflect_Field_Class) 
    {
        static jmethodID m = (jmethodID)class_lookup_method(clss, "getId", "()J");
        return (jfieldID) ((POINTER_SIZE_INT) CallLongMethodA(env, field, m, 0));
    }
    else
        return NULL;
}

VMEXPORT jobject JNICALL ToReflectedMethod(JNIEnv *env, jclass UNREF cls, jmethodID methodID,
                                            jboolean isStatic)
{
    TRACE2("jni", "ToReflectedMethod called");
    Method *m = (Method*)methodID;
    if ((bool)m->is_static() != (bool)isStatic) // True if flags are different
        return NULL;

    if (m->is_init())
        return reflection_reflect_constructor(env, m);
    else
        return reflection_reflect_method(env, m);
}

VMEXPORT jobject JNICALL ToReflectedField(JNIEnv *env, jclass UNREF cls, jfieldID fieldID,
                                           jboolean isStatic)
{
    TRACE2("jni", "ToReflectedField called");
    Field *f = (Field*)fieldID;
    if ((bool)f->is_static() != (bool)isStatic) // True if flags are different
        return NULL;

    return reflection_reflect_field(env, fieldID);
}

jobject JNICALL NewDirectByteBuffer(JNIEnv* env, void* address, jlong capacity)
{
    //no-impl stub replaced by classlib's func at VM startup
    return NULL;
}

void* JNICALL GetDirectBufferAddress(JNIEnv* env, jobject buf)
{
    //no-impl stub replaced by classlib's func at VM startup
    return NULL; 
}

jlong JNICALL GetDirectBufferCapacity(JNIEnv* env, jobject buf)
{
    //no-impl stub replaced by classlib's func at VM startup
    return -1;
}

/*    BEGIN: Invocation API functions.    */

VMEXPORT jint JNICALL DestroyJavaVM(JavaVM * vm)
{
    char * name = "destroy";
    jboolean daemon = JNI_FALSE;
    jthread java_thread;
    JavaVM_Internal * java_vm;
    JNIEnv * jni_env;
    jint status;

    TRACE2("jni", "DestroyJavaVM  called");

    java_vm = (JavaVM_Internal *) vm;

    java_thread = jthread_self();
    if (java_thread == NULL) {
        // Attaches main thread to VM.
        status = vm_attach_internal(&jni_env, &java_thread, java_vm, NULL, name, daemon);
        if (status != JNI_OK) return status;

        status = jthread_attach(jni_env, java_thread, daemon);
        if (status != TM_ERROR_NONE) return JNI_ERR;
        // Now JVMTIThread keeps global reference. Discared temporary global reference.
        jni_env->DeleteGlobalRef(java_thread);

        java_thread = jthread_self();        
    }    
    assert(java_thread != NULL);

    apr_thread_mutex_lock(GLOBAL_LOCK);
    
    status = vm_destroy(java_vm, java_thread);
    if (status != JNI_OK) return status;

    APR_RING_REMOVE(java_vm, link);
    
    // Destroy VM environment.
    delete java_vm->vm_env;
    
    // Destroy VM pool.
    apr_pool_destroy(java_vm->pool);
    
    // TODO: Destroy globals if it is last VM.

    apr_thread_mutex_unlock(GLOBAL_LOCK);
    
    // TODO: error code should be returned until
    // VM cleanups its internals properly.
    return JNI_ERR;
}

static jint attach_current_thread(JavaVM * java_vm, void ** p_jni_env, void * args, jboolean daemon)
{
    char * name;
    jobject group;
    JNIEnv * jni_env;
    JavaVMAttachArgs * jni_1_2_args;
    jthread java_thread;
    IDATA status; 

    TRACE2("jni", "AttachCurrentThread called");
    
    if (jthread_self()) {
        *p_jni_env = jthread_get_JNI_env(jthread_self());
        return JNI_OK;
    }

    name = NULL;
    group = NULL;

    if (args != NULL) {
        jni_1_2_args = (JavaVMAttachArgs *) args;
        if (jni_1_2_args->version != JNI_VERSION_1_2) {
            return JNI_EVERSION;
        }
        name = jni_1_2_args->name;
        group = jni_1_2_args->group;
    }

    // Attaches current thread to VM.
    status = vm_attach_internal(&jni_env, &java_thread, java_vm, group, name, daemon);
    if (status != JNI_OK) return status;

    *p_jni_env = jni_env;

    // Attaches current thread to TM.
    status = jthread_attach(jni_env, java_thread, daemon);
    assert(jthread_self() != NULL);

    // Now JVMTIThread keeps global reference. Discared temporary global reference.
   jni_env->DeleteGlobalRef(java_thread);

    // Send thread start event.
    // TODO: Thread start event should be sent before its initial method executes.
    jvmti_send_thread_start_end_event(1);

    return status == TM_ERROR_NONE ? JNI_OK : JNI_ERR;
}

VMEXPORT jint JNICALL AttachCurrentThread(JavaVM * vm, void ** p_jni_env, void * args)
{
    return attach_current_thread(vm, p_jni_env, args, JNI_FALSE);
}

VMEXPORT jint JNICALL AttachCurrentThreadAsDaemon(JavaVM * vm, void ** p_jni_env, void * args)
{
    return attach_current_thread(vm, p_jni_env, args, JNI_TRUE);
}

VMEXPORT jint JNICALL DetachCurrentThread(JavaVM * vm)
{
    jthread java_thread;
    IDATA status;
    
    java_thread = jthread_self();
    if (java_thread == NULL) return JNI_EDETACHED;

    status = jthread_detach(java_thread);
    
    return status == TM_ERROR_NONE ? JNI_OK : JNI_ERR;
}

VMEXPORT jint JNICALL GetEnv(JavaVM * vm, void ** penv, jint ver)
{
    VM_thread * vm_thread;

    TRACE2("jni", "GetEnv called, ver = " << ver);
    assert(hythread_is_suspend_enabled());

    vm_thread = p_TLS_vmthread;
    if (vm_thread == NULL) return JNI_EDETACHED;

    if ((ver & JVMTI_VERSION_MASK_INTERFACE_TYPE) == JVMTI_VERSION_INTERFACE_JNI) {
        switch (ver) {
            case JNI_VERSION_1_1:
            case JNI_VERSION_1_2:
            case JNI_VERSION_1_4:
                *penv = (void*)vm_thread->jni_env;
                return JNI_OK;
        }
    } else if((ver & JVMTI_VERSION_MASK_INTERFACE_TYPE) == JVMTI_VERSION_INTERFACE_JVMTI) {
        return create_jvmti_environment(vm, penv, ver);
    } else if((ver & JVMTI_VERSION_MASK_INTERFACE_TYPE) == 0x10000000) {
        WARN("GetEnv requested unsupported JVMPI environment!! Only JVMTI is supported by VM.");
    } else if((ver & JVMTI_VERSION_MASK_INTERFACE_TYPE) == 0x20000000) {
        WARN("GetEnv requested unsupported JVMDI environment!! Only JVMTI is supported by VM.");
    } else {
        WARN("GetEnv called with unsupported interface version 0x" << ((void *)((POINTER_SIZE_INT)ver)));
    }

    *penv = NULL;
    return JNI_EVERSION;
}

/*    END: Invocation API functions.    */

/* Global Handles: see jni_utils.h for more information about them */
ObjectHandle gh_jlc;
ObjectHandle gh_jls = 0;
ObjectHandle gh_jlcloneable;
ObjectHandle gh_aoboolean;
ObjectHandle gh_aobyte;
ObjectHandle gh_aochar;
ObjectHandle gh_aoshort;
ObjectHandle gh_aoint;
ObjectHandle gh_aolong;
ObjectHandle gh_aofloat;
ObjectHandle gh_aodouble;

ObjectHandle gh_jlboolean;
ObjectHandle gh_jlbyte;
ObjectHandle gh_jlchar;
ObjectHandle gh_jlshort;
ObjectHandle gh_jlint;
ObjectHandle gh_jllong;
ObjectHandle gh_jlfloat;
ObjectHandle gh_jldouble;

jfieldID gid_boolean_value;  
jfieldID gid_byte_value;     
jfieldID gid_char_value;     
jfieldID gid_short_value;    
jfieldID gid_int_value;      
jfieldID gid_long_value;     
jfieldID gid_float_value;    
jfieldID gid_double_value;   

jfieldID gid_throwable_traceinfo;

jfieldID gid_string_field_value;
jfieldID gid_string_field_bvalue;
jfieldID gid_string_field_offset;
jfieldID gid_string_field_count;

jmethodID gid_stringinit = 0;
jmethodID gid_doubleisNaN = 0;
jdouble gc_double_POSITIVE_INFINITY = 0;
jdouble gc_double_NEGATIVE_INFINITY = 0;

// TODO: should return error code instead of exiting.
static void check_for_unexpected_exception(){
    assert(hythread_is_suspend_enabled());
    if (exn_raised()) {
        print_uncaught_exception_message(stderr, "static initializing", exn_get());
        DIE("Error initializing java machine\n");
    }
}

void global_object_handles_init(JNIEnv* jni_env)
{
    Global_Env* vm_env = jni_get_vm_env(jni_env);

    gh_jlc = oh_allocate_global_handle();
    gh_jls = oh_allocate_global_handle();
    gh_jlcloneable = oh_allocate_global_handle();
    gh_aoboolean = oh_allocate_global_handle();
    gh_aobyte = oh_allocate_global_handle();
    gh_aochar = oh_allocate_global_handle();
    gh_aoshort = oh_allocate_global_handle();
    gh_aoint= oh_allocate_global_handle();
    gh_aolong = oh_allocate_global_handle();
    gh_aofloat= oh_allocate_global_handle();
    gh_aodouble = oh_allocate_global_handle();

    gh_jlboolean = oh_allocate_global_handle();
    gh_jlbyte = oh_allocate_global_handle();
    gh_jlchar = oh_allocate_global_handle();
    gh_jlshort = oh_allocate_global_handle();
    gh_jlint = oh_allocate_global_handle();
    gh_jllong = oh_allocate_global_handle();
    gh_jlfloat = oh_allocate_global_handle();
    gh_jldouble = oh_allocate_global_handle();
    ObjectHandle h_jlt = oh_allocate_global_handle();

    tmn_suspend_disable();  // --------------vvv
    gh_jlc->object = struct_Class_to_java_lang_Class(vm_env->JavaLangClass_Class);
    gh_jls->object = struct_Class_to_java_lang_Class(vm_env->JavaLangString_Class);
    gh_jlcloneable->object = struct_Class_to_java_lang_Class(vm_env->java_lang_Cloneable_Class);
    gh_aoboolean->object = struct_Class_to_java_lang_Class(vm_env->ArrayOfBoolean_Class);
    gh_aobyte->object = struct_Class_to_java_lang_Class(vm_env->ArrayOfByte_Class);
    gh_aochar->object = struct_Class_to_java_lang_Class(vm_env->ArrayOfChar_Class);
    gh_aoshort->object = struct_Class_to_java_lang_Class(vm_env->ArrayOfShort_Class);
    gh_aoint->object = struct_Class_to_java_lang_Class(vm_env->ArrayOfInt_Class);
    gh_aolong->object = struct_Class_to_java_lang_Class(vm_env->ArrayOfLong_Class);
    gh_aofloat->object = struct_Class_to_java_lang_Class(vm_env->ArrayOfFloat_Class);
    gh_aodouble->object = struct_Class_to_java_lang_Class(vm_env->ArrayOfDouble_Class);
    tmn_suspend_enable();    // -------------^^^

    Class* jlboolean = vm_env->LoadCoreClass("java/lang/Boolean");
    Class* jlbyte = vm_env->LoadCoreClass("java/lang/Byte");
    Class* jlchar = vm_env->LoadCoreClass("java/lang/Character");
    Class* jlshort = vm_env->LoadCoreClass("java/lang/Short");
    Class* jlint = vm_env->LoadCoreClass("java/lang/Integer");
    Class* jllong = vm_env->LoadCoreClass("java/lang/Long");
    Class* jlfloat = vm_env->LoadCoreClass("java/lang/Float");
    Class* jldouble = vm_env->LoadCoreClass("java/lang/Double");

    tmn_suspend_disable();
    gh_jlboolean->object = struct_Class_to_java_lang_Class(jlboolean);
    gh_jlbyte->object = struct_Class_to_java_lang_Class(jlbyte);
    gh_jlchar->object = struct_Class_to_java_lang_Class(jlchar);
    gh_jlshort->object = struct_Class_to_java_lang_Class(jlshort);
    gh_jlint->object = struct_Class_to_java_lang_Class(jlint);
    gh_jllong->object = struct_Class_to_java_lang_Class(jllong);
    gh_jlfloat->object = struct_Class_to_java_lang_Class(jlfloat);
    gh_jldouble->object = struct_Class_to_java_lang_Class(jldouble);
    h_jlt->object= struct_Class_to_java_lang_Class(vm_env->java_lang_Throwable_Class);
    tmn_suspend_enable(); //-------------------------------------------------------^

    assert(hythread_is_suspend_enabled());

    gid_throwable_traceinfo = jni_env->GetFieldID((jclass)h_jlt, "vm_stacktrace", "[J");
    assert(hythread_is_suspend_enabled());

    gid_boolean_value = jni_env->GetFieldID((jclass)gh_jlboolean, "value", "Z");   
    gid_byte_value = jni_env->GetFieldID((jclass)gh_jlbyte, "value", "B");        
    gid_char_value = jni_env->GetFieldID((jclass)gh_jlchar, "value", "C");       
    gid_short_value = jni_env->GetFieldID((jclass)gh_jlshort, "value", "S");      
    gid_int_value = jni_env->GetFieldID((jclass)gh_jlint, "value", "I");        
    gid_long_value = jni_env->GetFieldID((jclass)gh_jllong, "value", "J");       
    gid_float_value = jni_env->GetFieldID((jclass)gh_jlfloat, "value", "F");     
    gid_double_value = jni_env->GetFieldID((jclass)gh_jldouble, "value", "D");   
    assert(hythread_is_suspend_enabled());

    gid_doubleisNaN = jni_env->GetStaticMethodID((jclass)gh_jldouble, "isNaN", "(D)Z");

    gid_stringinit = jni_env->GetMethodID((jclass)gh_jls, "<init>", "([C)V");
    gid_string_field_value = jni_env->GetFieldID((jclass)gh_jls, "value", "[C");

    if (vm_env->strings_are_compressed) {
        gid_string_field_bvalue = jni_env->GetFieldID((jclass)gh_jls, "bvalue", "[B");
    }

    gid_string_field_offset = jni_env->GetFieldID((jclass)gh_jls, "offset", "I");
    gid_string_field_count = jni_env->GetFieldID((jclass)gh_jls, "count", "I");    
    assert(hythread_is_suspend_enabled());

    oh_deallocate_global_handle(h_jlt);
    assert(hythread_is_suspend_enabled());

#ifdef USE_NATIVE_ISARRAY
    field_name = env->string_pool.lookup("isArray");
    field_descr = env->string_pool.lookup("()Z");
    //gsig_ClassisArray = env->sig_table.lookup(field_name, field_descr);
#endif //#ifndef USE_NATIVE_ISARRAY
    assert(hythread_is_suspend_enabled());
}


void unsafe_global_object_handles_init(JNIEnv * jni_env) {
    assert(!hythread_is_suspend_enabled());
    tmn_suspend_enable();
   
    jfieldID POSITIVE_INFINITY_id = jni_env->GetStaticFieldID((jclass)gh_jldouble, "POSITIVE_INFINITY", "D");
    gc_double_POSITIVE_INFINITY = jni_env->GetStaticDoubleField((jclass)gh_jldouble, POSITIVE_INFINITY_id);
    check_for_unexpected_exception();
    assert(hythread_is_suspend_enabled());
    
    jfieldID NEGATIVE_INFINITY_id = jni_env->GetStaticFieldID((jclass)gh_jldouble, "NEGATIVE_INFINITY", "D");      
    gc_double_NEGATIVE_INFINITY = jni_env->GetStaticDoubleField((jclass)gh_jldouble, NEGATIVE_INFINITY_id);    
    assert(hythread_is_suspend_enabled());
     check_for_unexpected_exception();
     assert(hythread_is_suspend_enabled());
    tmn_suspend_disable();
}
