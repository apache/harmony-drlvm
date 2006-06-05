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

#include <stdlib.h>
#include <string.h>
#include "java_lang_System.h"

JNIEXPORT void JNICALL Java_java_lang_System_setErrUnsecure
(JNIEnv *env, jclass clazz, jobject err){
    jfieldID field_id = env->GetStaticFieldID(clazz, "err", "Ljava/io/PrintStream;");
    env->SetStaticObjectField(clazz, field_id, err);
}

JNIEXPORT void JNICALL Java_java_lang_System_setInUnsecure
(JNIEnv *env, jclass clazz, jobject in){
    jfieldID field_id = env->GetStaticFieldID(clazz, "in", "Ljava/io/InputStream;");
    env->SetStaticObjectField(clazz, field_id, in);
}

JNIEXPORT void JNICALL Java_java_lang_System_setOutUnsecure
(JNIEnv *env, jclass clazz, jobject out){
    jfieldID field_id = env->GetStaticFieldID(clazz, "out", "Ljava/io/PrintStream;");
    env->SetStaticObjectField(clazz, field_id, out);
}

extern char **environ;

JNIEXPORT jobject JNICALL Java_java_lang_System_getenvUnsecure__
(JNIEnv *env, jclass clazz){
    jclass cls = env->FindClass("java/util/Hashtable");
    jmethodID ctr = env->GetMethodID(cls, "<init>", "()V");
    jmethodID mtd = env->GetMethodID(cls, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject ret = env->NewObject(cls, ctr);
    for (char **e = environ ; *e; ++e){
        int idx = strcspn(*e, "=");
        char* key = new char[idx+1];
        strncpy(key, *e, idx);
        key[idx]='\0';
        env->CallObjectMethod(ret, mtd, env->NewStringUTF(key), env->NewStringUTF(*e+idx+1)); 
        delete [] key;
    }
    return ret;
}

JNIEXPORT jstring JNICALL Java_java_lang_System_getenvUnsecure__Ljava_lang_String_2
(JNIEnv *env, jclass clazz, jstring name){
    const char *str = env->GetStringUTFChars(name, 0);
    char *buf = getenv(str);
    env->ReleaseStringUTFChars(name, str);
    if (buf == NULL){
        return 0;
    }
    return env->NewStringUTF(buf);
}

JNIEXPORT void JNICALL Java_java_lang_System_rethrow
  (JNIEnv *env, jclass clazz, jthrowable thr){
    env->Throw(thr);
}
