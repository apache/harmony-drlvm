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
 * @author Alexey V. Varlamov
 * @version $Revision: 1.1.2.1.4.5 $
 */  

// This interface provides a set of common reflection mechanisms for Java class libraries.

#ifndef _REFLECTION_H_
#define _REFLECTION_H_

#include "jni_types.h"
#include "vm_core_types.h"

jobjectArray reflection_get_class_interfaces(JNIEnv*, jclass);
jobjectArray reflection_get_class_fields(JNIEnv*, jclass);
jobjectArray reflection_get_class_constructors(JNIEnv*, jclass);
jobjectArray reflection_get_class_methods(JNIEnv* jenv, jclass clazz);
jobjectArray reflection_get_parameter_types(JNIEnv *jenv, Class* type, jobject jmethod, Method* method = 0);

enum reflection_fields{
    PARAMETERS,
    EXCEPTIONS,
    DECLARING_CLASS,
    NAME, 
    TYPE,
    VM_MEMBER,
    FIELDS_NUMBER
}; 

Field* get_reflection_field(Class* clss, reflection_fields descriptor);

Class_Member* reflection_jobject_to_Class_Member(jobject jmember, Class* type);

bool jobjectarray_to_jvaluearray(JNIEnv* jenv, jvalue** output, Method* method, jobjectArray input);

#endif // !_REFLECTION_H_
