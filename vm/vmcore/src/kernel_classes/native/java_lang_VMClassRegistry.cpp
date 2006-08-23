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
 * @version $Revision: 1.1.2.1.4.4 $
 */

/**
 * @file java_lang_VMClassRegistry.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of 
 * java.lang.VMClassRegistry class.
 */
#define LOG_DOMAIN "vm.core.classes"
#include "cxxlog.h"

#include "Package.h"
#include "classloader.h"
#include "jni_utils.h"
#include "reflection.h"
#include "stack_trace.h"

#include "vm_strings.h"

#include "java_lang_VMClassRegistry.h"

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    defineClass
 * Signature: (Ljava/lang/String;[BII)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_java_lang_VMClassRegistry_defineClass
  (JNIEnv *jenv, jclass, jstring name, jobject cl, jbyteArray data, jint offset, jint len)
{
    const char* clssname = NULL;

    // obtain char * for the name if provided
    if(name)
        clssname = GetStringUTFChars(jenv, name, NULL);

    if (name && NULL != strchr(clssname, '/'))
    {
        std::stringstream ss;
        ss << "The name is expected in binary (canonical) form,"
            " therefore '/' symbols are not allowed: " << clssname;

        exn_raise_only(
            exn_create("java/lang/NoClassDefFoundError", ss.str().c_str()));

        return NULL;
    }

    // obtain raw classfile data data pointer
    jboolean is_copy;
    jbyte *bytes = GetByteArrayElements(jenv, data, &is_copy);
    assert(bytes);

    // define class
    jclass clss = DefineClass(jenv, clssname, cl, bytes + offset, len);

    // release JNI objects
    ReleaseByteArrayElements(jenv, data, bytes, 0);
    
    if(clssname)
      ReleaseStringUTFChars(jenv, name, clssname);

    return clss;
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    findLoadedClass
 * Signature: (Ljava/lang/String;Ljava/lang/ClassLoader;)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_java_lang_VMClassRegistry_findLoadedClass
    (JNIEnv *jenv_ext, jclass, jstring name, jobject cl)
{
    // check the name is provided
    if (name == NULL) {
        ThrowNew_Quick(jenv_ext, "java/lang/NullPointerException", "null class name value.");
        return NULL;
    }

    // obtain char* for the name
    JNIEnv_Internal *jenv = (JNIEnv_Internal *)jenv_ext;
    unsigned length = GetStringUTFLength(jenv, name);
    char* buf = (char*)STD_MALLOC(length+1);
    assert(buf);
    GetStringUTFRegion(jenv, name, 0, GetStringLength(jenv, name), buf);

    // check for wrong symbols
    if (strcspn(buf, "/[;") < length) {
        STD_FREE(buf);
        return NULL;
    }

    // filter out primitive types

    Global_Env *ge = jenv->vm->vm_env;
    Class* primitives[] = {
        ge->Boolean_Class,
        ge->Char_Class,
        ge->Float_Class,
        ge->Double_Class,
        ge->Byte_Class,
        ge->Short_Class,
        ge->Int_Class,
        ge->Long_Class,
        ge->Void_Class,
    };
    int primitives_len = sizeof(primitives) / sizeof(Class*);

    for (int i = 0; i < primitives_len; i++) {
        if (primitives[i] && primitives[i]->name) {
            char *pname = (char*)primitives[i]->name->bytes;
            if (0 == strcmp(buf, pname)) { 
                STD_FREE(buf);
                return NULL;
            }
        }
    }

    ClassLoaderHandle loader = NULL;
    Class_Handle clss = NULL;
    jclass jclss = NULL;

    if(cl) {
        // if non null class loader is provided, search among loaded classes
        loader = class_loader_lookup(cl);
        clss = class_find_loaded(loader, buf);
    } else {
        // if null class loader is specified
        // load class using bootstrap class loader
        // clss = class_find_class_from_loader(NULL, buf, TRUE);
        clss = class_find_class_from_loader(NULL, buf, FALSE);
    }

    STD_FREE(buf);

    if (clss)
        jclss = jni_class_from_handle(jenv, clss);

    if (ExceptionOccurred(jenv)) {
        ExceptionClear(jenv);
        assert(jclss == NULL);
    }

    return jclss;
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getClass
 * Signature: (Ljava/lang/Object;)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_java_lang_VMClassRegistry_getClass
  (JNIEnv *jenv, jclass, jobject jobj)
{
    // use JNI API function
    return GetObjectClass(jenv, jobj);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getClassLoader
 * Signature: (Ljava/lang/Class;)Ljava/lang/ClassLoader;
 */
JNIEXPORT jobject JNICALL Java_java_lang_VMClassRegistry_getClassLoader
  (JNIEnv *jenv, jclass, jclass clazz)
{
    Class_Handle clss = jni_get_class_handle(jenv, clazz);
    ClassLoaderHandle clh = class_get_class_loader(clss);
    return jni_class_loader_from_handle(jenv, clh);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getComponentType
 * Signature: (Ljava/lang/Class;)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_java_lang_VMClassRegistry_getComponentType
  (JNIEnv *jenv, jclass, jclass clazz)
{
    Class_Handle ch = jni_get_class_handle(jenv, clazz);
    Class *pCompClass = ((Class*)ch)->array_element_class;
    return jni_class_from_handle(jenv, pCompClass);
}

// return true if iklass is not inner of klass
inline static bool
class_is_inner_skip( Class_Handle klass, Class_Handle iklass )
{
    Class* outer = class_get_declaring_class(iklass);

    if (outer != NULL && outer != klass) {
        return true;
    }

    return false;
}


/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getDeclaredClasses
 * Signature: (Ljava/lang/Class;)[Ljava/lang/Class;
 */
JNIEXPORT jobjectArray JNICALL Java_java_lang_VMClassRegistry_getDeclaredClasses
  (JNIEnv *jenv, jclass, jclass clazz)
{
    unsigned index, num_ic, num_res;
    Class_Handle clss, iclss;

    // get class and number of inner classes
    clss = jni_get_class_handle(jenv, clazz);
    num_ic = num_res = class_number_inner_classes(clss);

    // calculate number of declared classes
    for(index=0; index < num_ic; index++) {
        iclss = class_get_inner_class(clss, index);
        if (!iclss)
            return NULL;        
        if( class_is_inner_skip( clss, iclss ) )
            num_res--;
    }

    // create array
    jclass cclazz = struct_Class_to_java_lang_Class_Handle(VM_Global_State::loader_env->JavaLangClass_Class);
    jobjectArray res = NewObjectArray(jenv, num_res, cclazz, NULL);

    // set array
    for( index = 0, num_res = 0; index < num_ic; index++) {
        iclss = class_get_inner_class(clss, index);
        if( class_is_inner_skip( clss, iclss ) )
            continue;
        SetObjectArrayElement(jenv, res, num_res++, jni_class_from_handle(jenv, iclss));
    }
    return res;
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getDeclaredConstructors
 * Signature: (Ljava/lang/Class;)[Ljava/lang/reflect/Constructor;
 */
JNIEXPORT jobjectArray JNICALL Java_java_lang_VMClassRegistry_getDeclaredConstructors
  (JNIEnv *jenv, jclass, jclass clazz)
{
    return reflection_get_class_constructors(jenv, clazz);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getDeclaredFields
 * Signature: (Ljava/lang/Class;)[Ljava/lang/reflect/Field;
 */
JNIEXPORT jobjectArray JNICALL Java_java_lang_VMClassRegistry_getDeclaredFields
  (JNIEnv *jenv, jclass, jclass clazz)
{
    return reflection_get_class_fields(jenv, clazz);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getDeclaredMethods
 * Signature: (Ljava/lang/Class;)[Ljava/lang/reflect/Method;
 */
JNIEXPORT jobjectArray JNICALL Java_java_lang_VMClassRegistry_getDeclaredMethods
  (JNIEnv *jenv, jclass, jclass clazz)
{
    return reflection_get_class_methods(jenv, clazz);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getDeclaringClass
 * Signature: (Ljava/lang/Class;)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_java_lang_VMClassRegistry_getDeclaringClass
  (JNIEnv *jenv, jclass, jclass clazz)
{
    Class_Handle clss = jni_get_class_handle(jenv, clazz);
    Class_Handle res = class_get_declaring_class(clss);
    return jni_class_from_handle(jenv, res);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getInterfaces
 * Signature: (Ljava/lang/Class;)[Ljava/lang/Class;
 */
JNIEXPORT jobjectArray JNICALL Java_java_lang_VMClassRegistry_getInterfaces
  (JNIEnv *jenv, jclass, jclass clazz)
{
    return reflection_get_class_interfaces(jenv, clazz);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getModifiers
 * Signature: (Ljava/lang/Class;)I
 */
JNIEXPORT jint JNICALL Java_java_lang_VMClassRegistry_getModifiers
  (JNIEnv *jenv, jclass, jclass clazz)
{
    Class_Handle clss = jni_get_class_handle(jenv, clazz);
    return class_get_flags(clss) & (~ACC_SUPER);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getName
 * Signature: (Ljava/lang/Class;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_java_lang_VMClassRegistry_getName
  (JNIEnv *jenv, jclass, jclass clazz)
{
    Class* clss = jclass_to_struct_Class(clazz);
    String* str = class_get_java_name(clss, VM_Global_State::loader_env);
    return String_to_interned_jstring(str);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getSuperclass
 * Signature: (Ljava/lang/Class;)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_java_lang_VMClassRegistry_getSuperclass
  (JNIEnv *jenv, jclass, jclass clazz)
{
    Class *clss = jni_get_class_handle(jenv, clazz);

    if (class_is_interface (clss) || clss->is_primitive || !clss->super_class) {
        // Interfaces and primitive classes have no superclasses.
        return (jclass)0;
    }

    return jni_class_from_handle(jenv, clss->super_class);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    getSystemPackages
 * Signature: (I)[[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_java_lang_VMClassRegistry_getSystemPackages
  (JNIEnv *jenv, jclass, jint len)
{
    Global_Env* genv = VM_Global_State::loader_env;
    ClassLoader* cl = static_cast<ClassLoader*>
        (genv->bootstrap_class_loader);
    Package_Table* ptab = cl->getPackageTable();
    if (ptab->size() == (unsigned)len) 
    {
        return NULL;
    }

    jclass string_class = struct_Class_to_java_lang_Class_Handle(genv->JavaLangString_Class);
    static Class* aos = genv->LoadCoreClass(genv->string_pool.lookup("[Ljava/lang/String;"));
    jclass string_array_class = struct_Class_to_java_lang_Class_Handle(aos);
    assert(string_class);
    assert(string_array_class);
        
    apr_pool_t* pool;
    apr_pool_create(&pool, NULL);
    unsigned index = 0;

    cl->Lock();
    jobjectArray result = NewObjectArray(jenv, ptab->size(), string_array_class, NULL);
    assert(result);

    for (Package_Table::const_iterator it = ptab->begin(), end = ptab->end(); 
        it != end; ++it, ++index)
    {
        jobjectArray pair = NewObjectArray(jenv, 2, string_class, NULL);
        assert(pair);
        
        char* name = apr_pstrdup(pool, (*it).first->bytes);
        for (char* c = name; *c != '\0'; ++c) {
            if (*c == '/') {
                *c = '.';
            }
        }
        SetObjectArrayElement(jenv, pair, 0, NewStringUTF(jenv, name));

        const char * jar = (*it).second->get_jar();
        if (jar) {
            SetObjectArrayElement(jenv, pair, 1, NewStringUTF(jenv, jar));
        }
        
        SetObjectArrayElement(jenv, result, index, pair);
    }
    cl->Unlock();
    apr_pool_destroy(pool);

    return result;
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    initializeClass
 * Signature: (Ljava/lang/Class;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMClassRegistry_initializeClass
  (JNIEnv *jenv, jclass unused, jclass clazz)
{
    Class *clss = jni_get_class_handle(jenv, clazz);
    Java_java_lang_VMClassRegistry_linkClass(jenv, unused, clazz);
    if(jenv->ExceptionCheck())
        return;
    class_initialize_from_jni(clss);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    isArray
 * Signature: (Ljava/lang/Class;)Z
 */
JNIEXPORT jboolean JNICALL Java_java_lang_VMClassRegistry_isArray
  (JNIEnv *jenv, jclass, jclass clazz)
{
    Class_Handle ch = jni_get_class_handle(jenv, clazz);
    return (jboolean)(class_is_array(ch) ? JNI_TRUE : JNI_FALSE);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    isAssignableFrom
 * Signature: (Ljava/lang/Class;Ljava/lang/Class;)Z
 */
JNIEXPORT jboolean JNICALL Java_java_lang_VMClassRegistry_isAssignableFrom
  (JNIEnv *jenv, jclass, jclass clazz, jclass fromClazz)
{
    // check parameters
    if (!clazz)
    {
        throw_exception_from_jni(jenv, "java/lang/NullPointerException", "clazz argument");
        return JNI_FALSE;
    }

    if (!fromClazz)
    {
        throw_exception_from_jni(jenv, "java/lang/NullPointerException", "fromClazz argument");
        return JNI_FALSE;
    }

    Class_Handle ch = jni_get_class_handle(jenv, fromClazz);

    // if primitive class
    if (class_is_primitive(ch))
        return (jboolean)(IsSameObject(jenv, clazz, fromClazz) ? JNI_TRUE : JNI_FALSE);

    // if non primitive
    return IsAssignableFrom(jenv, fromClazz, clazz);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    isInstance
 * Signature: (Ljava/lang/Class;Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_java_lang_VMClassRegistry_isInstance
  (JNIEnv *jenv, jclass, jclass clazz, jobject obj)
{
    // null object
    if (!obj) return JNI_FALSE;

    return IsInstanceOf(jenv, obj, clazz);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    isInterface
 * Signature: (Ljava/lang/Class;)Z
 */
JNIEXPORT jboolean JNICALL Java_java_lang_VMClassRegistry_isInterface
  (JNIEnv *jenv, jclass, jclass clazz)
{
    Class_Handle clss = jni_get_class_handle(jenv, clazz);
    return (jboolean)(class_property_is_interface2(clss) ? JNI_TRUE : JNI_FALSE);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    isPrimitive
 * Signature: (Ljava/lang/Class;)Z
 */
JNIEXPORT jboolean JNICALL Java_java_lang_VMClassRegistry_isPrimitive
  (JNIEnv *jenv, jclass, jclass clazz)
{
    Class_Handle ch = jni_get_class_handle(jenv, clazz);
    return (jboolean)(class_is_primitive(ch) ? JNI_TRUE : JNI_FALSE);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    linkClass
 * Signature: (Ljava/lang/Class;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMClassRegistry_linkClass
  (JNIEnv *jenv, jclass, jclass clazz)
{
    // ppervov: this method intentionally left blank
    //      as in our VM classes will never get to Java
    //      unlinked (except resolution stage)
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    loadArray
 * Signature: (Ljava/lang/Class;I)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_java_lang_VMClassRegistry_loadArray
  (JNIEnv *jenv, jclass, jclass compType, jint dims)
{
    Class *clss = jni_get_class_handle(jenv, compType);
    Class *arr_clss = clss;

    for (int i = 0; i < dims; i++) {
        arr_clss = (Class *)class_get_array_of_class(arr_clss);
        if (!arr_clss)   return 0;
    }

    return jni_class_from_handle(jenv, arr_clss);
}

/*
 * Class:     java_lang_VMClassRegistry
 * Method:    loadLibrary
 * Signature: (Ljava/lang/String;Ljava/lang/ClassLoader;)V
 */
JNIEXPORT void JNICALL Java_java_lang_VMClassRegistry_loadLibrary
  (JNIEnv *jenv, jclass, jstring filename, jobject classLoader)
{
    // check filename
    if (! filename)
    {
        jclass exc_class = FindClass(jenv, VM_Global_State::loader_env->JavaLangNullPointerException_String);
        ThrowNew(jenv, exc_class, "null file name value.");
        return;
    }

    // get filename char string
    const char *str_filename = GetStringUTFChars(jenv, filename, NULL);

    // load native library
    ClassLoaderHandle loader;
    if( classLoader ) {
        loader = class_loader_lookup( classLoader );
    } else {
        // bootstrap class loader
        loader = (ClassLoaderHandle)
            ((JNIEnv_Internal*)jenv)->vm->vm_env->bootstrap_class_loader;
    }
    class_loader_load_native_lib( str_filename, loader );

    // release char string
    ReleaseStringUTFChars(jenv, filename, str_filename);
}
