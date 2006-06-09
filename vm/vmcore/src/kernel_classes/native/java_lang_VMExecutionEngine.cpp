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
 * @version $Revision: 1.1.2.2.4.4 $
 */  

/**
 * @file jjava_lang_VMExecutionEngine.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of 
 * java.lang.VMExecutionEngine class.
 */

#include <apr_file_io.h>
#include "environment.h"
#include "port_filepath.h"
#include "port_sysinfo.h"
#include "jni_utils.h"
#include "properties.h"
#include "java_lang_VMExecutionEngine.h"

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    exit
 * Signature: (IZ)V
 */
// FixME: should be removed
//JNIEXPORT void JNICALL Java_java_lang_VMExecutionEngine_exit
JNIEXPORT void JNICALL Java_java_lang_VMExecutionEngine_exit__IZ
  (JNIEnv *, jclass, jint status, jboolean)
{
    // ToDo: process jboolean
    vm_exit(status);
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    exit
 * Signature: (IZ[Ljava/lang/Runnable;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMExecutionEngine_exit__IZ_3Ljava_lang_Runnable_2
  (JNIEnv *, jclass, jint status, jboolean, jobjectArray)
{
    // ToDo: process jboolean
    // ToDo: process jobjectArray
    vm_exit(status);
}



/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    getAssertionStatus
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_java_lang_VMExecutionEngine_getAssertionStatus
  (JNIEnv *, jclass, jstring)
{
    return 0;
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    getAvailableProcessors
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_java_lang_VMExecutionEngine_getAvailableProcessors
  (JNIEnv *, jclass)
{
    return port_CPUs_number();
}

/**
 * Adds property specified by key and val parameters to given Properties object.
 */
static void PropPut(JNIEnv* jenv, jobject properties, const char* key, const char* val)
{
    jobject key_string = NewStringUTF(jenv, key);
    jobject val_string = val ? NewStringUTF(jenv, val) : NULL;

    static jmethodID put_method = NULL;
    if (!put_method) {
        Class* clss = VM_Global_State::loader_env->java_util_Properties_Class;
        String* name = VM_Global_State::loader_env->string_pool.lookup("put");
        String* sig = VM_Global_State::loader_env->string_pool.lookup(
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        put_method = (jmethodID) class_lookup_method_recursive(clss, name, sig);
    }

    CallObjectMethod(jenv, properties, put_method, key_string, val_string);
}

static void insertSystemProperties(JNIEnv *jenv, jobject pProperties)
{
    static apr_pool_t *pp;
    if (!pp) {
        apr_pool_create(&pp, 0);
    }

    // Now, insert all the default properties.
    PropPut(jenv, pProperties, "user.language", "en");
    PropPut(jenv, pProperties, "user.region", "US");
    PropPut(jenv, pProperties, "file.encoding", "8859_1");
    //PropPut(jenv, pProperties, "file.encoding", apr_os_default_encoding(p));

    PropPut(jenv, pProperties, "file.separator", PORT_FILE_SEPARATOR_STR);
    PropPut(jenv, pProperties, "path.separator", PORT_PATH_SEPARATOR_STR);
    PropPut(jenv, pProperties, "line.separator", APR_EOL_STR);
    PropPut(jenv, pProperties, "os.arch", port_CPU_architecture());

    char *os_name, *os_version;
    port_OS_name_version(&os_name, &os_version, pp);
    PropPut(jenv, pProperties, "os.name", os_name);
    PropPut(jenv, pProperties, "os.version", os_version);
    PropPut(jenv, pProperties, "java.vendor.url", "http://www.intel.com/");

    char *path;
    apr_filepath_get(&path, APR_FILEPATH_NATIVE, pp);
    PropPut(jenv, pProperties, "user.dir", path);
    const char *tmp;
    if (APR_SUCCESS != apr_temp_dir_get(&tmp, pp)) {
        tmp = ".";
    }
    PropPut(jenv, pProperties, "java.tmpdir", tmp);
    PropPut(jenv, pProperties, "java.class.path",
        properties_get_string_property( 
            reinterpret_cast<PropertiesHandle>(&((JNIEnv_Internal*)jenv)->vm->vm_env->properties),
            "java.class.path") );
    PropPut(jenv, pProperties, "java.class.version", "45.3");

    //VM specified/APP specified properties are supported here.
    Properties::Iterator *iterator = VM_Global_State::loader_env->properties.getIterator();
    const Prop_entry *next = NULL;
    while((next = iterator->next())){
        PropPut(jenv, pProperties, next->key, ((Prop_String*)next->value)->value);
    }
    
    apr_pool_clear(pp);
} //insertSystemProperties

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    getProperties
 * Signature: ()Ljava/util/Properties;
 */
JNIEXPORT jobject JNICALL Java_java_lang_VMExecutionEngine_getProperties
  (JNIEnv *jenv, jclass)
{
    jobject jprops = create_default_instance(
        VM_Global_State::loader_env->java_util_Properties_Class);

    // set default VM properties
    insertSystemProperties(jenv, jprops);

    return jprops;
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    traceInstructions
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMExecutionEngine_traceInstructions
  (JNIEnv *, jclass, jboolean)
{
    return;
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    traceMethodCalls
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMExecutionEngine_traceMethodCalls
  (JNIEnv *, jclass, jboolean)
{
    return;
}
