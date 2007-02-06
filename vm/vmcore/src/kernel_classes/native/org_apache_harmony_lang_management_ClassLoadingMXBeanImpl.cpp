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
 * @file org_apache_harmony_lang_management_ClassLoadingMXBeanImpl.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of
 * org.apache.harmony.lang.management.ClassLoadingMXBeanImpl class.
 */

#include <cxxlog.h>
#include "org_apache_harmony_lang_management_ClassLoadingMXBeanImpl.h"
/*
 * Class:     org_apache_harmony_lang_management_ClassLoadingMXBeanImpl
 * Method:    getLoadedClassCountImpl
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_apache_harmony_lang_management_ClassLoadingMXBeanImpl_getLoadedClassCountImpl
(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getLoadedClassCountImpl stub invocation");
    return 1<<3;
};

/*
 * Class:     org_apache_harmony_lang_management_ClassLoadingMXBeanImpl
 * Method:    getTotalLoadedClassCountImpl
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_org_apache_harmony_lang_management_ClassLoadingMXBeanImpl_getTotalLoadedClassCountImpl
(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getTotalLoadedClassCountImpl stub invocation");
    return 1<<4;
};

/*
 * Class:     org_apache_harmony_lang_management_ClassLoadingMXBeanImpl
 * Method:    getUnloadedClassCountImpl
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_org_apache_harmony_lang_management_ClassLoadingMXBeanImpl_getUnloadedClassCountImpl
(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","getUnloadedClassCountImpl stub invocation");
    return 1<<2;
};

jboolean ClassLoadingVerbose = JNI_FALSE;


/*
 * Class:     org_apache_harmony_lang_management_ClassLoadingMXBeanImpl
 * Method:    isVerboseImpl
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_harmony_lang_management_ClassLoadingMXBeanImpl_isVerboseImpl
(JNIEnv *, jobject)
{
    // TODO implement this method stub correctly
    TRACE2("management","ClassLoadingMXBeanImpl_isVerboseImpl stub invocation");
    return ClassLoadingVerbose;
};

/*
 * Class:     org_apache_harmony_lang_management_ClassLoadingMXBeanImpl
 * Method:    setVerboseImpl
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_org_apache_harmony_lang_management_ClassLoadingMXBeanImpl_setVerboseImpl
(JNIEnv *, jobject, jboolean new_value)
{
    // TODO implement this method stub correctly
    TRACE2("management","ClassLoadingMXBeanImpl_setVerboseImpl stub invocation");
    ClassLoadingVerbose = new_value;
};

