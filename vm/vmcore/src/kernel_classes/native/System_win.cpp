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

#include <time.h>
#include <windows.h>
#include <winbase.h>
#include "java_lang_System.h"

static int supported = 0;
static double frequency;

JNIEXPORT jlong JNICALL Java_java_lang_System_currentTimeMillis
  (JNIEnv *env, jclass clazz){
      
      static jlong start_time_millis = 0; 
      
      if (start_time_millis == 0){
          jlong tick_count = GetTickCount();
          jlong millis = tick_count % 1000;
          jlong start_time = (jlong)time(0) * 1000;
          start_time_millis = start_time - tick_count + millis;
          return  start_time + millis;
      }

      return start_time_millis + GetTickCount();
}

JNIEXPORT jlong JNICALL Java_java_lang_System_nanoTime
  (JNIEnv *env, jclass cls){
    if(supported){
        LARGE_INTEGER count;
        QueryPerformanceCounter(&count);
        return (jlong)((double)count.QuadPart / frequency * 1E9);
    }
        return (jlong)(GetTickCount() * 1E6);
}


JNIEXPORT void JNICALL Java_java_lang_System_initNanoTime
  (JNIEnv *env, jclass cls){
    LARGE_INTEGER freq;
    supported = QueryPerformanceFrequency(&freq);
    if (supported){
        frequency = (double)freq.QuadPart;
    }
}

