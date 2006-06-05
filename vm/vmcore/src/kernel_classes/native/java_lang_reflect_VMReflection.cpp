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
 * @version $Revision: 1.1.2.3.4.5 $
 */  

/**
 * @file java_lang_reflect_VMReflection.cpp
 *
 * This file is a part of kernel class natives VM core component.
 * It contains implementation for native methods of 
 * java.lang.reflect.VMReflection class.
 */

#define LOG_DOMAIN "vm.core.reflection"
#include "cxxlog.h"

#include "reflection.h"
#include "environment.h"
#include "exceptions.h"
#include "vm_strings.h"
#include "primitives_support.h"
#include "jni_utils.h"

#include "java_lang_VMClassRegistry.h"
#include "java_lang_reflect_VMReflection.h"

static jobject get_member_field(JNIEnv *jenv, Class* clss, jobject member, reflection_fields descriptor) 
{
    TRACE("get member field : " << descriptor);
    Field *field = get_reflection_field(clss, descriptor);
    jobject value = GetObjectFieldOffset(jenv, member, field->get_offset());
    assert(value);

    return value;
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    getDeclaringClass
 * Signature: (Ljava/lang/Object;)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_java_lang_reflect_VMReflection_getDeclaringClass
  (JNIEnv *jenv, jclass, jobject member) 
{
    Class* clss = jobject_to_struct_Class(member);
    
    ASSERT(clss == VM_Global_State::loader_env->java_lang_reflect_Constructor_Class
        || clss == VM_Global_State::loader_env->java_lang_reflect_Method_Class
        || clss == VM_Global_State::loader_env->java_lang_reflect_Field_Class,
        "Unsupported type of member object");

    return get_member_field(jenv, clss, member, DECLARING_CLASS);
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    getExceptionTypes
 * Signature: (Ljava/lang/Object;)[Ljava/lang/Class;
 */
JNIEXPORT jobjectArray JNICALL Java_java_lang_reflect_VMReflection_getExceptionTypes
  (JNIEnv *jenv, jclass, jobject member)
{
    Class* clss = jobject_to_struct_Class(member);

    ASSERT(clss == VM_Global_State::loader_env->java_lang_reflect_Constructor_Class
        || clss == VM_Global_State::loader_env->java_lang_reflect_Method_Class,
        "Unsupported type of member object");

    Method* method = (Method*)reflection_jobject_to_Class_Member(member, clss);
    jclass jlc_class = struct_Class_to_java_lang_Class_Handle(
        VM_Global_State::loader_env->JavaLangClass_Class);

    // Create and fill exceptions array
    int n_exceptions = method_number_throws(method);
    jobjectArray exceptionTypes = NewObjectArray(jenv, n_exceptions, jlc_class, NULL);

    if (!exceptionTypes) {
        assert(exn_raised());
        return NULL;
    }

    int i = 0;
    for (; i < n_exceptions; i++) {
        Class_Handle exclass = method_get_throws(method, i);
        if (!exclass) {
            assert(exn_raised());
            return NULL;
        }
        SetObjectArrayElement(jenv, exceptionTypes, i, 
            struct_Class_to_java_lang_Class_Handle(exclass));
    }

    return exceptionTypes;
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    getModifiers
 * Signature: (Ljava/lang/Object;)I
 */
JNIEXPORT jint JNICALL Java_java_lang_reflect_VMReflection_getModifiers
  (JNIEnv * UNREF jenv, jclass, jobject member)
{
    Class* clss = jobject_to_struct_Class(member);

    ASSERT(clss == VM_Global_State::loader_env->java_lang_reflect_Constructor_Class
        || clss == VM_Global_State::loader_env->java_lang_reflect_Method_Class
        || clss == VM_Global_State::loader_env->java_lang_reflect_Field_Class,
        "Unsupported type of member object");

    return reflection_jobject_to_Class_Member(member, clss)->get_access_flags();
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    getName
 * Signature: (Ljava/lang/Object;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_java_lang_reflect_VMReflection_getName
  (JNIEnv *jenv, jclass, jobject member)
{
    Class* clss = jobject_to_struct_Class(member);

    if (clss == VM_Global_State::loader_env->java_lang_reflect_Constructor_Class) 
    {
        Class* declaringClass = jclass_to_struct_Class(
            get_member_field(jenv, clss, member, DECLARING_CLASS));
        String *str = class_get_java_name(declaringClass, VM_Global_State::loader_env);
        jstring name = String_to_interned_jstring(str);
        if (name == NULL) {
            assert(exn_raised());
        }
        return name;
    } 
    else if (clss == VM_Global_State::loader_env->java_lang_reflect_Method_Class
        || clss == VM_Global_State::loader_env->java_lang_reflect_Field_Class) 
    {
        return get_member_field(jenv, clss, member, NAME);
    }
    else 
    {
        ASSERT(0, "Unsupported type of member object");
        return NULL;
    }
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    getParameterTypes
 * Signature: (Ljava/lang/Object;)[Ljava/lang/Class;
 */
JNIEXPORT jobjectArray JNICALL Java_java_lang_reflect_VMReflection_getParameterTypes
  (JNIEnv *jenv, jclass, jobject member)
{
    Class* clss = jobject_to_struct_Class(member);

    ASSERT(clss == VM_Global_State::loader_env->java_lang_reflect_Constructor_Class
        || clss == VM_Global_State::loader_env->java_lang_reflect_Method_Class,
        "Unsupported type of member object");

    return reflection_get_parameter_types(jenv, clss, member);
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    getMethodReturnType
 * Signature: (Ljava/lang/Object;)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_java_lang_reflect_VMReflection_getMethodReturnType
  (JNIEnv *jenv, jclass, jobject member)
{
    Class* clss = jobject_to_struct_Class(member);

    ASSERT(clss == VM_Global_State::loader_env->java_lang_reflect_Method_Class,
        "Unsupported type of member object");

    Method_Handle method = (Method_Handle)reflection_jobject_to_Class_Member(member, clss);
    Method_Signature_Handle msh = method_get_signature(method);
    Type_Info_Handle ret_type = method_ret_type_get_type_info(msh);
    Class_Handle ret_clss = type_info_get_class(ret_type);

    if (!ret_clss) {
        if (!exn_raised()) {
            exn_raise_only((jthrowable) type_info_get_loading_error(ret_type));
        }
        return NULL;
    }
    return struct_Class_to_java_lang_Class_Handle(ret_clss);
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    getFieldType
 * Signature: (Ljava/lang/Object;)Ljava/lang/Class;
 */
JNIEXPORT jclass JNICALL Java_java_lang_reflect_VMReflection_getFieldType
  (JNIEnv *jenv, jclass, jobject member)
{
    Class* clss = jobject_to_struct_Class(member);

    ASSERT(clss == VM_Global_State::loader_env->java_lang_reflect_Field_Class,
        "Unsupported type of member object");

    return get_member_field(jenv, clss, member, TYPE);
}

// return value of a field of primitive type.
static jobject get_primitive_field(JNIEnv* jenv, jfieldID field_id, jclass declaring_class, jobject obj)
{
    char field_sig = ((Field*) field_id)->get_descriptor()->bytes[0];
    bool is_static = ((Field*) field_id)->is_static();
    jvalue primitive_value;

    switch (field_sig) {
    case 'B':
        primitive_value.b = (is_static) ?
                GetStaticByteField(jenv, declaring_class, field_id) :
                GetByteField(jenv, obj, field_id);
        break;
    case 'C':
        primitive_value.c = (is_static) ?
                GetStaticCharField(jenv, declaring_class, field_id) :
                GetCharField(jenv, obj, field_id);
        break;
    case 'D':
        primitive_value.d = (is_static) ?
                GetStaticDoubleField(jenv, declaring_class, field_id) :
                GetDoubleField(jenv, obj, field_id);
        break;
    case 'F':
        primitive_value.f = (is_static) ?
                GetStaticFloatField(jenv, declaring_class, field_id) :
                GetFloatField(jenv, obj, field_id);
        break;
    case 'I':
        primitive_value.i = (is_static) ?
                GetStaticIntField(jenv, declaring_class, field_id) :
                GetIntField(jenv, obj, field_id);
        break;
    case 'J':
        primitive_value.j = (is_static) ?
                GetStaticLongField(jenv, declaring_class, field_id) :
                GetLongField(jenv, obj, field_id);
        break;
    case 'S':
        primitive_value.s = (is_static) ?
                GetStaticShortField(jenv, declaring_class, field_id) :
                GetShortField(jenv, obj, field_id);
        break;
    case 'Z':
        primitive_value.z = (is_static) ?
                GetStaticBooleanField(jenv, declaring_class, field_id) :
                GetBooleanField(jenv, obj, field_id);
        break;
    default:
        ASSERT(0, "Unexpected type descriptor");
    }
    
    if (exn_raised()) 
        return NULL;   

    return wrap_primitive(jenv, primitive_value, field_sig);
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    getFieldValue
 * Signature: (Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_java_lang_reflect_VMReflection_getFieldValue
  (JNIEnv *jenv, jclass, jobject obj, jobject member)
{
    Class* type = VM_Global_State::loader_env->java_lang_reflect_Field_Class;
    Field* field = (Field*)reflection_jobject_to_Class_Member(member, type);

    jclass declaring_class = struct_Class_to_java_lang_Class_Handle(field->get_class());
        //get_member_field(jenv, type, member, DECLARING_CLASS);

    TRACE("get field value : " << field->get_class()->name->bytes << "." << field->get_name()->bytes);

    jobject retobj = NULL;

    if (field->get_field_type_desc()->is_primitive()) 
    {
        retobj = get_primitive_field(jenv, (jfieldID)field, declaring_class, obj);
    } 
    else if (field->is_static()) 
    { 
        retobj = GetStaticObjectField(jenv, declaring_class, (jfieldID)field);
    } 
    else 
    {
        retobj = GetObjectField(jenv, obj, (jfieldID)field);    
    } 

    return retobj;
} // Java_java_lang_reflect_VMReflection_getFieldValue

/* 
 * Returns false if no exception is set. If exception is set and is not 
 * an Error descendant, wraps it into new InvocationTargetException. 
 */
static bool rethrow_invocation_exception(JNIEnv* jenv) {
    jthrowable exn = ExceptionOccurred(jenv);
    if (!exn)
        return false;

    Global_Env* genv = VM_Global_State::loader_env;
    if (exn == genv->java_lang_OutOfMemoryError) {
        return true;
    }

    // API specifications only requires ExceptionInInitializerError
    // to pass through intact, and says nothing about other Errors;
    // but this is reasonable for any Error
    Class* Error_class = genv->java_lang_Error_Class;
    Class *exc_class = jobject_to_struct_Class(exn);

    while (exc_class && exc_class != Error_class) {        
        exc_class = exc_class->super_class;
    }

    if (exc_class != Error_class) {
        exn_clear();
        //static Class* ITE_class = genv->LoadCoreClass(
        //     genv->string_pool.lookup("java/lang/reflect/InvocationTargetException"));
        jobject ite_exn = exn_create("java/lang/reflect/InvocationTargetException", exn);
        if (!exn_raised()) {
            exn_raise_only(ite_exn ? ite_exn : genv->java_lang_OutOfMemoryError);
        }
    }

    return true;
}

// Invoke method with primitive return type. Return result in wrapper. For void return NULL.
static jobject invoke_primitive_method(JNIEnv* jenv, jobject obj, jclass declaring_class, Method* method, jvalue* jvalue_args)
{
    jmethodID method_id = (jmethodID) method;
    bool is_static = method->is_static();
    Java_Type return_type = method->get_return_java_type();
    jvalue result;

    switch(return_type) {
    case JAVA_TYPE_BOOLEAN:
        if (is_static)
            result.z = CallStaticBooleanMethodA(jenv, declaring_class, method_id, jvalue_args);
        else
            result.z = CallBooleanMethodA(jenv, obj, method_id, jvalue_args);
        break;
    case JAVA_TYPE_BYTE:
        if (is_static)
            result.b = CallStaticByteMethodA(jenv, declaring_class, method_id, jvalue_args);
        else
            result.b = CallByteMethodA(jenv, obj, method_id, jvalue_args);
        break;
    case JAVA_TYPE_CHAR:
        if (is_static)
            result.c = CallStaticCharMethodA(jenv, declaring_class, method_id, jvalue_args);
        else
            result.c = CallCharMethodA(jenv, obj, method_id, jvalue_args);
        break;
    case JAVA_TYPE_SHORT:
        if (is_static)
            result.s = CallStaticShortMethodA(jenv, declaring_class, method_id, jvalue_args);
        else
            result.s = CallShortMethodA(jenv, obj, method_id, jvalue_args);
        break;
    case JAVA_TYPE_INT:
        if (is_static)
            result.i = CallStaticIntMethodA(jenv, declaring_class, method_id, jvalue_args);
        else
            result.i = CallIntMethodA(jenv, obj, method_id, jvalue_args);
        break;
    case JAVA_TYPE_LONG:
        if (is_static)
            result.j = CallStaticLongMethodA(jenv, declaring_class, method_id, jvalue_args);
        else
            result.j = CallLongMethodA(jenv, obj, method_id, jvalue_args);
        break;
    case JAVA_TYPE_FLOAT:
        if (is_static)
            result.f = CallStaticFloatMethodA(jenv, declaring_class, method_id, jvalue_args);
        else
            result.f = CallFloatMethodA(jenv, obj, method_id, jvalue_args);
        break;
    case JAVA_TYPE_DOUBLE:
        if (is_static)
            result.d = CallStaticDoubleMethodA(jenv, declaring_class, method_id, jvalue_args);
        else
            result.d = CallDoubleMethodA(jenv, obj, method_id, jvalue_args);
        break;
    case JAVA_TYPE_VOID:
        if (is_static)
            CallStaticVoidMethodA(jenv, declaring_class, method_id, jvalue_args);
        else
            CallVoidMethodA(jenv, obj, method_id, jvalue_args);

        return NULL;
        break;
    default:
        ABORT("Unexpected java type");
    }

    return wrap_primitive(jenv, result, (char)return_type);
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    invokeMethod
 * Signature: (Ljava/lang/Object;Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_java_lang_reflect_VMReflection_invokeMethod
  (JNIEnv *jenv, jclass, jobject obj, jobject jmethod, jobjectArray args)
{
    Class* type = VM_Global_State::loader_env->java_lang_reflect_Method_Class;
    Method* method = (Method*)reflection_jobject_to_Class_Member(jmethod, type);

    TRACE("invoke : " << method->get_class()->name->bytes << "."  << method->get_name()->bytes << "()");

    jclass declaring_class = struct_Class_to_java_lang_Class_Handle(method->get_class());
    //get_member_field(jenv, type, jmethod, DECLARING_CLASS);

    unsigned num_args = method->get_num_args();
    jvalue *jvalue_args = (jvalue *)STD_ALLOCA(num_args * sizeof(jvalue));
    if (!jobjectarray_to_jvaluearray(jenv, &jvalue_args, method, args)) {
        return NULL;   
    }
    
    jobject jresult;

    Java_Type return_type = method->get_return_java_type();
    if (return_type != JAVA_TYPE_CLASS && return_type != JAVA_TYPE_ARRAY) 
    {
        jresult = invoke_primitive_method(jenv, obj, declaring_class, method, jvalue_args);
    } 
    else if (method->is_static()) 
    {
        jresult = CallStaticObjectMethodA(jenv, declaring_class, (jmethodID)method, jvalue_args);
    }
    else
    {
        jresult = CallObjectMethodA(jenv, obj, (jmethodID)method, jvalue_args);
    }

    // check and chain exception, if occurred
    if (rethrow_invocation_exception(jenv)) {
        return NULL;
    }
    
    return jresult;
} // Java_java_lang_reflect_VMReflection_invokeMethod

// create multidimensional array with depth dimensions specified by dims
jobject createArray(JNIEnv *jenv, jclass compType, jint *dims, int depth)
{
    jint size = dims[0];
    jclass clazz;
    
    if (depth > 1) 
        clazz = Java_java_lang_VMClassRegistry_loadArray(jenv, NULL, compType, (jint)(depth - 1));
    else
        clazz = compType;

    jobjectArray jarray = NewObjectArray(jenv, size, clazz, NULL);

    if (exn_raised()) {
        return NULL;
    }

    if (depth > 1)
    {
        for (int i = 0; i < size; i++)
        {
            jobject elem = createArray(jenv, compType, dims + 1, depth - 1);
            SetObjectArrayElement(jenv, jarray, i, elem);
        }
    }

    return jarray;
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    newArrayInstance
 * Signature: (Ljava/lang/Class;[I)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_java_lang_reflect_VMReflection_newArrayInstance
  (JNIEnv *jenv, jclass, jclass compType, jintArray jdims)
{
    jint depth = GetArrayLength(jenv, jdims);
    TRACE("new array: depth=" << depth);

    if (depth <= 0 || depth > 255) {
        const char *message = (depth <= 0) ? "zero-dimensional array specified." : 
                "requested dimensions number exceeds 255 supported limit." ;
        ThrowNew_Quick(jenv, "java/lang/IllegalArgumentException", message);
        return NULL;
    }

    jint* dims = GetIntArrayElements(jenv, jdims, NULL);

    for (int i = 0; i < depth; i++) {
        if (dims[i] < 0) {
            ReleaseIntArrayElements(jenv, jdims, dims, JNI_ABORT);
            ThrowNew_Quick(jenv, "java/lang/NegativeArraySizeException", 
                    "one of the specified dimensions is negative.");
            return NULL;
        }
    }

    jobject jarray = createArray(jenv, compType, dims, depth);

    ReleaseIntArrayElements(jenv, jdims, dims, JNI_ABORT);

    return jarray;
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    newClassInstance
 * Signature: (Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_java_lang_reflect_VMReflection_newClassInstance
  (JNIEnv *jenv, jclass, jobject jconstr, jobjectArray args)
{
    Method* method = (Method*)reflection_jobject_to_Class_Member(jconstr, VM_Global_State::loader_env->java_lang_reflect_Constructor_Class);

    TRACE("new class instance : " << method->get_class()->name->bytes);

    unsigned num_args = method->get_num_args();
    jvalue *jvalue_args = (jvalue *)STD_ALLOCA(num_args * sizeof(jvalue));
    if (!jobjectarray_to_jvaluearray(jenv, &jvalue_args, method, args)) {
        return NULL;   
    }

    jclass declaring_class = struct_Class_to_java_lang_Class_Handle(method->get_class());
    //get_member_field(jenv, VM_Global_State::loader_env->java_lang_reflect_Constructor_Class, jconstr, DECLARING_CLASS);

    // Create object
    jobject new_object = NewObjectA(jenv, declaring_class, (jmethodID) method, jvalue_args);

    // check and chain exception, if occurred
    if (rethrow_invocation_exception(jenv)) {
        return NULL;
    }

    return new_object;

} // Java_java_lang_reflect_VMReflection_newClassInstance

// set value to field of primitive type
static void set_primitive_field(JNIEnv* jenv, Field* field, jclass declaring_class, jobject obj, jobject value)
{

    // class of the value object:
    jclass value_class = GetObjectClass(jenv, value);
    assert(value_class);

    char value_sig = is_wrapper_class(jclass_to_struct_Class(value_class)->name->bytes);

    // If the value is not primitive
    if ( value_sig == '\0' ) {
        ThrowNew_Quick(jenv, "java/lang/IllegalArgumentException",
                "unwrapping conversion failed");
        return;
    }    

    jvalue primitive_value = unwrap_primitive(jenv, value, value_sig);

    char field_sig = field->get_descriptor()->bytes[0];

    if (! widen_primitive_jvalue(&primitive_value, value_sig, field_sig)) {
        ThrowNew_Quick(jenv, "java/lang/IllegalArgumentException", 
                "widening conversion failed");
        return;
    }

    jfieldID field_id = (jfieldID) field;
    bool is_static = field->is_static();

    switch (field_sig) {
    case 'B':
        if (is_static)
            SetStaticByteField(jenv, declaring_class, field_id, primitive_value.b);
        else
            SetByteField(jenv, obj, field_id, primitive_value.b);

        break;
    case 'C':
        if (is_static)
            SetStaticCharField(jenv, declaring_class, field_id, primitive_value.c);
        else
            SetCharField(jenv, obj, field_id, primitive_value.c);

        break;
    case 'D':
        if (is_static)
            SetStaticDoubleField(jenv, declaring_class, field_id, primitive_value.d);
        else
            SetDoubleField(jenv, obj, field_id, primitive_value.d);

        break;
    case 'F':
        if (is_static)
            SetStaticFloatField(jenv, declaring_class, field_id, primitive_value.f);
        else
            SetFloatField(jenv, obj, field_id, primitive_value.f);

        break;
    case 'I':
        if (is_static)
            SetStaticIntField(jenv, declaring_class, field_id, primitive_value.i);
        else
            SetIntField(jenv, obj, field_id, primitive_value.i);

        break;
    case 'J':
        if (is_static)
            SetStaticLongField(jenv, declaring_class, field_id, primitive_value.j);
        else
            SetLongField(jenv, obj, field_id, primitive_value.j);

        break;
    case 'S':
        if (is_static)
            SetStaticShortField(jenv, declaring_class, field_id, primitive_value.s);
        else
            SetShortField(jenv, obj, field_id, primitive_value.s);

        break;
    case 'Z':
        if (is_static)
            SetStaticBooleanField(jenv, declaring_class, field_id, primitive_value.z);
        else
            SetBooleanField(jenv, obj, field_id, primitive_value.z);

        break;
    default:
        ASSERT(0, "Unexpected type descriptor");
    }

    return;
}

/*
 * Class:     java_lang_reflect_VMReflection
 * Method:    setFieldValue
 * Signature: (Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_java_lang_reflect_VMReflection_setFieldValue
  (JNIEnv *jenv, jclass, jobject obj, jobject jfield, jobject value)
{
    Class* type = VM_Global_State::loader_env->java_lang_reflect_Field_Class;
    Field* field = (Field*)reflection_jobject_to_Class_Member(jfield, type);
    jclass declaring_class = struct_Class_to_java_lang_Class_Handle(field->get_class());
        //get_member_field(jenv, type, jfield, DECLARING_CLASS);

    TRACE("set field value : " << field->get_class()->name->bytes << "." << field->get_name()->bytes);

    if (field->get_field_type_desc()->is_primitive()) 
    {
        set_primitive_field(jenv, field, declaring_class, obj, value);
    } 
    else if (field->is_static())
    {
        SetStaticObjectField(jenv, declaring_class, (jfieldID)field, value);
    } 
    else 
    {
        SetObjectField(jenv, obj, (jfieldID)field, value);
    }
}
