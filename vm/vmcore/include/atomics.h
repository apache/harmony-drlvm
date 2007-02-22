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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.2.1.4.4 $
 */  

#ifndef _ATOMICS_H_
#define _ATOMICS_H_

#include "jni.h"
#include "Class.h"

#if defined(_EM64T_) && defined(_WIN64)
#include <intrin.h>
#pragma intrinsic (_ReadWriteBarrier)
#pragma intrinsic (_WriteBarrier)
#endif

JNIEXPORT jlong getFieldOffset
  (JNIEnv * env, jobject field);

JNIEXPORT jboolean compareAndSetObjectField
  (JNIEnv * env, jobject self, jobject obj, jlong offset, jobject expected, jobject value);
      
JNIEXPORT jboolean compareAndSetBooleanField
  (JNIEnv * env, jobject self, jobject obj, jlong offset, jboolean expected, jboolean value);
                  
JNIEXPORT jboolean compareAndSetIntField
  (JNIEnv * env, jobject self, jobject obj, jlong offset, jint expected, jint value);
                  
JNIEXPORT jboolean compareAndSetLongField
  (JNIEnv * env, jobject self, jobject obj, jlong offset, jlong expected, jlong value);
               
JNIEXPORT jboolean compareAndSetObjectArray
(JNIEnv * env, jobject self, jobjectArray array, jint index, jobject expected, jobject value);

JNIEXPORT jboolean compareAndSetBooleanArray
(JNIEnv * env, jobject self, jbooleanArray array, jint index, jboolean expected, jboolean value);

JNIEXPORT jboolean compareAndSetIntArray
(JNIEnv * env, jobject self, jintArray array, jint index, jint expected, jint value);

JNIEXPORT jboolean compareAndSetLongArray
(JNIEnv * env, jobject self, jlongArray array, jint index, jlong expected, jlong value);

//void MemoryReadWriteBarrier();

#if defined (PLATFORM_POSIX) 
    #if defined (_IPF_)
        inline void MemoryReadWriteBarrier() {
            asm volatile ("mf" ::: "memory");
        }
        inline void MemoryWriteBarrier() {
            asm volatile ("mf" ::: "memory");
        }
    #else 
        inline void MemoryReadWriteBarrier() {
            __asm__("mfence");
        } 
        inline void MemoryWriteBarrier() {
            __asm__("sfence");
        } 
    #endif
#else
    #if defined (_IPF_)
        inline void MemoryReadWriteBarrier() {
            __asm mf;
        }
        inline void MemoryWriteBarrier() {
            __asm mf;
        }
    #else
        inline void MemoryReadWriteBarrier() {
        #if defined(_EM64T_) && defined(_WIN64)
            _ReadWriteBarrier();
        #else
            __asm mfence;
        #endif
        }
        inline void MemoryWriteBarrier() {
        #if defined(_EM64T_) && defined(_WIN64)
            _WriteBarrier();
        #else
            __asm sfence;
        #endif
        }
    #endif
#endif

#endif // _ATOMICS_H_

