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
 * @version $Revision: 1.1.2.2.4.5 $
 */  
 
#define LOG_DOMAIN "vm.core.reflection"
#include "cxxlog.h"

#include <assert.h>

#include "jni.h"
#include "jni_utils.h"
#include "environment.h"
#include "vm_strings.h"
#include "reflection.h"
#include "port_malloc.h"

#include "exceptions.h"
#include "heap.h"
#include "primitives_support.h"

Field* get_reflection_field(Class* clss, reflection_fields descriptor) 
{
    Global_Env* genv = VM_Global_State::loader_env;
    static String* extra_strings[][2] = {
        {genv->string_pool.lookup("parameterTypes"), genv->string_pool.lookup("[Ljava/lang/Class;")},
        {genv->string_pool.lookup("exceptionTypes"), genv->string_pool.lookup("[Ljava/lang/Class;")},
        {genv->string_pool.lookup("declaringClass"), genv->string_pool.lookup("Ljava/lang/Class;")},
        {genv->string_pool.lookup("name"), genv->string_pool.lookup("Ljava/lang/String;")},
        {genv->string_pool.lookup("type"), genv->string_pool.lookup("Ljava/lang/Class;")},
        {genv->string_pool.lookup("vm_member"), genv->string_pool.lookup("J")},
    };

    static Field* resolved[FIELDS_NUMBER];

    Field *field = resolved[descriptor];
    if (!field) {
        String* name = extra_strings[descriptor][0];
        String* sig = extra_strings[descriptor][1];
        TRACE("Resolving special class field : " << name->bytes);
        field = class_lookup_field_recursive(clss, name, sig);
        ASSERT(field, "Cannot find special class field : " << name->bytes << " / " << sig->bytes);
        resolved[descriptor] = field;
    }

    return field;
}

Class_Member* reflection_jobject_to_Class_Member(jobject jmember, Class* type)
{
    Field *field = get_reflection_field(type, VM_MEMBER);

    tmn_suspend_disable();
    Byte *java_ref = (Byte *)jmember->object;
    jlong member = *(jlong *)(java_ref + field->get_offset());
    tmn_suspend_enable(); 
    assert(member);

    return (Class_Member*) ((POINTER_SIZE_INT) member);
}

static Class_Handle get_class(Type_Info_Handle type_info) {
    Class_Handle clss = type_info_get_class(type_info);

    if (!clss) {
        if (!exn_raised()) {
            exn_raise_only((jthrowable) type_info_get_loading_error(type_info));
        }
    }
    return clss;
}

// Set parameterTypes and exceptionTypes fields for Method or Constructor object
jobjectArray reflection_get_parameter_types(JNIEnv *jenv, Class* type, jobject jmethod, Method* method)
{
    if (!method) {
        method = (Method*)reflection_jobject_to_Class_Member(jmethod, type);
    }
    jclass jlc_class = struct_Class_to_java_lang_Class_Handle(VM_Global_State::loader_env->JavaLangClass_Class);

    // Create an array of the argument types
    Method_Signature_Handle msh = method_get_signature(method);
    int nparams = method_args_get_number(msh);
    int start = method->is_static() ? 0 : 1;
    if (start) --nparams;

    jobjectArray arg_types = NewObjectArray(jenv, nparams, jlc_class, NULL);

    if (!arg_types) {
        return NULL;
    }

    int i;
    for (i = 0; i < nparams; i++) 
    {
        Type_Info_Handle arg_type = method_args_get_type_info(msh, i+start);
        Class_Handle arg_clss = get_class(arg_type);
        if (!arg_clss) return NULL;
        SetObjectArrayElement(jenv, arg_types, i, struct_Class_to_java_lang_Class_Handle(arg_clss));
    }

    // Set paramaterTypes field
    //jfieldID parameterTypes_id = (jfieldID)get_reflection_field(type, PARAMETERS);
    //SetObjectField(jenv, jmethod, parameterTypes_id, arg_types);

    return arg_types;
}

// Set name field for Method or Field object
static bool set_name_field(JNIEnv *jenv, String* value, jobject jmember, Class* type)
{    
    jstring name = String_to_interned_jstring(value);
    if (name == NULL) {
        assert(exn_raised());
        return false;
    }

    jfieldID name_id = (jfieldID)get_reflection_field(type, NAME);
    SetObjectField(jenv, jmember, name_id, name);
    
    return true;
}

// Set type field for Method or Field object
static bool set_type_field(JNIEnv *jenv, jobject jmember, Class* type, Type_Info_Handle type_info)
{
    Class_Handle clss = get_class(type_info);
    if (!clss) {
        return false;
    }
    jfieldID type_id = (jfieldID)get_reflection_field(type, TYPE);
    SetObjectField(jenv, jmember, type_id, struct_Class_to_java_lang_Class_Handle(clss));
    return true;
}

