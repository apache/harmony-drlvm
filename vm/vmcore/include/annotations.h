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
 * @version $Revision$
 */  

#ifndef _ANNOTATIONS_H_
#define _ANNOTATIONS_H_

#include "jni_types.h"
#include "vm_core_types.h"

struct AnnotationTable;
struct Annotation;
struct AnnotationValue;

// Returns array of declared annotations.
// Returns zero-sized array if there are no annotations.
// May raise an exception, in this case returns null.
jobjectArray get_annotations(JNIEnv* jenv, AnnotationTable* table, Class* clss);

// Returns resolved annotation or null if resolution failed.
// If the cause parameter is not null, resolution error is assigned to it for upstream processing;
// otherwise the error is raised.
// In case of errors other than resolving failure, the error is raised, 
// null is returned and cause is unchanged
jobject resolve_annotation(JNIEnv* jenv, Annotation* antn, Class* clss, jthrowable* cause = NULL);

// Returns resolved annotation value or null if resolution failed.
// In case of a resolution failure, the error is assigned to the "cause" parameter 
// for upstream processing.
// In case of errors other than resolving failure, the error is raised, 
// null is returned and cause is unchanged
jobject resolve_annotation_value(JNIEnv* jenv, AnnotationValue& antn, Class* antn_clss, 
                                 String* name, jthrowable* cause);

#endif // !_ANNOTATIONS_H_
