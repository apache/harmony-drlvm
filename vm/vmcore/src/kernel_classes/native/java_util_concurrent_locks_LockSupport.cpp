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
 * @author Artem Aliev
 * @version $Revision: 1.1.2.1.4.4 $
 */  
#include "vm_threads.h"
#include "thread_generic.h"
#include "java_util_concurrent_locks_LockSupport.h"
#include "jni.h"
#include "open/jthread.h"

/* Inaccessible static: parked */
/*
 * Method: java.util.concurrent.locks.LockSupport.unpark(Ljava/lang/Thread;)V
 */
JNIEXPORT void JNICALL Java_java_util_concurrent_locks_LockSupport_unpark (JNIEnv *jenv, jclass, jobject thread) {
    jthread_unpark(thread);
}

/*
 * Method: java.util.concurrent.locks.LockSupport.park()V
 */
JNIEXPORT void JNICALL Java_java_util_concurrent_locks_LockSupport_park (JNIEnv * UNREF jenv, jclass) {
    jthread_park();
}

/*
 * Method: java.util.concurrent.locks.LockSupport.parkNanos(J)V
 */

#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:1682)
#endif

JNIEXPORT void JNICALL Java_java_util_concurrent_locks_LockSupport_parkNanos(JNIEnv * UNREF jenv, jclass, jlong nanos) {
    jthread_timed_park(0,(jint)nanos);
}

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif

/*
 * Method: java.util.concurrent.locks.LockSupport.parkUntil(J)V
 */
JNIEXPORT void JNICALL Java_java_util_concurrent_locks_LockSupport_parkUntil(JNIEnv * UNREF jenv, jclass UNREF thread, jlong milis) {
    //FIXME integration should be parkUntil
    jthread_timed_park((jlong)milis, 0);
}

