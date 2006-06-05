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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.6.4 $
 */  

#include "jni.h"
#include "atomics.h"
#include "org_apache_harmony_util_concurrent_Atomics.h"


JNIEXPORT jboolean JNICALL 
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetObject__Ljava_lang_Object_2Ljava_lang_reflect_Field_2Ljava_lang_Object_2Ljava_lang_Object_2
(JNIEnv * env, jobject self, jobject obj, jobject fieldID, jobject expected, jobject value)
{     
    return compareAndSetObjectField(env, self, obj, fieldID, expected, value);
}


JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetBoolean__Ljava_lang_Object_2Ljava_lang_reflect_Field_2ZZ
(JNIEnv * env, jobject self, jobject obj, jobject fieldID, jboolean expected, jboolean value)
{
    return compareAndSetBooleanField(env, self, obj, fieldID, expected, value);
}

                  
JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetInt__Ljava_lang_Object_2Ljava_lang_reflect_Field_2II
(JNIEnv * env, jobject self, jobject obj, jobject fieldID, jint expected, jint value)
{     
    return compareAndSetIntField(env, self, obj, fieldID, expected, value);
}


JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetLong__Ljava_lang_Object_2Ljava_lang_reflect_Field_2JJ
(JNIEnv * env, jobject self, jobject obj, jobject fieldID, jlong expected, jlong value)
{     
    return compareAndSetLongField(env, self, obj, fieldID, expected, value);
}
 

JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetInt___3IIII
(JNIEnv * env, jobject self, jintArray array, jint index, jint expected, jint value)
{
    return compareAndSetIntArray(env, self, array, index, expected, value);
}


JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetBoolean___3ZIZZ
(JNIEnv * env, jobject self, jbooleanArray array, jint index, jboolean expected, jboolean value)
{
    return compareAndSetBooleanArray(env, self, array, index, expected, value);
}


JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetLong___3JIJJ
(JNIEnv * env, jobject self, jlongArray array, jint index, jlong expected, jlong value)
{
    return compareAndSetLongArray(env, self, array, index, expected, value);
}


JNIEXPORT jboolean JNICALL
Java_org_apache_harmony_util_concurrent_Atomics_compareAndSetObject___3Ljava_lang_Object_2ILjava_lang_Object_2Ljava_lang_Object_2
(JNIEnv * env, jobject self, jobjectArray array, jint index, jobject expected, jobject value)
{
    return compareAndSetObjectArray(env, self, array, index, expected, value);
}