// Construct object of member_class (Constructor, Field, Method). 
// Set declaringClass field.
static jobject reflect_member(JNIEnv *jenv, Class_Member* member, Class* type)
{
    Global_Env* genv = VM_Global_State::loader_env;
    // Construct member object
    tmn_suspend_disable();
    jobject jmember = oh_allocate_local_handle();
    ManagedObject *new_obj = class_alloc_new_object(type);
    if (!new_obj) {
        tmn_suspend_enable();
        return NULL;
    }
    jmember->object = new_obj;
    tmn_suspend_enable(); 

    // Call constructor of member class on newly allocated object 
    // passing this object itself as a parameter
    static String* descr = genv->string_pool.lookup("(Ljava/lang/Object;)V");
    Method* member_constr = class_lookup_method(type, genv->Init_String, descr);
    jvalue args;
    args.l = jmember;
    CallVoidMethodA(jenv, jmember, (jmethodID)member_constr, &args);
    if (exn_raised())
        return NULL;

    // Set vm_member field
    jfieldID member_id = (jfieldID)get_reflection_field(type, VM_MEMBER);
    jlong long_member = (jlong) ((POINTER_SIZE_INT) member);
    SetLongField(jenv, jmember, member_id, long_member);
    
    // Set declaringClass field
    jfieldID clazz_id = (jfieldID)get_reflection_field(type, DECLARING_CLASS);
    SetObjectField(jenv, jmember, clazz_id, struct_Class_to_java_lang_Class_Handle(member->get_class()));

    return jmember;
} //reflect_member

jobject reflection_reflect_method(JNIEnv *jenv, Method_Handle method)
{
    // Construct a java.lang.reflect.Method object
    Class* type = VM_Global_State::loader_env->java_lang_reflect_Method_Class;
    jobject jmethod = reflect_member(jenv, method, type);
    if (!jmethod) {
        assert(exn_raised());
        return NULL;
    }

    if (!set_name_field(jenv, method->get_name(), jmethod, type)) {
        assert(exn_raised());
        return NULL;
    }

    //Too early to try load classes for PARAMETERS and return TYPE.
    //No need to set EXCEPTIONS in advance - it is almost never requested.

    return jmethod;
}

jobject reflection_reflect_constructor(JNIEnv *jenv, Method_Handle constructor)
{
    // Construct a java.lang.reflect.Constructor object
    Class* type = VM_Global_State::loader_env->java_lang_reflect_Constructor_Class;
    jobject jconst = reflect_member(jenv, constructor, type);
    if (!jconst) 
        return NULL;

    //Too early to try load classes for PARAMETERS.
    //No need to set EXCEPTIONS in advance - it is almost never requested.
    //No need to set name in advance - it is useless for constructors.

    return jconst;
}

jobject reflection_reflect_field(JNIEnv *jenv, Field_Handle field)
{
    assert(field);

    // We do not reflect injected fields
    if (field_is_injected(field)) return NULL;
    
    // Construct a java.lang.reflect.Field object
    Class* type = VM_Global_State::loader_env->java_lang_reflect_Field_Class;
    jobject jfield = reflect_member(jenv, field, type);
    if (!jfield) 
        return NULL;

    if (!set_name_field(jenv, field->get_name(), jfield, type))
        return NULL;

    // Get type 
    Type_Info_Handle field_type = field_get_type_info_of_field_value(field);

    if (!set_type_field(jenv, jfield, type, field_type)) {
        assert(exn_raised());
        return NULL;
    }

    return jfield;
}

jobjectArray reflection_get_class_interfaces(JNIEnv* jenv, jclass clazz)
{
    Class_Handle clss = jni_get_class_handle(jenv, clazz);

    unsigned intf_number = class_number_implements(clss);

    jclass cclass = struct_Class_to_java_lang_Class_Handle( 
        VM_Global_State::loader_env->JavaLangClass_Class);

    jobjectArray arr = NewObjectArray(jenv, intf_number, cclass, NULL);
    if (! arr) 
        return NULL;

    // Fill the array
    for (unsigned i = 0; i < intf_number; i++) {
        jclass intf = jni_class_from_handle(jenv, class_get_implements(clss, i));
        SetObjectArrayElement(jenv, arr, i, intf);
        if (exn_raised()) 
            return NULL;
    }

    return arr;
}

jobjectArray reflection_get_class_fields(JNIEnv* jenv, jclass clazz)
{
    Class_Handle clss = jni_get_class_handle(jenv, clazz);
    assert(clss);
    TRACE("get class fields : " << class_get_name(clss));

    unsigned num_fields = class_number_fields(clss);
    unsigned num_res_fields = 0;

    unsigned i;
    // Determine the number of elements in the result.
    for (i = 0; i < num_fields; i++) {
        Field_Handle fh = class_get_field(clss, i);
        if (field_is_injected(fh)) continue;
        if (class_is_array(clss) && 0 == strcmp("length", field_get_name(fh))) continue;
        num_res_fields++;
    }

    // Create result array
    jclass fclazz = struct_Class_to_java_lang_Class_Handle(VM_Global_State::loader_env->java_lang_reflect_Field_Class);
    if (! fclazz) return NULL;
    jobjectArray farray = (jobjectArray) NewObjectArray(jenv, num_res_fields, fclazz, NULL);
    if (! farray) return NULL;

    // Fill in the array
    num_res_fields = 0;

    for(i = 0; i < num_fields; i++) {
        Field_Handle fh = class_get_field(clss, i);
        if (field_is_injected(fh)) continue;
        if (class_is_array(clss) && 0 == strcmp("length", field_get_name(fh))) continue;

        jobject jfield = reflection_reflect_field(jenv, fh);
        SetObjectArrayElement(jenv, farray, num_res_fields, jfield);
        if (exn_raised()) return NULL;
        num_res_fields++;
    }

    return farray;
} // reflection_get_class_fields

