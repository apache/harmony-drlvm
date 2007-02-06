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
 * @file org_apache_harmony_lang_management_ThreadMXBeanImpl.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of
 * org.apache.harmony.lang.management.ThreadMXBeanImpl class.
 */

#include <jni.h>
#include <cxxlog.h>
#include "exceptions.h"
#include "environment.h"
#include "java_lang_System.h"
#include "org_apache_harmony_lang_management_ThreadMXBeanImpl.h"

/* Native methods */

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.findMonitorDeadlockedThreadsImpl()[J
 */
JNIEXPORT jlongArray JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_findMonitorDeadlockedThreadsImpl(JNIEnv *jenv, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","findMonitorDeadlockedThreadsImpl stub invocation");
    jlongArray array = jenv->NewLongArray(0);

    return array;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getAllThreadIdsImpl()[J
 */
JNIEXPORT jlongArray JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getAllThreadIdsImpl(JNIEnv *jenv_ext, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getAllThreadIdsImpl stub invocation");
    JNIEnv_Internal *jenv = (JNIEnv_Internal *)jenv_ext;

    jlongArray array = jenv->NewLongArray(1);
    if (jenv->ExceptionCheck()) return NULL;

    jclass threadClazz =jenv->FindClass("java/lang/Thread");
    if (jenv->ExceptionCheck()) return NULL;

    jmethodID currentThreadMethod = jenv->GetStaticMethodID(threadClazz, "currentThread",
        "()Ljava/lang/Thread;");
    if (jenv->ExceptionCheck()) return NULL;

    jobject currentThread = jenv->CallStaticObjectMethod(threadClazz, currentThreadMethod, NULL);
    if (jenv->ExceptionCheck()) return NULL;

    jmethodID getIdMethod = jenv->GetMethodID(threadClazz, "getId", "()J");
    if (jenv->ExceptionCheck()) return NULL;

    jlong id = jenv->CallLongMethod(currentThread, getIdMethod);
    if (jenv->ExceptionCheck()) return NULL;

    jenv->SetLongArrayRegion(array, 0, 1, &id);

    return array;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getDaemonThreadCountImpl()I
 */
JNIEXPORT jint JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getDaemonThreadCountImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getDaemonThreadCountImpl stub invocation");
    return 0;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getPeakThreadCountImpl()I
 */
JNIEXPORT jint JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getPeakThreadCountImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getPeakThreadCountImpl stub invocation");
    return 1;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getThreadCountImpl()I
 */
JNIEXPORT jint JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getThreadCountImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getThreadCountImpl stub invocation");
    return 1;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getThreadCpuTimeImpl(J)J
 */
JNIEXPORT jlong JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getThreadCpuTimeImpl(JNIEnv *jenv, jobject,
                                                                              jlong id)
{
    // TODO implement this method stub correctly
    TRACE2("management","getThreadCpuTimeImpl stub invocation");
    if (id <= 0) {
        TRACE2("management","getThreadCpuTimeImpl java/lang/IllegalArgumentException is thrown");
        ThrowNew_Quick(jenv, "java/lang/IllegalArgumentException", "id <= 0");
    };

    return 1L<<10;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getThreadByIdImpl(J)Ljava/lang/Thread;
 */
JNIEXPORT jobject JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getThreadByIdImpl(
    JNIEnv *jenv_ext,
    jobject,
    jlong)
{
    // TODO implement this method stub correctly
    TRACE2("management","getThreadByIdImpl stub invocation");

    JNIEnv_Internal *jenv = (JNIEnv_Internal *)jenv_ext;

    jclass threadClazz =jenv->FindClass("java/lang/Thread");
    if (jenv->ExceptionCheck()) return NULL;

    jmethodID currentThreadMethod = jenv->GetStaticMethodID(threadClazz, "currentThread",
        "()Ljava/lang/Thread;");
    if (jenv->ExceptionCheck()) return NULL;

    jobject jresult = jenv->CallStaticObjectMethod(threadClazz, currentThreadMethod, NULL);
    return jresult;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getObjectThreadIsBlockedOnImpl(Ljava/lang/Thread;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getObjectThreadIsBlockedOnImpl(JNIEnv *, jobject,
                                                                                        jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getObjectThreadIsBlockedOnImpl stub invocation");
    return NULL;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getThreadOwningObjectImpl(Ljava/lang/Object;)Ljava/lang/Thread;
 */
JNIEXPORT jobject JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getThreadOwningObjectImpl(JNIEnv *, jobject,
                                                                                   jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getThreadOwningObjectImpl stub invocation");
    return NULL;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.isSuspendedImpl(Ljava/lang/Thread;)Z
 */
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_isSuspendedImpl(JNIEnv *, jobject,
                                                                         jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","ThreadMXBeanImpl_isSuspendedImpl stub invocation");
    return JNI_TRUE;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getThreadWaitedCountImpl(Ljava/lang/Thread;)J
 */
JNIEXPORT jlong JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getThreadWaitedCountImpl(JNIEnv *, jobject,
                                                                                  jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getThreadWaitedCountImpl stub invocation");
    return 2L;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getThreadWaitedTimeImpl(Ljava/lang/Thread;)J
 */
JNIEXPORT jlong JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getThreadWaitedTimeImpl(JNIEnv *, jobject,
                                                                                 jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getThreadWaitedCountImpl stub invocation");
    return 1L<<12;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getThreadBlockedTimeImpl(Ljava/lang/Thread;)J
 */
JNIEXPORT jlong JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getThreadBlockedTimeImpl(JNIEnv *, jobject,
                                                                                  jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getThreadBlockedTimeImpl stub invocation");
    return 1L<<11;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getThreadBlockedCountImpl(Ljava/lang/Thread;)J
 */
JNIEXPORT jlong JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getThreadBlockedCountImpl(JNIEnv *, jobject,
                                                                                   jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getThreadBlockedCountImpl stub invocation");
    return 5L;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.createThreadInfoImpl(JLjava/lang/String;Ljava/lang/Thread$State;ZZJJJJLjava/lang/String;JLjava/lang/String;[Ljava/lang/StackTraceElement;)Ljava/lang/management/ThreadInfo;
 */
JNIEXPORT jobject JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_createThreadInfoImpl(
    JNIEnv *jenv_ext,
    jobject ,
    jlong threadIdVal,
    jstring threadNameVal,
    jobject threadStateVal,
    jboolean suspendedVal,
    jboolean inNativeVal,
    jlong blockedCountVal,
    jlong blockedTimeVal,
    jlong waitedCountVal,
    jlong waitedTimeVal,
    jstring lockNameVal,
    jlong lockOwnerIdVal,
    jstring lockOwnerNameVal,
    jobjectArray stackTraceVal)
{
    // TODO implement this method stub correctly
    TRACE2("management","createThreadInfoImpl stub invocation");

    JNIEnv_Internal *jenv = (JNIEnv_Internal *)jenv_ext;

    jclass threadInfoClazz =jenv->FindClass("java/lang/management/ThreadInfo");
    if (jenv->ExceptionCheck()) return NULL;
    jmethodID threadInfoClazzConstructor = jenv->GetMethodID(threadInfoClazz, "<init>",
        "(JLjava/lang/String;Ljava/lang/Thread$State;ZZJJJJLjava/lang/String;"
        "JLjava/lang/String;[Ljava/lang/StackTraceElement;)V");
    if (jenv->ExceptionCheck()) return NULL;

    jobject threadInfo = jenv->NewObject(
        threadInfoClazz,
        threadInfoClazzConstructor,
        threadIdVal,
        threadNameVal,
        threadStateVal,
        suspendedVal,
        inNativeVal,
        blockedCountVal,
        blockedTimeVal,
        waitedCountVal,
        waitedTimeVal,
        lockNameVal,
        lockOwnerIdVal,
        lockOwnerNameVal,
        stackTraceVal);

    assert(!exn_raised());

    return threadInfo;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getThreadUserTimeImpl(J)J
 */
JNIEXPORT jlong JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getThreadUserTimeImpl(JNIEnv *, jobject,
                                                                               jlong)
{
    // TODO implement this method stub correctly
    TRACE2("management","getThreadUserTimeImpl stub invocation");
    return 1L<<11;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.getTotalStartedThreadCountImpl()J
 */
JNIEXPORT jlong JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_getTotalStartedThreadCountImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getTotalStartedThreadCountImpl stub invocation");
    return 5L;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.isCurrentThreadCpuTimeSupportedImpl()Z
 */
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_isCurrentThreadCpuTimeSupportedImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","isCurrentThreadCpuTimeSupportedImpl stub invocation");
    return JNI_TRUE;
};

jboolean thread_contention_monitoring = JNI_TRUE;

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.isThreadContentionMonitoringEnabledImpl()Z
 */
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_isThreadContentionMonitoringEnabledImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","isThreadContentionMonitoringEnabledImpl stub invocation");
    return thread_contention_monitoring;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.isThreadContentionMonitoringSupportedImpl()Z
 */
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_isThreadContentionMonitoringSupportedImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","isThreadContentionMonitoringSupportedImpl stub invocation");
    return JNI_TRUE;
};

jboolean thread_cpu_time_enabled = JNI_TRUE;

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.isThreadCpuTimeEnabledImpl()Z
 */
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_isThreadCpuTimeEnabledImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","isThreadCpuTimeEnabledImpl stub invocation");
    return thread_cpu_time_enabled;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.isThreadCpuTimeSupportedImpl()Z
 */
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_isThreadCpuTimeSupportedImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","isThreadCpuTimeSupportedImpl stub invocation");
    return JNI_TRUE;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.resetPeakThreadCountImpl()V
 */
JNIEXPORT void JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_resetPeakThreadCountImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","resetPeakThreadCountImpl stub invocation");
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.setThreadContentionMonitoringEnabledImpl(Z)V
 */
JNIEXPORT void JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_setThreadContentionMonitoringEnabledImpl(
    JNIEnv *,
    jobject,
    jboolean new_value)
{
    // TODO implement this method stub correctly
    TRACE2("management","setThreadContentionMonitoringEnabledImpl stub invocation");
    thread_contention_monitoring = new_value;
};

/*
 * Method: org.apache.harmony.lang.management.ThreadMXBeanImpl.setThreadCpuTimeEnabledImpl(Z)V
 */
JNIEXPORT void JNICALL
Java_org_apache_harmony_lang_management_ThreadMXBeanImpl_setThreadCpuTimeEnabledImpl(JNIEnv *, jobject,
                                                                                     jboolean new_value)
{
    // TODO implement this method stub correctly
    TRACE2("management","setThreadCpuTimeEnabledImpl stub invocation");
    thread_cpu_time_enabled = new_value;
};



