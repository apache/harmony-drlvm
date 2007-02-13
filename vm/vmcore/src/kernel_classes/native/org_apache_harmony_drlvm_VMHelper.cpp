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


#include "org_apache_harmony_drlvm_VMHelper.h"

#include "open/vm.h"
#include "open/vm_util.h"
#include "environment.h"
#include <assert.h>

JNIEXPORT jint JNICALL Java_org_apache_harmony_drlvm_VMHelper_getPointerTypeSize (JNIEnv *, jclass) {
    return (jint)sizeof(void*);
}


JNIEXPORT jboolean JNICALL Java_org_apache_harmony_drlvm_VMHelper_isCompressedRefsMode(JNIEnv *, jclass) {
    return (jboolean)VM_Global_State::loader_env->compress_references;
}

JNIEXPORT jboolean JNICALL Java_org_apache_harmony_drlvm_VMHelper_isCompressedVTableMode(JNIEnv *, jclass) {
#ifdef USE_COMPRESSED_VTABLE_POINTERS
    return true;
#else
    return false;
#endif
}


JNIEXPORT jlong JNICALL Java_org_apache_harmony_drlvm_VMHelper_getCompressedModeVTableBaseOffset(JNIEnv *, jclass) {
    bool cm = (jboolean)VM_Global_State::loader_env->compress_references;
    if (cm) {
        return (jlong)vm_get_vtable_base();
    }
    return -1;
}


JNIEXPORT jlong JNICALL Java_org_apache_harmony_drlvm_VMHelper_getCompressedModeObjectBaseOffset(JNIEnv *, jclass) {
    bool cm = (jboolean)VM_Global_State::loader_env->compress_references;
    if (cm) {
        return (jlong)VM_Global_State::loader_env->heap_base;;
    }
    return -1;
}