jobjectArray reflection_get_class_constructors(JNIEnv* jenv, jclass clazz)
{
    Class_Handle clss = jni_get_class_handle(jenv, clazz);
    unsigned num_methods = class_get_number_methods(clss);
    unsigned n_consts = 0;
    TRACE("get class constructors : " << class_get_name(clss));

    unsigned i;
    // Determine the number of elements in the result. Note that fake methods never have the name "<init>".
    for (i = 0; i < num_methods; i++) {
        Method_Handle mh = class_get_method(clss, i);
        if (strcmp(method_get_name(mh), "<init>") == 0)
            n_consts++;             
    }

    // Create result array
    jclass cclazz = struct_Class_to_java_lang_Class_Handle(
        VM_Global_State::loader_env->java_lang_reflect_Constructor_Class);
    if (!cclazz) return NULL;
    jobjectArray carray = (jobjectArray) NewObjectArray(jenv, n_consts, cclazz, NULL);
    if (!carray) return NULL;

    // Fill in the array
    for (i = 0, n_consts = 0; i < num_methods; i++) {
        Method_Handle mh = class_get_method(clss, i);
        if (strcmp(method_get_name(mh), "<init>") != 0) continue;

        jobject jconst = reflection_reflect_constructor(jenv, mh);
        SetObjectArrayElement(jenv, carray, n_consts++, jconst);
        if (exn_raised()) return NULL;
    }

    return carray;
} // reflection_get_class_constructors

jobjectArray reflection_get_class_methods(JNIEnv* jenv, jclass clazz)
{
    Class_Handle clss = jni_get_class_handle(jenv, clazz);
    unsigned num_methods = class_get_number_methods(clss);
    unsigned num_res_methods = 0;
    TRACE("get class methods : " << class_get_name(clss));

    unsigned i;
    // Determine the number of elements in the result. Note that fake methods never have the name "<init>".
    for (i = 0; i < num_methods; i++) {
        Method_Handle mh = class_get_method(clss, i);
        if (strcmp(method_get_name(mh), "<init>") == 0 ||
                strcmp(method_get_name(mh), "<clinit>") == 0 ||
                mh->is_fake_method())
            continue;

        num_res_methods++;             
    }

    // Create result array
    jclass member_class = struct_Class_to_java_lang_Class_Handle(VM_Global_State::loader_env->java_lang_reflect_Method_Class);
    if (! member_class) return NULL;
    jobjectArray member_array = NewObjectArray(jenv, num_res_methods, member_class, NULL);
    if (! member_array) return NULL;

    // Fill in the array
    unsigned member_i = 0;
    for (i = 0; i < num_methods; i++) {
        Method_Handle mh = class_get_method(clss, i);
        if (strcmp(method_get_name(mh), "<init>") == 0 ||
                strcmp(method_get_name(mh), "<clinit>") == 0 ||
                mh->is_fake_method())
            continue;

        jobject member = reflection_reflect_method(jenv, mh);
        SetObjectArrayElement(jenv, member_array, member_i, member);
        if (exn_raised()) 
            return NULL;

        member_i ++;
    }

    return member_array;
} // reflection_get_class_methods

/*
The following function eases conversion from jobjectArray parameters to 
JNI friendly jvalue array.
*/
bool jobjectarray_to_jvaluearray(JNIEnv *jenv, jvalue **output, Method *method, jobjectArray input)
{
    Arg_List_Iterator iter = method->get_argument_list();
    unsigned arg_number = 0;
    jvalue* array = *output;

    Java_Type type;
    while((type = curr_arg(iter)) != JAVA_TYPE_END) {
        jobject arg = GetObjectArrayElement(jenv, input, arg_number);
        if (type == JAVA_TYPE_ARRAY || type == JAVA_TYPE_CLASS) 
        {
            array[arg_number].l = arg;
        }
        else //unwrap to primitive
        {
            ASSERT(arg, "Cannot unwrap NULL");
            Class* arg_clss = jobject_to_struct_Class(arg);
            char arg_sig = is_wrapper_class(arg_clss->name->bytes);
            char param_sig = (char)type;

            ASSERT(arg_sig, "Reflection arguments mismatch: expected " 
                << param_sig << " but was " << arg_clss->name->bytes);
            
            array[arg_number] = unwrap_primitive(jenv, arg, arg_sig);

            if (!widen_primitive_jvalue(array + arg_number, arg_sig, param_sig)) {
                ThrowNew_Quick(jenv, "java/lang/IllegalArgumentException",
                    "widening conversion failed");
                return false;
            }
        }
        iter = advance_arg_iterator(iter);
        arg_number++;
    }

    return true;
} //jobjectarray_to_jvaluearray
