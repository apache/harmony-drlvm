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
 * @version $Revision: 1.1.2.1.4.5 $
 */  

/**
 * @file java_lang_String.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of java.lang.String kernel 
 * class.
 */

#include "java_lang_String.h"

#include "vm_strings.h"

/**
 * Implements java.lang.String.intern(..) method.
 * For details see kernel classes component documentation.
 */
JNIEXPORT jstring JNICALL
Java_java_lang_String_intern(JNIEnv *jenv, jstring str)
{
    // just call corresponding VM internal function
    return string_intern(jenv, str);
}
