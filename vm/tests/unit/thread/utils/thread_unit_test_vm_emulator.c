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

#include "testframe.h"
#include "thread_unit_test_utils.h"
#include <assert.h>
#include <apr-1/apr_pools.h>
#include <open/hythread.h>
#include <open/hythread_ext.h>
#include <open/thread_externals.h>
#include "thread_private.h"

/*
 * VM emulator for thread manager unit tests 
 */

int vm_objects_are_equal(jobject obj1, jobject obj2);

jmethodID isDaemonID = (jmethodID)&isDaemonID;
jmethodID runID = (jmethodID)&runID;
jmethodID setDaemonID = (jmethodID)&setDaemonID;
struct JNINativeInterface_ jni_vtable;
JNIEnv common_env = & jni_vtable;
apr_pool_t *pool = NULL;

jthread new_jobject(){

    apr_status_t status;
    _jobject *obj;
    _jjobject *object;

    if (!pool){
        status = apr_pool_create(&pool, NULL);
        if (status) return NULL; 
    }

    obj = apr_palloc(pool, sizeof(_jobject));
    object = apr_palloc(pool, sizeof(_jjobject));
    assert(obj);
    obj->object = object;
    obj->object->data = NULL;
    obj->object->daemon = 0;
    obj->object->name = NULL;
    return obj;
}

void delete_jobject(jobject obj){

}

JNIEnv * new_JNIEnv(){
    return &common_env;
}

void delete_JNIEnv(JNIEnv *env){
}

jclass JNICALL FindClass(JNIEnv *env, const char *name){
    return new_jobject();
}

jmethodID JNICALL GetMethodID(JNIEnv *env, jclass clazz,
                              const char *name, const char *sig){
    if (! strcmp(name, "isDaemon")){
        return isDaemonID;
    } else if (! strcmp(name, "setDaemon")){
        return setDaemonID;
    } else if (! strcmp(name, "runImpl")){
        return runID;
    }
    log_info("GetMethodID emulator: UNKNOWN METHOD");
    assert(0);
    return 0;
}

jboolean JNICALL CallBooleanMethodA(JNIEnv *env, jobject obj, 
                                    jmethodID methodID,  jvalue * args){
    if (methodID == isDaemonID){
        return obj->object->daemon;
    }
    log_info("CallBooleanMethodA emulator: UNKNOWN METHOD");
    assert(0);
    return 0;
}

//void JNICALL CallVoidMethod(JNIEnv *env, jobject obj, jmethodID methodID, ...){
//
//    printf("CallVoidMethod...\n");
//}
//
void JNICALL CallVoidMethodA(JNIEnv *env, jobject obj,
                             jmethodID methodID, jvalue * args){

    if (methodID == setDaemonID){
        obj->object->daemon = args[0].z;
    } else if (methodID == runID){
        obj->object->run_method();
    } else {
        log_info("CallVoidMethodA emulator: UNKNOWN METHOD");
        assert(0);
    }
}

const char* JNICALL GetStringUTFChars(JNIEnv *env, jstring str, jboolean *isCopy){
    return str->object->name;
}

jstring JNICALL NewStringUTF(JNIEnv *env, const char * name){

    jstring jname = new_jobject();
    jname->object->name = (char *)name;
    return jname;
}

jobject JNICALL NewGlobalRef(JNIEnv *env, jobject object){
    jobject obj = new_jobject();
    assert(obj);
    assert(object);
    obj->object = object->object;
    return obj;
}

void JNICALL DeleteGlobalRef(JNIEnv *env, jobject object){
}

void JNICALL ReleaseStringUTFChars(JNIEnv *env, jstring str, const char* chars){
}

jint JNICALL ThrowNew (JNIEnv *env, jclass clazz, const char* chars){
    tested_thread_sturct_t * tts;
    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;

    tm_native_thread = hythread_self();
    tm_java_thread = hythread_get_private_data(tm_native_thread);

    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        if (vm_objects_are_equal(tts->java_thread, tm_java_thread->thread_object)){
            tts->excn = clazz;
            return 0;
        }
    }
    return 1;
}

jint JNICALL Throw (JNIEnv *env, jobject clazz){
    tested_thread_sturct_t * tts;
    hythread_t tm_native_thread;
    jvmti_thread_t tm_java_thread;
    
    tm_native_thread = hythread_self();
    tm_java_thread = hythread_get_private_data(tm_native_thread);
    
    reset_tested_thread_iterator(&tts);
    while(next_tested_thread(&tts)){
        if (vm_objects_are_equal(tts->java_thread, tm_java_thread->thread_object)){
            tts->excn = clazz;
            return 0;
        }
    }
    return 1;
}   


void jni_init(){

    jni_vtable.FindClass = &FindClass;
    jni_vtable.GetMethodID = &GetMethodID;
    jni_vtable.CallBooleanMethodA = &CallBooleanMethodA;
    //jni_vtable.CallVoidMethod = &CallVoidMethod;
    jni_vtable.CallVoidMethodA = &CallVoidMethodA;
    jni_vtable.GetStringUTFChars = &GetStringUTFChars;
    jni_vtable.NewStringUTF = &NewStringUTF;
    jni_vtable.DeleteGlobalRef = &DeleteGlobalRef;
    jni_vtable.NewGlobalRef = &NewGlobalRef;
    jni_vtable.ReleaseStringUTFChars = &ReleaseStringUTFChars;
    jni_vtable.ThrowNew = ThrowNew;
    jni_vtable.Throw = Throw;
}

