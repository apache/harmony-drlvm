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
 * @file org_apache_harmony_lang_management_RuntimeMXBeanImpl.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of
 * org.apache.harmony.lang.management.RuntimeMXBeanImpl class.
 */

#include <apr_time.h>
#include <cxxlog.h>
#include "java_lang_System.h"
#include "org_apache_harmony_lang_management_RuntimeMXBeanImpl.h"
/*
 * Method: org.apache.harmony.lang.management.RuntimeMXBeanImpl.getNameImpl()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_apache_harmony_lang_management_RuntimeMXBeanImpl_getNameImpl(JNIEnv *env, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","RuntimeMXBeanImpl_getNameImpl stub invocation");
    char *buf = "DRLVM";
    return env->NewStringUTF(buf);
};

/*
 * Method: org.apache.harmony.lang.management.RuntimeMXBeanImpl.getStartTimeImpl()J
 */
JNIEXPORT jlong JNICALL
Java_org_apache_harmony_lang_management_RuntimeMXBeanImpl_getStartTimeImpl(JNIEnv *env, jobject )
{
    // TODO implement this method stub correctly
    TRACE2("management","RuntimeMXBeanImpl_getStartTimeImpl stub invocation");
    return apr_time_now()/1000;
};

/*
 * Method: org.apache.harmony.lang.management.RuntimeMXBeanImpl.getUptimeImpl()J
 */
JNIEXPORT jlong JNICALL
Java_org_apache_harmony_lang_management_RuntimeMXBeanImpl_getUptimeImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","RuntimeMXBeanImpl_getUptimeImpl stub invocation");
    return 1L<<10;
};

/*
 * Method: org.apache.harmony.lang.management.RuntimeMXBeanImpl.isBootClassPathSupportedImpl()Z
 */
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_lang_management_RuntimeMXBeanImpl_isBootClassPathSupportedImpl(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","isBootClassPathSupportedImpl stub invocation");
    return JNI_TRUE;
};


