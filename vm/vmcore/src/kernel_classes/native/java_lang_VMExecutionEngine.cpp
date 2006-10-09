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

#define LOG_DOMAIN "vm.kernel"
#include "cxxlog.h"

#include <apr_env.h>
#include <apr_file_io.h>
#include <apr_time.h>

#include "port_dso.h"
#include "port_env.h"
#include "port_filepath.h"
#include "port_sysinfo.h"
#include "port_timer.h"
#include "environment.h"
#include "jni_utils.h"
#include "properties.h"
#include "exceptions.h"
#include "java_lang_VMExecutionEngine.h"
#include "assertion_registry.h"
#include "init.h"

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
 * Signature: (Ljava/lang/Class;ZI)I
 */
JNIEXPORT jint JNICALL Java_java_lang_VMExecutionEngine_getAssertionStatus
  (JNIEnv * jenv, jclass, jclass jclss, jboolean recursive, jint defaultStatus)
{
    Assertion_Status status = ASRT_UNSPECIFIED;
    Global_Env* genv = jni_get_vm_env(jenv);
    Assertion_Registry* reg = genv->assert_reg;
    if (!reg) {
        return status;
    }

    if(jclss) {
        Class* clss = jclass_to_struct_Class(jclss);
        while (clss->declaringclass_index) {
            clss = class_get_declaring_class((Class_Handle)clss);
        }
        const char* name = class_get_java_name(clss, genv)->bytes;
        bool system = (((void*)clss->class_loader) == ((void*)genv->bootstrap_class_loader));
        TRACE("check assert status for " << name << " system=" << system);
        if (system || !recursive) {
            status = reg->get_class_status(name);
        } 
        TRACE("name checked: " << status);
        if (recursive || system) {
            if (status == ASRT_UNSPECIFIED) {
                status = reg->get_package_status(name);
            }
            TRACE("pkg checked: " << status);
            if (status == ASRT_UNSPECIFIED) {
                if (defaultStatus != ASRT_UNSPECIFIED) {
                    status = (Assertion_Status)defaultStatus;
                } else {
                    status = reg->is_enabled(system);
                }
            }
            TRACE("default checked: " << status);
        }
    } else {
        if (reg->classes || reg->packages || reg->enable_system) {
            status = ASRT_ENABLED;
        } else {
            status = reg->enable_all;
        }
    }
    TRACE("Resulting assertion status: " << status);
    return status;
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
static bool PropPut(JNIEnv* jenv, jobject properties, const char* key, const char* val)
{
    jobject key_string = NewStringUTF(jenv, key);
    if (!key_string) return false;
    jobject val_string = val ? NewStringUTF(jenv, val) : NULL;
    if (val && !val_string) return false;

    static jmethodID put_method = NULL;
    if (!put_method) {
        Class* clss = VM_Global_State::loader_env->java_util_Properties_Class;
        String* name = VM_Global_State::loader_env->string_pool.lookup("put");
        String* sig = VM_Global_State::loader_env->string_pool.lookup(
            "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        put_method = (jmethodID) class_lookup_method_recursive(clss, name, sig);
    }

    CallObjectMethod(jenv, properties, put_method, key_string, val_string);
    return !exn_raised();
}

static void insertSystemProperties(JNIEnv *jenv, jobject pProperties, apr_pool_t *pp)
{
    char *os_name, *os_version, *path;
    port_OS_name_version(&os_name, &os_version, pp);
    apr_filepath_get(&path, APR_FILEPATH_NATIVE, pp);
    const char *tmp;
    if (APR_SUCCESS != apr_temp_dir_get(&tmp, pp)) {
        tmp = ".";
    }
    PropPut(jenv, pProperties, "java.tmpdir", tmp);
    PropPut(jenv, pProperties, "java.class.path",
        properties_get_string_property( 
            reinterpret_cast<PropertiesHandle>(jni_get_vm_env(jenv)->properties),
            "java.class.path") );

    // TODO : fix this - it should come from Class.h
    PropPut(jenv, pProperties, "java.class.version", "49.0");

    // First, insert all the default properties.
    if (!PropPut(jenv, pProperties, "user.language", "en")
        || !PropPut(jenv, pProperties, "user.region", "US")
        || !PropPut(jenv, pProperties, "file.encoding", "8859_1")
        //PropPut(jenv, pProperties, "file.encoding", apr_os_default_encoding(p));
        || !PropPut(jenv, pProperties, "file.separator", PORT_FILE_SEPARATOR_STR)
        || !PropPut(jenv, pProperties, "path.separator", PORT_PATH_SEPARATOR_STR)
        || !PropPut(jenv, pProperties, "line.separator", APR_EOL_STR)
        || !PropPut(jenv, pProperties, "os.arch", port_CPU_architecture())
        || !PropPut(jenv, pProperties, "os.name", os_name)
        || !PropPut(jenv, pProperties, "os.version", os_version)
        || !PropPut(jenv, pProperties, "java.vendor.url", "http://www.intel.com/")
        || !PropPut(jenv, pProperties, "user.dir", path)
        || !PropPut(jenv, pProperties, "java.tmpdir", tmp)
        || !PropPut(jenv, pProperties, "java.class.version", "49.0") )
    {
        return;
    }

    // Next, add runtime specified properties.
    Properties::Iterator *iterator = VM_Global_State::loader_env->properties->getIterator();
    const Prop_entry *next = NULL;
    while((next = iterator->next())){
        if (!PropPut(jenv, pProperties, next->key, ((Prop_String*)next->value)->value)){
            break;
        }
    }
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

    if (jprops) {
        apr_pool_t *pp;
        if (APR_SUCCESS == apr_pool_create(&pp, 0)) {
            insertSystemProperties(jenv, jprops, pp);
            apr_pool_destroy(pp);
        }
    }

    return jprops;
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    traceInstructions
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMExecutionEngine_traceInstructions
  (JNIEnv *jenv, jclass, jboolean)
{
    //ThrowNew_Quick(jenv, "java/lang/UnsupportedOperationException", NULL);
    return;
}

/*
 * Class:     java_lang_VMExecutionEngine
 * Method:    traceMethodCalls
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMExecutionEngine_traceMethodCalls
  (JNIEnv *jenv, jclass, jboolean)
{
    //ThrowNew_Quick(jenv, "java/lang/UnsupportedOperationException", NULL);
    return;
}

/*
* Class:     java_lang_VMExecutionEngine
* Method:    currentTimeMillis
* Signature: ()J
*/
JNIEXPORT jlong JNICALL Java_java_lang_VMExecutionEngine_currentTimeMillis
(JNIEnv *, jclass) {
    return apr_time_now()/1000;
}

/*
* Class:     java_lang_VMExecutionEngine
* Method:    nanoTime
* Signature: ()J
*/
JNIEXPORT jlong JNICALL Java_java_lang_VMExecutionEngine_nanoTime
(JNIEnv *, jclass) {
    return port_nanotimer();
}

/*
* Class:     java_lang_VMExecutionEngine
* Method:    getenv
* Signature: (Ljava/lang/String;)Ljava/lang/String;
*/
JNIEXPORT jstring JNICALL Java_java_lang_VMExecutionEngine_getenv__Ljava_lang_String_2
(JNIEnv *jenv, jclass, jstring jname) {
    jstring res = NULL;
    if(jname) {
        const char* key = GetStringUTFChars(jenv, jname, NULL);
        apr_pool_t *pp;
        char* value;
        if (APR_SUCCESS == apr_pool_create(&pp, 0) 
            && APR_SUCCESS == apr_env_get(&value, key, pp)) {
            res = NewStringUTF(jenv, value);
            apr_pool_destroy(pp);
        }
        ReleaseStringUTFChars(jenv, jname, key);
    }
    return res;
}

/*
* Class:     java_lang_VMExecutionEngine
* Method:    getenv
* Signature: ()Ljava/util/Map;
*/
JNIEXPORT jobject JNICALL Java_java_lang_VMExecutionEngine_getenv__
(JNIEnv *jenv, jclass) {
    Global_Env * genv = VM_Global_State::loader_env;
    Class* mapClass = genv->LoadCoreClass("java/util/HashMap");
    jmethodID put = (jmethodID)class_lookup_method_recursive(mapClass, "put", 
        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    jobject jmap = create_default_instance(mapClass);
    if (jmap) {
        apr_pool_t *pp;
        if (APR_SUCCESS == apr_pool_create(&pp, 0)) {
            for (char **e = port_env_all(pp) ; *e; ++e){
                size_t idx = strcspn(*e, "=");
                char* key = apr_pstrndup(pp, *e, idx);
                jobject jkey = NewStringUTF(jenv, key);
                if (!jkey) break;
                jobject jval = NewStringUTF(jenv, *e+idx+1);
                if (!jval) break;
                CallObjectMethod(jenv, jmap, put, jkey, jval); 
                assert(!exn_raised());
            }
            apr_pool_destroy(pp);
        }
    }

    return jmap;
}

/*
* Class:     java_lang_VMExecutionEngine
* Method:    mapLibraryName
* Signature: (Ljava/lang/String;)Ljava/lang/String;
*/
JNIEXPORT jstring JNICALL Java_java_lang_VMExecutionEngine_mapLibraryName
(JNIEnv *jenv, jclass, jstring jlibname) {
    jstring res = NULL;
    if(jlibname) {
        const char* libname = GetStringUTFChars(jenv, jlibname, NULL);
        apr_pool_t *pp;
        if (APR_SUCCESS == apr_pool_create(&pp, 0)) {
            res = NewStringUTF(jenv, port_dso_name_decorate(libname, pp));
            apr_pool_destroy(pp);
        }
        ReleaseStringUTFChars(jenv, jlibname, libname);
    }
    return res;
}
