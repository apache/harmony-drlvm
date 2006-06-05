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
 * @author Roman S. Bushmanov
 * @version $Revision: 1.1.2.1.4.3 $
 */

#include <sys/time.h>
#include "java_lang_System.h"

static jlong init_sec = 0;

JNIEXPORT jlong JNICALL Java_java_lang_System_currentTimeMillis
  (JNIEnv *env, jclass clazz){
    struct timeval tv;
    struct timezone tz;

    gettimeofday(&tv, &tz);

    jlong retval = tv.tv_usec / 1000;
    retval += (jlong)tv.tv_sec * 1000;

    return retval;
}

JNIEXPORT jlong JNICALL Java_java_lang_System_nanoTime(JNIEnv *env, jclass cls){
        struct timeval tv;
        struct timezone tz;

        gettimeofday(&tv, &tz);

        return (jlong)(((jlong)tv.tv_sec - init_sec) * 1E9 + (jlong)tv.tv_usec * 1E3);
}

JNIEXPORT void JNICALL Java_java_lang_System_initNanoTime(JNIEnv *env, jclass cls){
        struct timeval tv;
        struct timezone tz;

        gettimeofday(&tv, &tz);
        
        init_sec = tv.tv_sec;
}
