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
 * @author Intel, Euguene Ostrovsky
 * @version $Revision: 1.1.2.2.4.4 $
 */  

/**
 * @file java_lang_Class.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of 
 * java.lang.Class kernel class.
 */

#include "org_apache_harmony_vm_VMStack.h"

#include "java_lang_Class.h"

JNIEXPORT jobjectArray JNICALL Java_java_lang_Class_getStackClasses
  (JNIEnv *jenv_ext, jclass, jint maxSize, jboolean considerPrivileged)
{
    // reuse similar method of org.apache.harmony.drl.vm.VMStack class
    return Java_org_apache_harmony_vm_VMStack_getClasses(jenv_ext, NULL, maxSize, considerPrivileged);
}
