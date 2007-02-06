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
 * @author Andrey Yakushev
 * @version $Revision$
 */

/**
 * @file org_apache_harmony_lang_management_MemoryMXBeanImpl.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of
 * org.apache.harmony.lang.management.MemoryMXBeanImpl class.
 */

#include "org_apache_harmony_lang_management_MemoryMXBeanImpl.h"
#include <cxxlog.h>
#include "environment.h"
/* Header for class org_apache_harmony_lang_management_MemoryMXBeanImpl */

/*
 * Class:     org_apache_harmony_lang_management_MemoryMXBeanImpl
 * Method:    createMemoryManagers
 * Signature: ()V
 * IMPORTANT : VM is the sole caller of this method.
 */
JNIEXPORT void JNICALL Java_org_apache_harmony_lang_management_MemoryMXBeanImpl_createMemoryManagers
(JNIEnv * jenv_ext, jobject obj)
{
    // TODO implement this method stub correctly
    TRACE2("management","createMemoryManagers stub invocation");

    JNIEnv_Internal *jenv = (JNIEnv_Internal *)jenv_ext;

    jclass memoryMXBeanImplClazz =jenv->FindClass("org/apache/harmony/lang/management/MemoryMXBeanImpl");
    if (jenv->ExceptionCheck()) {return;};
    jmethodID createMemoryManagerHelperMethod = jenv->GetMethodID(
        memoryMXBeanImplClazz,
        "createMemoryManagerHelper",
        "(Ljava/lang/String;IZ)V");
    if (jenv->ExceptionCheck()) {return;};

    jobject nameGCMM = jenv->NewStringUTF("GCMemoryManager");
    if (jenv->ExceptionCheck()) {return;};
    jenv->CallVoidMethod(obj, createMemoryManagerHelperMethod, nameGCMM, 1, JNI_TRUE);
    if (jenv->ExceptionCheck()) {return;};

    jobject nameNMM = jenv->NewStringUTF("NativeMemoryManager");
    if (jenv->ExceptionCheck()) {return;};
    jenv->CallVoidMethod(obj, createMemoryManagerHelperMethod, nameNMM, 1, JNI_FALSE);
};

/*
 * Class:     org_apache_harmony_lang_management_MemoryMXBeanImpl
 * Method:    getHeapMemoryUsageImpl
 * Signature: ()Ljava/lang/management/MemoryUsage;
 */
JNIEXPORT jobject JNICALL Java_org_apache_harmony_lang_management_MemoryMXBeanImpl_getHeapMemoryUsageImpl
(JNIEnv * jenv_ext, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getHeapMemoryUsageImpl stub invocation");

    JNIEnv_Internal *jenv = (JNIEnv_Internal *)jenv_ext;

    jlong init = 1L<<21;
    jlong used = 1L<<20;
    jlong committed = 1L<<20;
    jlong max = 1L<<22;

    jclass memoryUsageClazz =jenv->FindClass("java/lang/management/MemoryUsage");
    if (jenv->ExceptionCheck()) {return NULL;};
    jmethodID memoryUsageClazzConstructor = jenv->GetMethodID(memoryUsageClazz, "<init>", "(JJJJ)V");
    if (jenv->ExceptionCheck()) {return NULL;};

    jobject memoryUsage = jenv->NewObject(memoryUsageClazz, memoryUsageClazzConstructor, init, used,
        committed, max);

    return memoryUsage;
};

/*
 * Class:     org_apache_harmony_lang_management_MemoryMXBeanImpl
 * Method:    getNonHeapMemoryUsageImpl
 * Signature: ()Ljava/lang/management/MemoryUsage;
 */
JNIEXPORT jobject JNICALL Java_org_apache_harmony_lang_management_MemoryMXBeanImpl_getNonHeapMemoryUsageImpl
(JNIEnv * jenv_ext, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getNonHeapMemoryUsageImpl stub invocation");
    Global_Env* genv = VM_Global_State::loader_env;

    JNIEnv_Internal *jenv = (JNIEnv_Internal *)jenv_ext;

    jlong init = 1L<<21;
    jlong used = 1L<<20;
    jlong committed = 1L<<20;
    jlong max = 1L<<22;

    jclass memoryUsageClazz =jenv->FindClass("java/lang/management/MemoryUsage");
    if (jenv->ExceptionCheck()) {return NULL;};
    jmethodID memoryUsageClazzConstructor = jenv->GetMethodID(memoryUsageClazz, "<init>", "(JJJJ)V");
    if (jenv->ExceptionCheck()) {return NULL;};

    jobject memoryUsage = jenv->NewObject(memoryUsageClazz, memoryUsageClazzConstructor, init, used,
        committed, max);

    return memoryUsage;
};

/*
 * Class:     org_apache_harmony_lang_management_MemoryMXBeanImpl
 * Method:    getObjectPendingFinalizationCountImpl
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_apache_harmony_lang_management_MemoryMXBeanImpl_getObjectPendingFinalizationCountImpl
(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getObjectPendingFinalizationCountImp stub invocation");
    return 20;
}

jboolean memory_bean_verbose = JNI_TRUE;

/*
 * Class:     org_apache_harmony_lang_management_MemoryMXBeanImpl
 * Method:    isVerboseImpl
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_harmony_lang_management_MemoryMXBeanImpl_isVerboseImpl
(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","MemoryMXBeanImpl_isVerboseImpl stub invocation");
    return memory_bean_verbose;
};

/*
 * Class:     org_apache_harmony_lang_management_MemoryMXBeanImpl
 * Method:    setVerboseImpl
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_org_apache_harmony_lang_management_MemoryMXBeanImpl_setVerboseImpl
(JNIEnv *, jobject, jboolean newValue)
{
    // TODO implement this method stub correctly
    TRACE2("management","MemoryMXBeanImpl_setVerboseImpl stub invocation");
    memory_bean_verbose = newValue;
};

