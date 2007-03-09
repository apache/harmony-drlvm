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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.6.2.1.2.4 $
 */

#define LOG_DOMAIN "vm.core"
#include "cxxlog.h"

#include "classloader.h"
#include "lock_manager.h"
#include "compile.h"
#include "method_lookup.h"
#include "vm_arrays.h"
#include "vm_strings.h"
#include "properties.h"

#include "open/hythread_ext.h"
#include "thread_manager.h"
#include "cci.h"

#include "class_interface.h"
#include "Package.h"

Boolean class_property_is_final(Class_Handle cl) {
    assert(cl);
    return cl->is_final();
}

Boolean class_property_is_abstract(Class_Handle cl) {
    assert(cl);
    return cl->is_abstract();
}

Boolean class_property_is_interface2(Class_Handle cl) {
    assert(cl);
    return cl->is_interface();
}


Boolean class_is_array(Class_Handle cl) {
    assert(cl);
    return cl->is_array();
} //class_is_array


Boolean     class_get_array_num_dimensions(Class_Handle cl) {
    assert(cl);
    return cl->get_number_of_dimensions();
}

const char* class_get_package_name(Class_Handle cl) {
    assert(cl);
    return cl->get_package()->get_name()->bytes;
}

Boolean     method_is_abstract(Method_Handle m) {
    assert(m);
    return ((Method*)m)->is_abstract();
} // method_is_abstract



Boolean field_is_static(Field_Handle f)
{
    assert(f);
    return ((Field *)f)->is_static();
}



Boolean field_is_final(Field_Handle f)
{
    assert(f);
    return ((Field *)f)->is_final();
}


Boolean field_is_volatile(Field_Handle f)
{
    assert(f);
    return ((Field *)f)->is_volatile();
}



Boolean field_is_private(Field_Handle f)
{
    assert(f);
    return ((Field *)f)->is_private();
}



Boolean field_is_public(Field_Handle f)
{
    assert(f);
    return ((Field*)f)->is_public();
}



unsigned field_get_offset(Field_Handle f)
{
    assert(f);
    assert(!((Field *)f)->is_static());
    return ((Field *)f)->get_offset();
} //field_get_offset



void* field_get_address(Field_Handle f)
{
    assert(f);
    assert(f->is_static());
    return f->get_address();
} // field_get_address



const char* field_get_name(Field_Handle f)
{
    assert(f);
    return ((Field *)f)->get_name()->bytes;
}



const char* field_get_descriptor(Field_Handle f)
{
    assert(f);
    return ((Field *)f)->get_descriptor()->bytes;
}



Java_Type   field_get_type(Field_Handle f)
{
    assert(f);
    return ((Field *)f)->get_java_type();
} // field_get_type



unsigned field_get_flags(Field_Handle f)
{
    assert(f);
    return ((Field *)f)->get_access_flags();
}



Class_Handle field_get_class(Field_Handle f)
{
    assert(f);
    return ((Field *)f)->get_class();
} //field_get_class

void field_get_track_access_flag(Field_Handle f, char** address,
                                 char* mask)
{
    return ((Field *)f)->get_track_access_flag(address, mask);
}

void field_get_track_modification_flag(Field_Handle f, char** address,
                                 char* mask)
{
    return ((Field *)f)->get_track_modification_flag(address, mask);
}

Boolean method_is_static(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->is_static();
}


Boolean method_is_final(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->is_final();
}




Boolean method_is_synchronized(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->is_synchronized();
} //method_is_synchronized



Boolean method_is_private(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->is_private();
} //method_is_private

Boolean method_is_public(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->is_public();
}


Boolean method_is_strict(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->is_strict();
} // method_is_strict




Boolean method_is_native(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->is_native();
} // method_is_native


Byte *method_allocate_info_block(Method_Handle m, JIT_Handle j, size_t size)
{
    assert(m);
    return (Byte *)((Method *)m)->allocate_jit_info_block(size, (JIT *)j);
} //method_allocate_info_block



Byte *method_allocate_jit_data_block(Method_Handle m, JIT_Handle j, size_t size, size_t alignment)
{
    assert(m);
    return (Byte *)((Method *)m)->allocate_JIT_data_block(size, (JIT *)j, alignment);
} //method_allocate_jit_data_block




Byte *method_get_info_block_jit(Method_Handle m, JIT_Handle j)
{
    assert(m);
    Method* method = (Method*) m;
    CodeChunkInfo *jit_info = method->get_chunk_info_no_create_mt((JIT *)j, CodeChunkInfo::main_code_chunk_id);
    if (jit_info == NULL) {
        return NULL;
    }
    return (Byte *)jit_info->_jit_info_block;
} //method_get_info_block_jit




unsigned method_get_info_block_size_jit(Method_Handle m, JIT_Handle j)
{
    assert(m);
    CodeChunkInfo *jit_info = ((Method *)m)->get_chunk_info_no_create_mt((JIT *)j, CodeChunkInfo::main_code_chunk_id);
    return jit_info == 0 ? 0 : (unsigned) jit_info->_jit_info_block_size;
} //method_get_info_block_size_jit



Byte *method_allocate_data_block(Method_Handle m, JIT_Handle j, size_t size, size_t alignment)
{
    assert(m);
    return (Byte *)((Method *)m)->allocate_rw_data_block(size, alignment, (JIT *)j);
} //method_allocate_data_block


Byte *
method_allocate_code_block(Method_Handle m,
                           JIT_Handle j,
                           size_t size,
                           size_t alignment,
                           unsigned heat,
                           int id,
                           Code_Allocation_Action action)
{
    Method *method = (Method *)m;
    assert(method);

    Class_Handle UNUSED clss = method_get_class(method);
    assert(clss);

    JIT *jit = (JIT *)j;
    assert(jit);

    Byte *code_block = NULL;
    // the following method is safe to call from multiple threads
    code_block = (Byte *) method->allocate_code_block_mt(size, alignment, jit, heat, id, action);

    return code_block;
} // method_allocate_code_block



void method_set_relocatable(Method_Handle m, JIT_Handle j, NativeCodePtr code_address, Boolean is_relocatable)
{
    Global_Env *env = VM_Global_State::loader_env;
    CodeChunkInfo *cci = env->vm_methods->find(code_address);
    assert(cci);
    assert(cci->get_jit() == j);
    assert(cci->get_method() == m);
    cci->set_relocatable(is_relocatable);
} //method_set_relocatable




Byte *method_get_code_block_addr_jit(Method_Handle m, JIT_Handle j)
{
    assert(m);
    return method_get_code_block_addr_jit_new(m, j, 0);
} //method_get_code_block_addr_jit



unsigned method_get_code_block_size_jit(Method_Handle m, JIT_Handle j)
{
    assert(m);
    return method_get_code_block_size_jit_new(m, j, 0);
} //method_get_code_block_size_jit



Byte *method_get_code_block_addr_jit_new(Method_Handle method,
                                         JIT_Handle j,
                                         int id)
{
    assert(method);
    CodeChunkInfo *jit_info = ((Method *)method)->get_chunk_info_no_create_mt((JIT *)j, id);
    assert(jit_info);
    return (Byte *)jit_info->get_code_block_addr();
} //method_get_code_block_addr_jit_new



unsigned method_get_code_block_size_jit_new(Method_Handle method,
                                            JIT_Handle j,
                                            int id)
{
    assert(method);
    CodeChunkInfo *jit_info = ((Method *)method)->get_chunk_info_no_create_mt((JIT *)j, id);
    return jit_info == 0 ? 0 :(unsigned) jit_info->get_code_block_size();
} //method_get_code_block_size_jit_new


const Byte *method_get_byte_code_addr(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_byte_code_addr();
}



size_t method_get_byte_code_size(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_byte_code_size();
}



unsigned method_get_max_locals(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_max_locals();
}



unsigned method_get_max_stack(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_max_stack();
}



unsigned    method_get_flags(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_access_flags();
}



unsigned    method_get_offset(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_offset();
}



void *method_get_indirect_address(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_indirect_address();
} //method_get_indirect_address

const char *
method_get_name(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_name()->bytes;
}



const char* method_get_descriptor(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_descriptor()->bytes;
}



Class_Handle method_get_class(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_class();
}



Java_Type method_get_return_type(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_return_java_type();
}



Boolean method_is_overridden(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->is_overridden();
} //method_is_overridden



Boolean method_uses_fastcall(Method_Handle m)
{
    assert(m);
    return FALSE;
} //method_uses_fastcall



Boolean method_is_fake(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->is_fake_method();
} //method_is_fake



JIT_Handle method_get_JIT_id(Compile_Handle h)
{
    assert(h);
    Compilation_Handle* ch = (Compilation_Handle*)h;
    return ch->jit;
} //method_get_JIT_id



void method_set_inline_assumption(Compile_Handle h,
                                  Method_Handle caller,
                                  Method_Handle callee)
{
    assert(h);
    assert(caller);
    assert(callee);
    Compilation_Handle* ch = (Compilation_Handle*)h;
    JIT *jit = ch->jit;
    Method *caller_method = (Method *)caller;
    Method *callee_method = (Method *)callee;
    callee_method->set_inline_assumption(jit, caller_method);
} //method_set_inline_assumption



Method_Iterator method_get_first_method_jit(JIT_Handle j)
{
    Global_Env *env = VM_Global_State::loader_env;
    return env->vm_methods->get_first_method_jit((JIT *)j);
} //method_get_first_method_jit



Method_Iterator method_get_next_method_jit(Method_Iterator i)
{
    Global_Env *env = VM_Global_State::loader_env;
    return env->vm_methods->get_next_method_jit((CodeChunkInfo *)i);
} //method_get_next_method_jit



Method_Handle method_get_method_jit(Method_Iterator i)
{
    return ((CodeChunkInfo *)i)->get_method();
} //method_get_method_jit

const char* class_get_name(Class_Handle cl)
{
    assert(cl);
    return cl->get_name()->bytes;
} //class_get_name


unsigned class_get_flags(Class_Handle cl)
{
    assert(cl);
    return cl->get_access_flags();
} //class_get_flags


int class_get_super_offset()
{
    return sizeof(VTable*);
} //class_get_super_offset


int vtable_get_super_array_offset()
{
    VTable *vtable = 0;
    return (int) (((Byte *)&vtable->superclasses) - (Byte *)vtable);
}//vtable_get_super_array_offset


int class_get_depth(Class_Handle cl)
{
    assert(cl);
    return cl->get_depth();
} //class_get_depth

Boolean class_get_fast_instanceof_flag(Class_Handle cl)
{
    assert(cl);
    return cl->get_fast_instanceof_flag();
} //class_get_depth


Class_Handle vtable_get_class(VTable_Handle vh) {
    return vh->clss;
} // vtable_get_class

Boolean class_is_initialized(Class_Handle ch)
{
    assert(ch);
    return ch->is_initialized();
} //class_is_initialized



Boolean class_needs_initialization(Class_Handle ch)
{
    assert(ch);
    return !ch->is_initialized();
} //class_needs_initialization



Boolean class_has_non_default_finalizer(Class_Handle cl)
{
    assert(cl);
    return cl->has_finalizer();
} //class_has_non_default_finalizer



Class_Handle class_get_super_class(Class_Handle cl)
{
    assert(cl);
    return cl->get_super_class();
} //class_get_super_class

unsigned class_number_implements(Class_Handle ch)
{
    assert(ch);
    return ch->get_number_of_superinterfaces();
}

Class_Handle class_get_implements(Class_Handle ch, unsigned idx)
{
    assert(ch);
    return ch->get_superinterface(idx);
}


Class_Handle class_get_declaring_class(Class_Handle ch)
{
    return ch->resolve_declaring_class(VM_Global_State::loader_env);
}

unsigned class_number_inner_classes(Class_Handle ch)
{
    assert(ch);
    return ch->get_number_of_inner_classes();
}

Boolean class_is_inner_class_public(Class_Handle ch, unsigned idx)
{
    assert(ch);
    return (ch->get_inner_class_access_flags(idx) & ACC_PUBLIC) != 0;
}

Class_Handle class_get_inner_class(Class_Handle ch, unsigned idx)
{
    assert(ch);
    uint16 cp_index = ch->get_inner_class_index(idx);
    return ch->_resolve_class(VM_Global_State::loader_env, cp_index);
}

Boolean class_is_instanceof(Class_Handle s, Class_Handle t)
{
    assert(s);
    assert(t);
    return class_is_subtype(s, t);
} //class_get_super_class



Class_Handle class_get_array_element_class(Class_Handle cl)
{
    assert(cl);
    return cl->get_array_element_class();
} //class_get_array_element_class



Boolean class_hint_is_exceptiontype(Class_Handle ch)
{
    assert(ch);
    Global_Env *env = VM_Global_State::loader_env;
    Class *exc_base_clss = env->java_lang_Throwable_Class;
    while(ch != NULL) {
        if(ch == exc_base_clss) {
            return TRUE;
        }
        //sundr printf("class name before super: %s %p\n", ch->name->bytes, ch);
        ch = class_get_super_class(ch);
    }
    return FALSE;
} //class_hint_is_exceptiontype




/*
 * Procedure class_get_class_of_primitive_type
 *      Given an VM_Data_Type that describes a primitive type,
 *      returns the corresponding preloaded Class for that type.
 *
 * Side effects:
 *      None
 */
Class_Handle class_get_class_of_primitive_type(VM_Data_Type typ)
{
    Global_Env *env = VM_Global_State::loader_env;
    Class *clss = NULL;
    switch(typ) {
    case VM_DATA_TYPE_INT8:
        clss = env->Byte_Class;
        break;
    case VM_DATA_TYPE_INT16:
        clss = env->Short_Class;
        break;
    case VM_DATA_TYPE_INT32:
        clss = env->Int_Class;
        break;
    case VM_DATA_TYPE_INT64:
        clss = env->Long_Class;
        break;
    case VM_DATA_TYPE_F8:
        clss = env->Double_Class;
        break;
    case VM_DATA_TYPE_F4:
        clss = env->Float_Class;
        break;
    case VM_DATA_TYPE_BOOLEAN:
        clss = env->Boolean_Class;
        break;
    case VM_DATA_TYPE_CHAR:
        clss = env->Char_Class;
        break;
    case VM_DATA_TYPE_VOID:
        clss = env->Void_Class;
        break;
    case VM_DATA_TYPE_INTPTR:
    case VM_DATA_TYPE_UINTPTR:
    case VM_DATA_TYPE_UINT8:
    case VM_DATA_TYPE_UINT16:
    case VM_DATA_TYPE_UINT32:
    case VM_DATA_TYPE_UINT64:
        clss = NULL;    // ts07.09.02 - to allow star jit initialization
        break;
    default:
        ABORT("Unknown vm data type");          // We need a better way to indicate an internal error
    }
    return clss;
} //class_get_class_of_primitive_type

VTable_Handle class_get_vtable(Class_Handle cl)
{
    assert(cl);
    return cl->get_vtable();
} //class_get_vtable


const char* class_get_source_file_name(Class_Handle cl)
{
    assert(cl);
    if(!cl->has_source_information())
        return NULL;
    return cl->get_source_file_name();
} // class_get_source_file_name


const char* class_get_const_string(Class_Handle cl, unsigned index)
{
    assert(cl);
    return cl->get_constant_pool().get_string_chars(index);
} //class_get_const_string



// Returns the address where the interned version of the string is stored: this will be the address
// of a slot containing a Java_java_lang_String* or a uint32 compressed reference. Also interns the
// string so that the JIT can load a reference to the interned string without checking if it is null.
void *class_get_const_string_intern_addr(Class_Handle cl, unsigned index)
{
    assert(cl);
    Global_Env* env = VM_Global_State::loader_env;
    String* str = cl->get_constant_pool().get_string(index);
    assert(str);

    bool must_instantiate;
    if (env->compress_references) {
        must_instantiate = (str->intern.compressed_ref == 0 /*NULL*/);
    } else {
        must_instantiate = (str->intern.raw_ref == NULL);
    }

    if (must_instantiate) {
        // vm_instantiate_cp_string_resolved assumes that GC is disabled
        tmn_suspend_disable();
        // Discard the result. We are only interested in the side-effect of setting str->intern.
        BEGIN_RAISE_AREA;
        vm_instantiate_cp_string_resolved(str);
        END_RAISE_AREA;
        tmn_suspend_enable();
    }

    if (env->compress_references) {
        return &(str->intern.compressed_ref);
    } else {
        return &(str->intern.raw_ref);
    }
} //class_get_const_string_intern_addr


const char* class_get_cp_entry_signature(Class_Handle src_class, unsigned short index)
{
    Class* clss = (Class*)src_class;
    ConstantPool& cp = src_class->get_constant_pool();

    assert(cp.is_fieldref(index)
        || cp.is_methodref(index)
        || cp.is_interfacemethodref(index));

    index = cp.get_ref_name_and_type_index(index);
    index = cp.get_name_and_type_descriptor_index(index);
    return cp.get_utf8_chars(index);
} // class_get_cp_entry_signature


VM_Data_Type class_get_cp_field_type(Class_Handle src_class, unsigned short cp_index)
{
    assert(src_class->get_constant_pool().is_fieldref(cp_index));

    char class_id = (class_get_cp_entry_signature(src_class, cp_index))[0];
    switch(class_id)
    {
    case VM_DATA_TYPE_BOOLEAN:
        return VM_DATA_TYPE_BOOLEAN;
    case VM_DATA_TYPE_CHAR:
        return VM_DATA_TYPE_CHAR;
    case VM_DATA_TYPE_INT8:
        return VM_DATA_TYPE_INT8;
    case VM_DATA_TYPE_INT16:
        return VM_DATA_TYPE_INT16;
    case VM_DATA_TYPE_INT32:
        return VM_DATA_TYPE_INT32;
    case VM_DATA_TYPE_INT64:
        return VM_DATA_TYPE_INT64;
    case VM_DATA_TYPE_F4:
        return VM_DATA_TYPE_F4;
    case VM_DATA_TYPE_F8:
        return VM_DATA_TYPE_F8;
    case VM_DATA_TYPE_ARRAY:
    case VM_DATA_TYPE_CLASS:
        return VM_DATA_TYPE_CLASS;
    default:
        ABORT("Unknown vm data type");
    }
    return VM_DATA_TYPE_INVALID;
} // class_get_cp_field_type


VM_Data_Type class_get_const_type(Class_Handle cl, unsigned index)
{
    assert(cl);
    Java_Type jt = JAVA_TYPE_INVALID;
    ConstantPool& cp = cl->get_constant_pool();
    switch(cp.get_tag(index)) {
    case CONSTANT_String:
        jt = JAVA_TYPE_STRING;
        break;
    case CONSTANT_Integer:
        jt = JAVA_TYPE_INT;
        break;
    case CONSTANT_Float:
        jt = JAVA_TYPE_FLOAT;
        break;
    case CONSTANT_Long:
        jt = JAVA_TYPE_LONG;
        break;
    case CONSTANT_Double:
        jt = JAVA_TYPE_DOUBLE;
        break;
    case CONSTANT_Class:
        jt = JAVA_TYPE_CLASS;
        break;
    case CONSTANT_UnusedEntry:
        if(cp.get_tag(index - 1) == CONSTANT_Double) {
            jt = JAVA_TYPE_DOUBLE;
            break;
        } else if(cp.get_tag(index - 1) == CONSTANT_Long) {
            jt = JAVA_TYPE_LONG;
            break;
        }
    default:
        LDIE(5, "non-constant type is requested from constant pool : {0}" << cp.get_tag(index));
    }

    return (VM_Data_Type)jt;
} //class_get_const_type



const void *class_get_const_addr(Class_Handle cl, unsigned index)
{
    assert(cl);
    return cl->get_constant_pool().get_address_of_constant(index);
} //class_get_const_addr



Arg_List_Iterator method_get_argument_list(Method_Handle m)
{
    assert(m);
    return (Arg_List_Iterator)((Method *)m)->get_argument_list();
}






Class_Handle
vm_resolve_class(Compile_Handle h,
                  Class_Handle c,
                 unsigned index)
{
    assert(c);
    return (Class*) resolve_class(h, c, index);
} //vm_resolve_class



Class_Handle
vm_resolve_class_new(Compile_Handle h,
                     Class_Handle c,
                     unsigned index)
{
    assert(c);
    return resolve_class_new(h, c, index);
} //vm_resolve_class_new



Class_Handle resolve_class_from_constant_pool(Class_Handle c_handle, unsigned index)
{
    assert(c_handle);
    return c_handle->_resolve_class(VM_Global_State::loader_env, index);
}



ClassLoaderHandle
class_get_class_loader(Class_Handle ch)
{
    assert(ch);
    return ch->get_class_loader();
} //class_get_class_loader



Class_Handle
class_load_class_by_name(const char *name,
                         Class_Handle ch)
{
    assert(ch);
    Global_Env *env = VM_Global_State::loader_env;
    String *n = env->string_pool.lookup(name);
    return ch->get_class_loader()->LoadClass(env, n);
} //class_load_class_by_name



Class_Handle
class_load_class_by_name_using_bootstrap_class_loader(const char *name)
{
    Global_Env *env = VM_Global_State::loader_env;
    String *n = env->string_pool.lookup(name);
    Class *clss = env->bootstrap_class_loader->LoadVerifyAndPrepareClass(env, n);
    return (Class_Handle)clss;
} 



Class_Handle
class_load_class_by_descriptor(const char *descr,
                               Class_Handle ch)
{
    assert(ch);
    Global_Env *env = VM_Global_State::loader_env;
    String *n;
    switch(descr[0]) {
    case '[':
        n = env->string_pool.lookup(descr);
        break;
    case 'L':
        {
            int len = (int) strlen(descr);
            n = env->string_pool.lookup(descr + 1, len - 2);
        }
        break;
    case 'B':
        return env->Byte_Class;
    case 'C':
        return env->Char_Class;
    case 'D':
        return env->Double_Class;
    case 'F':
        return env->Float_Class;
    case 'I':
        return env->Int_Class;
    case 'J':
        return env->Long_Class;
    case 'S':
        return env->Short_Class;
    case 'Z':
        return env->Boolean_Class;
    default:
        n = 0;
    }
    assert(n);
    return ch->get_class_loader()->LoadClass(env, n);;
} //class_load_class_by_descriptor


Class_Handle class_find_loaded(ClassLoaderHandle loader, const char* name)
{
    char* name3 = strdup(name);
    char* p = name3;
    while (*p) {
        if (*p=='.') *p='/';
        p++;
    }
    Global_Env* env = VM_Global_State::loader_env;
    String* name2 = env->string_pool.lookup(name3);
    Class* ch;
    if (loader) {
        ch = loader->LookupClass(name2);
    } else {
        ch = env->bootstrap_class_loader->LookupClass(name2);
    }
    STD_FREE(name3);
    if(ch && (!ch->verify(env) || !ch->prepare(env))) return NULL;
    return ch;
}

Class_Handle class_find_class_from_loader(ClassLoaderHandle loader, const char* n, Boolean init)
{
    ASSERT_RAISE_AREA;
    assert(hythread_is_suspend_enabled()); // -salikh
    char *new_name = strdup(n);
    char *p = new_name;
    while (*p) {
        if (*p == '.') *p = '/';
        p++;
    }
    String* name = VM_Global_State::loader_env->string_pool.lookup(new_name);
    STD_FREE(new_name);
    Class* ch;
    if (loader) {
        ch = class_load_verify_prepare_by_loader_jni(
            VM_Global_State::loader_env, name, loader);
    } else {
        assert(hythread_is_suspend_enabled());
        ch = class_load_verify_prepare_from_jni(VM_Global_State::loader_env, name);
    }
    if (!ch) return NULL;
    // All initialization from jni should not propagate exceptions and
    // should return to calling native method.
    if(init) {
        class_initialize_from_jni(ch);

        if (exn_raised()) {
            return NULL;
        }
    }

    if(exn_raised()) {
        return 0;
    }

    return ch;
}


//
// The following do not cause constant pools to be resolve, if they are not
// resolved already
//
const char* const_pool_get_field_name(Class_Handle cl, unsigned index)
{
    assert(cl);
    ConstantPool& const_pool = cl->get_constant_pool();
    if(!const_pool.is_fieldref(index)) {
        ABORT("Wrong index");
        return 0;
    }
    index = const_pool.get_ref_name_and_type_index(index);
    index = const_pool.get_name_and_type_name_index(index);
    return const_pool.get_utf8_chars(index);
} // const_pool_get_field_name



const char* const_pool_get_field_class_name(Class_Handle cl, unsigned index)
{
    assert(cl);
    ConstantPool& const_pool = cl->get_constant_pool();
    if(!const_pool.is_fieldref(index)) {
        ABORT("Wrong index");
        return 0;
    }
    index = const_pool.get_ref_class_index(index);
    return const_pool_get_class_name(cl, index);
} //const_pool_get_field_class_name



const char* const_pool_get_field_descriptor(Class_Handle cl, unsigned index)
{
    assert(cl);
    ConstantPool& const_pool = cl->get_constant_pool();
    if (!const_pool.is_fieldref(index)) {
        ABORT("Wrong index");
        return 0;
    }
    index = const_pool.get_ref_name_and_type_index(index);
    index = const_pool.get_name_and_type_descriptor_index(index);
    return const_pool.get_utf8_chars(index);
} // const_pool_get_field_descriptor



const char* const_pool_get_method_name(Class_Handle cl, unsigned index)
{
    assert(cl);
    ConstantPool& const_pool = cl->get_constant_pool();
    if (!const_pool.is_methodref(index)) {
        ABORT("Wrong index");
        return 0;
    }
    index = const_pool.get_ref_name_and_type_index(index);
    index = const_pool.get_name_and_type_name_index(index);
    return const_pool.get_utf8_chars(index);
} // const_pool_get_method_name



const char *const_pool_get_method_class_name(Class_Handle cl, unsigned index)
{
    assert(cl);
    ConstantPool& const_pool = cl->get_constant_pool();
    if (!const_pool.is_methodref(index)) {
        ABORT("Wrong index");
        return 0;
    }
    index = const_pool.get_ref_class_index(index);
    return const_pool_get_class_name(cl,index);
} //const_pool_get_method_class_name



const char* const_pool_get_interface_method_name(Class_Handle cl, unsigned index)
{
    assert(cl);
    ConstantPool& const_pool = cl->get_constant_pool();
    if(!const_pool.is_interfacemethodref(index)) {
        ABORT("Wrong index");
        return 0;
    }
    index = const_pool.get_ref_name_and_type_index(index);
    index = const_pool.get_name_and_type_name_index(index);
    return const_pool.get_utf8_chars(index);
} // const_pool_get_interface_method_name



const char* const_pool_get_interface_method_class_name(Class_Handle cl, unsigned index)
{
    assert(cl);
    ConstantPool& const_pool = cl->get_constant_pool();
    if (!const_pool.is_interfacemethodref(index)) {
        ABORT("Wrong index");
        return 0;
    }
    index = const_pool.get_ref_class_index(index);
    return const_pool_get_class_name(cl,index);
} //const_pool_get_interface_method_class_name



const char* const_pool_get_method_descriptor(Class_Handle cl, unsigned index)
{
    assert(cl);
    ConstantPool& const_pool = cl->get_constant_pool();
    if (!const_pool.is_methodref(index)) {
        ABORT("Wrong index");
        return 0;
    }
    index = const_pool.get_ref_name_and_type_index(index);
    index = const_pool.get_name_and_type_descriptor_index(index);
    return const_pool.get_utf8_chars(index);
} // const_pool_get_method_descriptor



const char* const_pool_get_interface_method_descriptor(Class_Handle cl, unsigned index)
{
    assert(cl);
    ConstantPool& const_pool = cl->get_constant_pool();
    if (!const_pool.is_interfacemethodref(index)) {
        ABORT("Wrong index");
        return 0;
    }
    index = const_pool.get_ref_name_and_type_index(index);
    index = const_pool.get_name_and_type_descriptor_index(index);
    return const_pool.get_utf8_chars(index);
} // const_pool_get_interface_method_descriptor



const char* const_pool_get_class_name(Class_Handle cl, unsigned index)
{
    assert(cl);
    ConstantPool& const_pool = cl->get_constant_pool();
    if (!const_pool.is_class(index)) {
        ABORT("Wrong index");
        return 0;
    }
    return const_pool.get_utf8_chars(const_pool.get_class_name_index(index));
} // const_pool_get_class_name


unsigned method_number_throws(Method_Handle m)
{
    assert(m);
    return ((Method*)m)->num_exceptions_method_can_throw();
}

Class_Handle method_get_throws(Method_Handle mh, unsigned idx)
{
    assert(hythread_is_suspend_enabled());
    assert(mh);
    Method* m = (Method*)mh;
    String* exn_name = m->get_exception_name(idx);
    if (!exn_name) return NULL;
    ClassLoader* class_loader = m->get_class()->get_class_loader();
    Class *c =
        class_loader->LoadVerifyAndPrepareClass(VM_Global_State::loader_env, exn_name);
    // if loading failed - exception should be raised
    assert((!c && exn_raised()) || (c && !exn_raised()));
    return c;
}

unsigned method_get_num_handlers(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->num_bc_exception_handlers();
} //method_get_num_handlers



void method_get_handler_info(Method_Handle m,
                                    unsigned handler_id,
                                    unsigned *begin_offset,
                                    unsigned *end_offset,
                                    unsigned *handler_offset,
                                    unsigned *handler_cpindex)
{
    assert(m);
    Handler *h = ((Method *)m)->get_bc_exception_handler_info(handler_id);
    *begin_offset    = h->get_start_pc();
    *end_offset      = h->get_end_pc();
    *handler_offset  = h->get_handler_pc();
    *handler_cpindex = h->get_catch_type_index();
} //method_get_handler_info



void method_set_num_target_handlers(Method_Handle m,
                                    JIT_Handle j,
                                    unsigned num_handlers)
{
    assert(m);
    ((Method *)m)->set_num_target_exception_handlers((JIT *)j, num_handlers);
} //method_set_num_target_handlers



void method_set_target_handler_info(Method_Handle m,
                                    JIT_Handle    j,
                                    unsigned      eh_number,
                                    void         *start_ip,
                                    void         *end_ip,
                                    void         *handler_ip,
                                    Class_Handle  catch_cl,
                                    Boolean       exc_obj_is_dead)
{
    assert(m);
    ((Method *)m)->set_target_exception_handler_info((JIT *)j,
                                                     eh_number,
                                                     start_ip,
                                                     end_ip,
                                                     handler_ip,
                                                     (Class *)catch_cl,
                                                     (exc_obj_is_dead == TRUE));
} //method_set_target_handler_info

Method_Side_Effects method_get_side_effects(Method_Handle m)
{
    assert(m);
    return ((Method *)m)->get_side_effects();
} //method_get_side_effects



void method_set_side_effects(Method_Handle m, Method_Side_Effects mse)
{
    assert(m);
    ((Method *)m)->set_side_effects(mse);
} //method_set_side_effects



Method_Handle class_lookup_method_recursively(Class_Handle clss,
                                              const char *name,
                                              const char *descr)
{
    assert(clss);
    return (Method_Handle) class_lookup_method_recursive((Class *)clss, name, descr);
} //class_lookup_method_recursively


struct ChList {
    JIT_Handle          jit;
    Compilation_Handle  ch;
    ChList*             next;
};

static ChList* chs = NULL;

Compile_Handle jit_get_comp_handle(JIT_Handle j)
{
    for (ChList *c = chs;  c != NULL;  c = c->next) {
        if (c->jit == j) {
            return (Compile_Handle)(&c->ch);
        }
    }
    ChList* n = (ChList*)STD_MALLOC(sizeof(ChList));
    assert(n);
    n->jit = j;
    n->ch.env = VM_Global_State::loader_env;
    n->ch.jit = (JIT*)j;
    n->next = chs;
    chs = n;
    return &n->ch;
} //jit_get_comp_handle



int object_get_vtable_offset()
{
    return 0;
} //object_get_vtable_offset

////////////////////////////////////////////////////////////
/// 20020220 New stuff for the interface


////////////////////////////////////////////////////////////
// begin declarations


// Returns the number of local variables defined for the method.
VMEXPORT unsigned method_vars_get_number(Method_Handle mh);

VMEXPORT Class_Handle class_get_class_of_primitive_type(VM_Data_Type ch);

////////////////////////////////////////////////////////////
// begin definitions



Class_Handle get_system_object_class()
{
    Global_Env *env = VM_Global_State::loader_env;
    return env->JavaLangObject_Class;
} //get_system_object_class



Class_Handle get_system_class_class()
{
    Global_Env *env = VM_Global_State::loader_env;
    return env->JavaLangClass_Class;
} //get_system_class_class



Class_Handle get_system_string_class()
{
    Global_Env *env = VM_Global_State::loader_env;
    return env->JavaLangString_Class;
} //get_system_string_class



Boolean field_is_literal(Field_Handle fh)
{
    assert(fh);
    Field *f = (Field *)fh;
    return f->get_const_value_index() != 0;
} //field_is_literal



Boolean class_is_before_field_init(Class_Handle ch)
{
    assert(ch);
    return FALSE;
} //class_is_before_field_init




int vector_first_element_offset_unboxed(Class_Handle element_type)
{
    assert(element_type);
    Class *clss = (Class *)element_type;
    int offset = 0;
    if(clss->is_primitive()) {
        Global_Env *env = VM_Global_State::loader_env;
        if(clss == env->Double_Class) {
            offset = VM_VECTOR_FIRST_ELEM_OFFSET_8;
        } else if(clss == env->Long_Class) {
            offset = VM_VECTOR_FIRST_ELEM_OFFSET_8;
        } else {
            offset = VM_VECTOR_FIRST_ELEM_OFFSET_1_2_4;
        }
    } else {
        offset = VM_VECTOR_FIRST_ELEM_OFFSET_REF;
    }
    assert(offset);
    return offset;
} //vector_first_element_offset_unboxed


int array_first_element_offset_unboxed(Class_Handle element_type)
{
    assert(element_type);
    return vector_first_element_offset_unboxed(element_type);
} //array_first_element_offset_unboxed



int vector_first_element_offset(VM_Data_Type element_type)
{
    switch(element_type) {
    case VM_DATA_TYPE_CLASS:
        return VM_VECTOR_FIRST_ELEM_OFFSET_REF;
    default:
        {
            Class_Handle elem_class = class_get_class_of_primitive_type(element_type);
            return vector_first_element_offset_unboxed(elem_class);
        }
    }
} //vector_first_element_offset




int vector_first_element_offset_class_handle(Class_Handle UNREF element_type)
{
    return VM_VECTOR_FIRST_ELEM_OFFSET_REF;
} //vector_first_element_offset_class_handle

int vector_first_element_offset_vtable_handle(VTable_Handle UNREF element_type)
{
    return VM_VECTOR_FIRST_ELEM_OFFSET_REF;
} //vector_first_element_offset_class_handle


Boolean method_is_java(Method_Handle mh)
{
    assert(mh);
    return TRUE;
} //method_is_java




// 20020220 TO DO:
// 1. Add the is_value_type field to the Java build of VM?
Boolean class_is_valuetype(Class_Handle ch)
{
    assert(ch);
    return class_is_primitive(ch);
} //class_is_valuetype


#ifdef COMPACT_FIELD
Boolean class_is_compact_field()
{
    return Class::compact_fields && Class::sort_fields ;
}
#endif

Boolean class_is_enum(Class_Handle ch)
{
    assert(ch);
    return ch->is_enum() ? TRUE : FALSE;
} //class_is_enum




static Class *class_get_array_of_primitive_type(VM_Data_Type typ)
{
    Global_Env *env = VM_Global_State::loader_env;
    Class *clss = NULL;
    switch(typ) {
    case VM_DATA_TYPE_INT8:
        clss = env->ArrayOfByte_Class;
        break;
    case VM_DATA_TYPE_INT16:
        clss = env->ArrayOfShort_Class;
        break;
    case VM_DATA_TYPE_INT32:
        clss = env->ArrayOfInt_Class;
        break;
    case VM_DATA_TYPE_INT64:
        clss = env->ArrayOfLong_Class;
        break;
    case VM_DATA_TYPE_F8:
        clss = env->ArrayOfDouble_Class;
        break;
    case VM_DATA_TYPE_F4:
        clss = env->ArrayOfFloat_Class;
        break;
    case VM_DATA_TYPE_BOOLEAN:
        clss = env->ArrayOfBoolean_Class;
        break;
    case VM_DATA_TYPE_CHAR:
        clss = env->ArrayOfChar_Class;
        break;
    case VM_DATA_TYPE_UINT8:
    case VM_DATA_TYPE_UINT16:
    case VM_DATA_TYPE_UINT32:
    case VM_DATA_TYPE_UINT64:
    case VM_DATA_TYPE_INTPTR:
    case VM_DATA_TYPE_UINTPTR:
    default:
        ABORT("Unexpected vm data type");          // We need a better way to indicate an internal error
        break;
    }
    return clss;
} //class_get_array_of_primitive_type



Class_Handle class_get_array_of_unboxed(Class_Handle ch)
{
    assert(ch);
    if(class_is_primitive(ch)) {
        VM_Data_Type typ = class_get_primitive_type_of_class(ch);
        return class_get_array_of_primitive_type(typ);
    } else {
        // 20020319 This should never happen for Java.
        ABORT("The given class handle does not represent a primitive type");
        return 0;
    }
} //class_get_array_of_unboxed




Boolean field_is_unmanaged_static(Field_Handle fh)
{
    assert(fh);
    return FALSE;
} //field_is_unmanaged_static



Boolean class_is_primitive(Class_Handle ch)
{
    assert(ch);
    return ch->is_primitive();
} //class_is_primitive



VM_Data_Type class_get_primitive_type_java(Class_Handle clss)
{
    assert(clss);
    Global_Env *env = VM_Global_State::loader_env;
    if (clss == env->Boolean_Class)
        return VM_DATA_TYPE_BOOLEAN;
    if (clss == env->Char_Class)
        return VM_DATA_TYPE_CHAR;
    if (clss == env->Byte_Class)
        return VM_DATA_TYPE_INT8;
    if (clss == env->Short_Class)
        return VM_DATA_TYPE_INT16;
    if (clss == env->Int_Class)
        return VM_DATA_TYPE_INT32;
    if (clss == env->Long_Class)
        return VM_DATA_TYPE_INT64;
    if (clss == env->Float_Class)
        return VM_DATA_TYPE_F4;
    if (clss == env->Double_Class)
        return VM_DATA_TYPE_F8;

    return VM_DATA_TYPE_CLASS;
} //class_get_primitive_type_java





VM_Data_Type class_get_primitive_type_of_class(Class_Handle ch)
{
    assert(ch);
    return class_get_primitive_type_java(ch);
} //class_get_primitive_type_of_class


unsigned method_vars_get_number(Method_Handle mh)
{
    assert(mh);
    Method *m = (Method *)mh;
    return m->get_max_locals();
} //method_vars_get_number



// Returns the number of arguments defined for the method.
// This number includes the this pointer (if present).
unsigned method_args_get_number(Method_Signature_Handle mh)
{
    assert(mh);
    Method_Signature *ms = (Method_Signature *)mh;
    assert(!ms->sig);
    Method *m = ms->method;
    assert(m);
    return m->get_num_args();
} //method_args_get_number


// 20020303 At the moment we resolve the return type eagerly, but
// arguments are resolved lazily.  We should rewrite return type resolution
// to be lazy too.
void Method_Signature::initialize_from_java_method(Method *meth)
{
    assert(meth);
    num_args = meth->get_num_args();

    ClassLoader* cl = meth->get_class()->get_class_loader();
    const char* d = meth->get_descriptor()->bytes;
    assert(d[0]=='(');
    d++;
    arg_type_descs = (TypeDesc**)STD_MALLOC(sizeof(TypeDesc*)*num_args);
    assert(arg_type_descs);
    unsigned cur = 0;
    if (!meth->is_static()) {
        arg_type_descs[cur] = type_desc_create_from_java_class(meth->get_class());
        assert(arg_type_descs[cur]);
        cur++;
    }
    while (d[0]!=')') {
        assert(cur<num_args);
        arg_type_descs[cur] = type_desc_create_from_java_descriptor(d, cl);
        assert(arg_type_descs[cur]);
        cur++;
        // Advance d to next argument
        while (d[0]=='[') d++;
        d++;
        if (d[-1]=='L') {
            while(d[0]!=';') d++;
            d++;
        }
    }
    assert(cur==num_args);
    d++;
    return_type_desc = type_desc_create_from_java_descriptor(d, cl);
    assert(return_type_desc);
} //Method_Signature::initialize_from_java_method



void Method_Signature::reset()
{
    return_type_desc = 0;
    if (arg_type_descs)
        STD_FREE(arg_type_descs);

    num_args         = 0;
    arg_type_descs   = 0;
    method           = 0;
    sig              = 0;
} //Method_Signature::reset

void Method_Signature::initialize_from_method(Method *meth)
{
    assert(meth);
    reset();
    method      = meth;
    initialize_from_java_method(meth);
} //Method_Signature::initialize_from_method



Method_Signature_Handle method_get_signature(Method_Handle mh)
{
    assert(mh);
    Method *m = (Method *)mh;
    Method_Signature *ms = m->get_method_sig();
    if(!ms) {
        ms = new Method_Signature();
        ms->initialize_from_method(m);
        m->set_method_sig(ms);
    }
    return ms;
} //method_get_signature




Boolean method_args_has_this(Method_Signature_Handle msh)
{
    assert(msh);
    Method_Signature *ms = (Method_Signature *)msh;
    return !ms->method->is_static();
} //method_args_has_this


unsigned class_number_fields(Class_Handle ch)
{
    assert(ch);
    return ch->get_number_of_fields();
}

unsigned class_num_instance_fields(Class_Handle ch)
{
    assert(ch);
    return ch->get_number_of_fields() - ch->get_number_of_static_fields();
} //class_num_instance_fields



unsigned class_num_instance_fields_recursive(Class_Handle ch)
{
    assert(ch);
    unsigned num_inst_fields = 0;
    while(ch) {
        num_inst_fields += class_num_instance_fields(ch);
        ch = class_get_super_class(ch);
    }
    return num_inst_fields;
} // class_num_instance_fields_recursive


Field_Handle class_get_field(Class_Handle ch, unsigned idx)
{
    assert(ch);
    if(idx >= ch->get_number_of_fields()) return NULL;
    return ch->get_field(idx);
} // class_get_field

Field_Handle class_get_instance_field(Class_Handle ch, unsigned idx)
{
    assert(ch);
    return ch->get_field(ch->get_number_of_static_fields() + idx);
} // class_get_instance_field



Field_Handle class_get_instance_field_recursive(Class_Handle ch, unsigned idx)
{
    assert(ch);
    unsigned num_fields_recursive = class_num_instance_fields_recursive(ch);
    assert(idx < num_fields_recursive);
    while(ch) {
        unsigned num_fields = class_num_instance_fields(ch);
        unsigned num_inherited_fields = num_fields_recursive - num_fields;
        if(idx >= num_inherited_fields) {
            Field_Handle fh = class_get_instance_field(ch, idx - num_inherited_fields);
            return fh;
        }
        num_fields_recursive = num_inherited_fields;
        ch = class_get_super_class(ch);
    }
    return 0;
} // class_get_instance_field_recursive


unsigned class_get_number_methods(Class_Handle ch)
{
    assert(ch);
    return ch->get_number_of_methods();
} // class_get_number_methods


Method_Handle class_get_method(Class_Handle ch, unsigned index)
{
    assert(ch);
    if(index >= ch->get_number_of_methods())
        return NULL;
    return ch->get_method(index);
} // class_get_method


// -gc magic needs this to do the recursive load.
Class_Handle field_get_class_of_field_value(Field_Handle fh)
{
    assert(hythread_is_suspend_enabled());
    assert(fh);
    Class_Handle ch = class_load_class_by_descriptor(field_get_descriptor(fh),
                                          field_get_class(fh));
    if(!ch->verify(VM_Global_State::loader_env))
        return NULL;
    if(!ch->prepare(VM_Global_State::loader_env))
        return NULL;
    return ch;
} //field_get_class_of_field_value


Boolean field_is_reference(Field_Handle fh)
{
    assert((Field *)fh);
    Java_Type typ = field_get_type(fh);
    return (typ == JAVA_TYPE_CLASS || typ == JAVA_TYPE_ARRAY);
} //field_is_reference

Boolean field_is_magic(Field_Handle fh)
{
    assert((Field *)fh);
    
    return fh->is_magic_type();
} //field_is_magic


Boolean field_is_enumerable_reference(Field_Handle fh)
{
    assert((Field *)fh);
    return ((field_is_reference(fh) && !field_is_magic(fh)));
} //field_is_enumerable_reference


Boolean field_is_injected(Field_Handle f)
{
    assert(f);
    return ((Field*)f)->is_injected();
} //field_is_injected


/////////////////////////////////////////////////////////////////////
// New GC interface demo

// This is just for the purposes of the demo.  The GC is free to define
// whatever it wants in this structure, as long as it doesn't exceed the
// size limit.  What should the size limit be?

struct GC_VTable {
    Class_Handle ch;  // for debugging
    uint32 num_ref_fields;
    uint32 *ref_fields_offsets;
    unsigned is_array : 1;
    unsigned is_primitive : 1;
};


#define VERBOSE_GC_CLASS_PREPARED 0

/////////////////////////////////////////////////////////////////////
//  New signature stuff




Type_Info_Handle method_vars_get_type_info(Method_Handle mh,
                                           unsigned UNREF idx)
{
    assert(mh);
    // Always NULL for Java.
    return 0;
} //method_vars_get_type_info



Type_Info_Handle method_args_get_type_info(Method_Signature_Handle msh,
                                           unsigned idx)
{
    assert(msh);
    Method_Signature *ms = (Method_Signature *)msh;
    if(idx >= ms->num_args) {
        ABORT("Wrong index");
        return 0;
    }
    assert(ms->arg_type_descs);
    return ms->arg_type_descs[idx];
} //method_args_get_type_info



Type_Info_Handle method_ret_type_get_type_info(Method_Signature_Handle msh)
{
    assert(msh);
    Method_Signature *ms = (Method_Signature *)msh;
    return ms->return_type_desc;
} //method_ret_type_get_type_info



Boolean type_info_is_managed_pointer(Type_Info_Handle tih)
{
    assert(tih);
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->is_managed_pointer();
} //type_info_is_managed_pointer


Boolean method_vars_is_managed_pointer(Method_Handle mh, unsigned idx)
{
    assert(mh);
    Type_Info_Handle tih = method_vars_get_type_info(mh, idx);
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->is_managed_pointer();
} //method_vars_is_managed_pointer


Boolean method_vars_is_pinned(Method_Handle mh, unsigned UNREF idx)
{
    assert(mh);
    // 20030626 This is wrong, but equivalent to the existing code when I wrote this.
    return FALSE;
} //method_vars_is_pinned


Boolean method_args_is_managed_pointer(Method_Signature_Handle msh, unsigned idx)
{
    assert(msh);
    Type_Info_Handle tih = method_args_get_type_info(msh, idx);
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->is_managed_pointer();
} //method_args_is_managed_pointer



Boolean method_ret_type_is_managed_pointer(Method_Signature_Handle msh)
{
    assert(msh);
    Type_Info_Handle tih = method_ret_type_get_type_info(msh);
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->is_managed_pointer();
} //method_ret_type_is_managed_pointer



VM_Data_Type type_info_get_type(Type_Info_Handle tih)
{
    assert(tih);
    TypeDesc* td = (TypeDesc*)tih;
    switch (td->get_kind()) {
    case K_S1:               return VM_DATA_TYPE_INT8;
    case K_S2:               return VM_DATA_TYPE_INT16;
    case K_S4:               return VM_DATA_TYPE_INT32;
    case K_S8:               return VM_DATA_TYPE_INT64;
    case K_Sp:               return VM_DATA_TYPE_INTPTR;
    case K_U1:               return VM_DATA_TYPE_UINT8;
    case K_U2:               return VM_DATA_TYPE_UINT16;
    case K_U4:               return VM_DATA_TYPE_UINT32;
    case K_U8:               return VM_DATA_TYPE_UINT64;
    case K_Up:               return VM_DATA_TYPE_UINTPTR;
    case K_F4:               return VM_DATA_TYPE_F4;
    case K_F8:               return VM_DATA_TYPE_F8;
    case K_Char:             return VM_DATA_TYPE_CHAR;
    case K_Boolean:          return VM_DATA_TYPE_BOOLEAN;
    case K_Void:             return VM_DATA_TYPE_VOID;
    case K_Object:           return VM_DATA_TYPE_CLASS;
    case K_Vector:           return VM_DATA_TYPE_ARRAY;
    case K_UnboxedValue:     return VM_DATA_TYPE_VALUE;
    case K_UnmanagedPointer: return VM_DATA_TYPE_UP;
    case K_ManagedPointer:   return VM_DATA_TYPE_MP;
    // The rest are not implemented in the VM_Data_Type scheme
    case K_Array:
    case K_MethodPointer:
    case K_TypedRef:
    default:
        ABORT("Invalid vm data type");
        return VM_DATA_TYPE_INVALID;
    }
} //type_info_get_type



Boolean type_info_is_reference(Type_Info_Handle tih)
{
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->get_kind()==K_Object;
} //type_info_is_reference



Boolean type_info_is_unboxed(Type_Info_Handle tih)
{
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->is_unboxed_value();
} //type_info_is_unboxed



Boolean type_info_is_unmanaged_pointer(Type_Info_Handle tih)
{
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->is_unmanaged_pointer();
} //type_info_is_unmanaged_pointer



Boolean type_info_is_void(Type_Info_Handle tih)
{
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->get_kind()==K_Void;
} //type_info_is_void



Boolean type_info_is_method_pointer(Type_Info_Handle tih)
{
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->is_method_pointer();
} //type_info_is_method_pointer



Boolean type_info_is_vector(Type_Info_Handle tih)
{
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->is_vector();
} //type_info_is_vector



Boolean type_info_is_general_array(Type_Info_Handle tih)
{
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->is_array() && !td->is_vector();
} //type_info_is_general_array



Boolean type_info_is_primitive(Type_Info_Handle tih)
{
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    return td->is_primitive();
} //type_info_is_primitive


Class_Handle type_info_get_class(Type_Info_Handle tih)
{
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    Class* c = td->load_type_desc();
    if(!c) return NULL;
    if(!c->verify(VM_Global_State::loader_env)) return NULL;
    if(!c->prepare(VM_Global_State::loader_env)) return NULL;
    return c;
} //type_info_get_class

Class_Handle type_info_get_class_no_exn(Type_Info_Handle tih)
{
    Class_Handle ch = type_info_get_class(tih);
    exn_clear();
    return ch;
} // type_info_get_class_no_exn

Method_Signature_Handle type_info_get_method_sig(Type_Info_Handle UNREF tih)
{
    ABORT("Not implemented");
    return 0;
} //type_info_get_method_sig



Type_Info_Handle type_info_get_type_info(Type_Info_Handle tih)
{
    TypeDesc* td = (TypeDesc*)tih;
    assert(td);
    switch (td->get_kind()) {
    case K_Vector:
    case K_Array:
        return td->get_element_type();
    case K_ManagedPointer:
    case K_UnmanagedPointer:
        return td->get_pointed_to_type();
    default:
        ABORT("Unexpected kind");
        return 0;
    }
} //type_info_get_type_info

void free_string_buffer(char *buffer)
{
    STD_FREE(buffer);
} //free_string_buffer


Type_Info_Handle field_get_type_info_of_field_value(Field_Handle fh)
{
    assert(fh);
    Field *field = (Field *)fh;
    TypeDesc* td = field->get_field_type_desc();
    assert(td);
    return td;
} //field_get_type_info_of_field_value



Type_Info_Handle class_get_element_type_info(Class_Handle ch)
{
    assert(ch);
    TypeDesc* td = ch->get_array_element_type_desc();
    assert(td);
    return td;
} //class_get_element_type_info



/////////////////////////////////////////////////////
// New GC stuff

Boolean class_is_non_ref_array(Class_Handle ch)
{
    assert(ch);
    // Use the if statement to normalize the value of TRUE
    if((ch->get_vtable()->class_properties & CL_PROP_NON_REF_ARRAY_MASK) != 0)
    {
        assert(ch->is_array());
        return TRUE;
    } else {
        return FALSE;
    }
} // class_is_non_ref_array


Boolean class_is_pinned(Class_Handle ch)
{
    assert(ch);
    return (ch->get_vtable()->class_properties & CL_PROP_PINNED_MASK) != 0
        ? TRUE : FALSE;
} // class_is_pinned


Boolean class_is_finalizable(Class_Handle ch)
{
    assert(ch);
    return (ch->get_vtable()->class_properties & CL_PROP_FINALIZABLE_MASK) != 0
        ? TRUE : FALSE;
} // class_is_finalizable

WeakReferenceType class_is_reference(Class_Handle clss)
{
    assert(clss);
    if (class_is_extending_class( (class_handler)clss, "java/lang/ref/WeakReference"))
        return WEAK_REFERENCE;
    else if (class_is_extending_class( (class_handler)clss, "java/lang/ref/SoftReference"))
        return SOFT_REFERENCE;
    else if (class_is_extending_class( (class_handler)clss, "java/lang/ref/PhantomReference"))
        return PHANTOM_REFERENCE;
    else
        return NOT_REFERENCE;
}

int class_get_referent_offset(Class_Handle ch)
{
    Field_Handle referent =
        class_lookup_field_recursive(ch, "referent", "Ljava/lang/Object;");
    if (!referent) {
        LDIE(6, "Class {0} has no 'Object referent' field" << class_get_name(ch));
    }
    return referent->get_offset();
}

void* class_alloc_via_classloader(Class_Handle ch, int32 size)
{
    assert(ch);
	assert(size >= 0);
    Class *clss = (Class *)ch;
    assert (clss->get_class_loader());
    return clss->get_class_loader()->Alloc(size);
} //class_alloc_via_classloader

unsigned class_get_alignment(Class_Handle ch)
{
    assert(ch);
    return (unsigned)(ch->get_vtable()->class_properties
        & CL_PROP_ALIGNMENT_MASK);
} //class_get_alignment



// (20020313) Should it always be the same as class_get_alignment?
unsigned class_get_alignment_unboxed(Class_Handle ch)
{
    assert(ch);
    return class_get_alignment(ch);
} //class_get_alignment_unboxed

//
// Returns the size of an element in the array class.
//
unsigned class_element_size(Class_Handle ch)
{
    assert(ch);
    return ch->get_array_element_size();
} //class_element_size



unsigned class_get_boxed_data_size(Class_Handle ch)
{
    assert(ch);
    return ch->get_allocated_size();
} //class_get_boxed_data_size



static struct {
    const char* c;
    const char* m;
    const char* d;
} no_inlining_table[] = {
    { "java/lang/ClassLoader", "getCallerClassLoader", "()Ljava/lang/ClassLoader;" },
    { "java/lang/Class", "forName", "(Ljava/lang/String;)Ljava/lang/Class;" },
};

static unsigned no_inlining_table_count = sizeof(no_inlining_table)/sizeof(no_inlining_table[0]);

Boolean method_is_no_inlining(Method_Handle mh)
{
    assert(mh);
    const char* c = class_get_name(method_get_class(mh));
    const char* m = method_get_name(mh);
    const char* d = method_get_descriptor(mh);
    for(unsigned i=0; i<no_inlining_table_count; i++)
        if (strcmp(c, no_inlining_table[i].c)==0 &&
            strcmp(m, no_inlining_table[i].m)==0 &&
            strcmp(d, no_inlining_table[i].d)==0)
            return TRUE;
    return FALSE;
} //method_is_no_inlining



Boolean method_is_require_security_object(Method_Handle mh)
{
    assert(mh);
    return FALSE;
} //method_is_require_security_object



#define QUAL_NAME_BUFF_SIZE 128

// Class ch is a subclass of method_get_class(mh).  The function returns a method handle
// for an accessible method overriding mh in ch or in its closest superclass that overrides mh.
// Class ch must be a class not an interface.
Method_Handle method_find_overridden_method(Class_Handle ch, Method_Handle mh)
{
    assert(ch);
    assert(mh);
    Method *method = (Method *)mh;
    assert(!ch->is_interface());   // ch cannot be an interface

    const String *name = method->get_name();
    const String *desc = method->get_descriptor();
    Method *m = NULL;
    for(; ch;  ch = ch->get_super_class()) {
        m = ch->lookup_method(name, desc);
        if (m != NULL) {
            // The method m can only override mh/method
            // if m's class can access mh/method (JLS 6.6.5).
            if(m->get_class()->can_access_member(method)) {
                break;
            }
        }
    }
    return m;
} //method_find_overridden_method



void core_free(void* p)
{
    STD_FREE(p);
}



int32 vector_get_length(Vector_Handle vector)
{
    assert(vector);
    // XXX- need some assert that "vector" is really an array type
    return get_vector_length(vector);
} //vector_get_length



Managed_Object_Handle *vector_get_element_address_ref(Vector_Handle vector, int32 idx)
{
    assert(vector);
    Managed_Object_Handle *elem = (Managed_Object_Handle *)get_vector_element_address_ref(vector, idx);
    return elem;
} //vector_get_element_address_ref



unsigned vm_vector_size(Class_Handle vector_class, int length)
{
    assert(vector_class);
    return vector_class->calculate_array_size(length);
} // vm_vector_size



enum safepoint_state get_global_safepoint_status()
{
    return global_safepoint_status;
} //get_global_safepoint_status



void vm_gc_lock_enum()
{
    hythread_global_lock();
} // vm_gc_lock_enum



void vm_gc_unlock_enum()
{
     hythread_global_unlock();
} // vm_gc_unlock_enum



void *vm_get_gc_thread_local()
{
    return (void *) &p_TLS_vmthread->_gc_private_information;
}


size_t vm_number_of_gc_bytes_in_vtable()
{
    return GC_BYTES_IN_VTABLE;
}


size_t vm_number_of_gc_bytes_in_thread_local()
{
    return GC_BYTES_IN_THREAD_LOCAL;
}


VMEXPORT Boolean vm_references_are_compressed()
{
    return VM_Global_State::loader_env->compress_references;
} //vm_references_are_compressed


VMEXPORT void *vm_heap_base_address()
{
    return (void*)VM_Global_State::loader_env->heap_base;
} //vm_heap_base_address


VMEXPORT void *vm_heap_ceiling_address()
{
    return (void *)VM_Global_State::loader_env->heap_end;
} //vm_heap_ceiling_address


Boolean vm_vtable_pointers_are_compressed()
{
    return ManagedObject::are_vtable_pointers_compressed();
} //vm_vtable_pointers_are_compressed


Class_Handle allocation_handle_get_class(Allocation_Handle ah)
{
    assert(ah);
    VTable *vt;

    if (vm_vtable_pointers_are_compressed())
    {
        vt = (VTable *) ((POINTER_SIZE_INT)ah + vm_get_vtable_base());
    }
    else
    {
        vt = (VTable *) ah;
    }
    return (Class_Handle) vt->clss;
}


Allocation_Handle class_get_allocation_handle(Class_Handle ch)
{
    assert(ch);
    return ch->get_allocation_handle();
}


unsigned vm_get_vtable_ptr_size()
{
    if(vm_vtable_pointers_are_compressed())
    {
        return sizeof(uint32);
    }
    else
    {
        return sizeof(POINTER_SIZE_INT);
    }
}

int vm_max_fast_instanceof_depth()
{
    return MAX_FAST_INSTOF_DEPTH;
}


////////////////////////////////////////////////////////////////////////////////////
// Direct call-related functions that allow a JIT to be notified whenever a VM data
// structure changes that would require code patching or recompilation.
////////////////////////////////////////////////////////////////////////////////////

// Called by a JIT in order to be notified whenever the given class (or any of its subclasses?)
// is extended. The callback_data pointer will be passed back to the JIT during the callback.
// The callback function is JIT_extended_class_callback.
void vm_register_jit_extended_class_callback(JIT_Handle jit, Class_Handle clss,
                                             void* callback_data)
{
    assert(clss);
    JIT* jit_to_be_notified = (JIT*)jit;
    Class *c = (Class *)clss;
    clss->register_jit_extended_class_callback(jit_to_be_notified, callback_data);
} // vm_register_jit_extended_class_callback


// Called by a JIT in order to be notified whenever the given method is overridden by a newly
// loaded class. The callback_data pointer will be passed back to the JIT during the callback.
// The callback function is JIT_overridden_method_callback.
void vm_register_jit_overridden_method_callback(JIT_Handle jit, Method_Handle method,
                                                void* callback_data)
{
    assert(method);
    JIT* jit_to_be_notified = (JIT*)jit;
    method->register_jit_overridden_method_callback(jit_to_be_notified, callback_data);
} //vm_register_jit_overridden_method_callback


// Called by a JIT in order to be notified whenever the given method is recompiled or
// initially compiled. The callback_data pointer will be passed back to the JIT during the callback.
// The callback method is JIT_recompiled_method_callback.
void vm_register_jit_recompiled_method_callback(JIT_Handle jit, Method_Handle method,
                                                 void *callback_data)
{
    assert(method);
    JIT *jit_to_be_notified = (JIT *)jit;
    Method *m = (Method *)method;
    m->register_jit_recompiled_method_callback(jit_to_be_notified, callback_data);
} //vm_register_jit_recompiled_method_callback


void vm_patch_code_block(Byte *code_block, Byte *new_code, size_t size)
{
    assert(code_block != NULL);
    assert(new_code != NULL);

    // 20030203 We ensure that no thread is executing code that is simultaneously being patched.
    // We do this in part by stopping the other threads. This ensures that no thread will try to
    // execute code while it is being patched. Also, we take advantage of restrictions on the
    // patches done by JIT on IPF: it replaces the branch offset in a single bundle containing
    // a branch long. Note that this function does not synchronize the I- or D-caches.

    // Run through list of active threads and suspend the other ones.
    hythread_suspend_all(NULL, NULL);
    patch_code_with_threads_suspended(code_block, new_code, size);

    hythread_resume_all(NULL);

} //vm_patch_code_block


// Called by a JIT to have the VM recompile a method using the specified JIT. After
// recompilation, the corresponding vtable entries will be updated, and the necessary
// callbacks to JIT_recompiled_method_callback will be made. It is a requirement that
// the method has not already been compiled by the given JIT; this means that multiple
// instances of a JIT may need to be active at the same time. (See vm_clone_jit.)
void vm_recompile_method(JIT_Handle jit, Method_Handle method)
{
    compile_do_compilation_jit((Method*) method, (JIT*) jit);
} // vm_recompile_method

// Called by JIT during compilation to have the VM synchronously request a JIT (maybe another one)
// to compile another method.
JIT_Result vm_compile_method(JIT_Handle jit, Method_Handle method)
{
    return compile_do_compilation_jit((Method*) method, (JIT*) jit);
} // vm_compile_method



Boolean class_iterator_initialize(ChaClassIterator *chaClassIterator, Class_Handle root_class)
{
    chaClassIterator->_current = NULL;
    chaClassIterator->_is_valid = FALSE;
    chaClassIterator->_root_class = root_class;

    // Partial implementation for now.
    if (!root_class->is_interface() && !root_class->is_array())
    {
        chaClassIterator->_is_valid = TRUE;
        chaClassIterator->_current = root_class;
    }

    return chaClassIterator->_is_valid;
} // class_iterator_initialize


Class_Handle class_iterator_get_current(ChaClassIterator *chaClassIterator)
{
    return (chaClassIterator->_is_valid ? chaClassIterator->_current : NULL);
} // class_iterator_get_current


void class_iterator_advance(ChaClassIterator* chaClassIterator)
{
    if (!chaClassIterator->_is_valid)
        return;
    if (chaClassIterator->_current == NULL)
        return;
    Class* clss = (Class*)chaClassIterator->_current;
    if(clss->get_first_child() != NULL)
    {
        chaClassIterator->_current = (Class_Handle)clss->get_first_child();
        return;
    }
    Class* next = clss;
    while(next != NULL)
    {
        if(next->get_next_sibling() != NULL)
        {
            next = next->get_next_sibling();
            break;
        }
        next = next->get_super_class();
    }
    if(next != NULL && next->get_depth() <= chaClassIterator->_root_class->get_depth())
        next = NULL;
    chaClassIterator->_current = next;
} // class_iterator_advance


Boolean method_iterator_initialize(ChaMethodIterator *chaClassIterator, Method_Handle method, Class_Handle root_class)
{
    assert(method);
    chaClassIterator->_current = NULL;
    class_iterator_initialize(&chaClassIterator->_class_iter, root_class);
    chaClassIterator->_method = method;

    // Don't support static methods for now, since the Signature of a method
    // doesn't distinguish between static and non-static.
    if (method_is_static(method))
    {
        return (chaClassIterator->_class_iter._is_valid = FALSE);
    }

    // If the root_class contains the method, then start the iteration at
    // root_class.  Otherwise, advance the iterator to the first class that
    // contains the method.
    Method *m = class_lookup_method_recursive((Class *) root_class, ((Method *) method)->get_name(), ((Method *) method)->get_descriptor());
    if (m == NULL)
    {
        method_iterator_advance(chaClassIterator);
    }
    else
    {
        chaClassIterator->_current = (Method_Handle) m;
    }

    return chaClassIterator->_class_iter._is_valid;
} // method_iterator_initialize


Method_Handle method_iterator_get_current(ChaMethodIterator *chaClassIterator)
{
    return (chaClassIterator->_class_iter._is_valid ? chaClassIterator->_current : NULL);
} // method_iterator_get_current


void method_iterator_advance(ChaMethodIterator *chaClassIterator)
{
    if (!chaClassIterator->_class_iter._is_valid)
        return;
    if (chaClassIterator->_class_iter._current == NULL)
        return;

    // Move the class iterator forward until a class is found that
    // implements the method, such that the method handle's class
    // is equal to the current class of the iterator.
    Method *m = (Method *) chaClassIterator->_method;
    const String *name = m->get_name();
    const String *desc = m->get_descriptor();
    while (true)
    {
        class_iterator_advance(&chaClassIterator->_class_iter);
        Class *c = (Class *) class_iterator_get_current(&chaClassIterator->_class_iter);
        if (c == NULL)
        {
            chaClassIterator->_current = NULL;
            return;
        }
        Method *next = c->lookup_method(name, desc);
        if (next != NULL && next->get_class() == c)
        {
            chaClassIterator->_current = (Method_Handle) next;
            return;
        }
    }
} // method_iterator_advance


CallingConvention vm_managed_calling_convention()
{
    return CC_Vm;
} //vm_managed_calling_convention

void set_property(const char* key, const char* value, PropertyTable table_number) 
{
    assert(key);
    switch(table_number) {
    case JAVA_PROPERTIES: 
        VM_Global_State::loader_env->JavaProperties()->set(key, value);
    	break;
    case VM_PROPERTIES: 
        VM_Global_State::loader_env->VmProperties()->set(key, value);
        break;
    default:
        ASSERT(0, "Unknown property table: " << table_number);
    }
}

char* get_property(const char* key, PropertyTable table_number)
{
    assert(key);
    char* value;
    switch(table_number) {
    case JAVA_PROPERTIES: 
        value = VM_Global_State::loader_env->JavaProperties()->get(key);
        break;
    case VM_PROPERTIES: 
        value = VM_Global_State::loader_env->VmProperties()->get(key);
        break;
    default:
        value = NULL;
        ASSERT(0, "Unknown property table: " << table_number);
    }
    return value;
}

void destroy_property_value(char* value)
{
    if (value)
    {
        //FIXME which properties?
        VM_Global_State::loader_env->JavaProperties()->destroy(value);
    }
}

int is_property_set(const char* key, PropertyTable table_number)
{
    int value;
    assert(key);
    switch(table_number) {
    case JAVA_PROPERTIES: 
        value = VM_Global_State::loader_env->JavaProperties()->is_set(key) ? 1 : 0;
        break;
    case VM_PROPERTIES: 
        value = VM_Global_State::loader_env->VmProperties()->is_set(key) ? 1 : 0;
        break;
    default:
        value = -1;
        ASSERT(0, "Unknown property table: " << table_number);
    }
    return value;
}

void unset_property(const char* key, PropertyTable table_number)
{
    assert(key);
    switch(table_number) {
    case JAVA_PROPERTIES: 
        VM_Global_State::loader_env->JavaProperties()->unset(key);
        break;
    case VM_PROPERTIES: 
        VM_Global_State::loader_env->VmProperties()->unset(key);
        break;
    default:
        ASSERT(0, "Unknown property table: " << table_number);
    }
}

char** get_properties_keys(PropertyTable table_number)
{
    char** value;
    switch(table_number) {
    case JAVA_PROPERTIES: 
        value = VM_Global_State::loader_env->JavaProperties()->get_keys();
        break;
    case VM_PROPERTIES: 
        value = VM_Global_State::loader_env->VmProperties()->get_keys();
        break;
    default:
        value = NULL;
        ASSERT(0, "Unknown property table: " << table_number);
    }
    return value;
}

char** get_properties_keys_staring_with(const char* prefix, PropertyTable table_number)
{
    assert(prefix);
    char** value;
    switch(table_number) {
    case JAVA_PROPERTIES: 
        value = VM_Global_State::loader_env->JavaProperties()->get_keys_staring_with(prefix);
        break;
    case VM_PROPERTIES: 
        value = VM_Global_State::loader_env->VmProperties()->get_keys_staring_with(prefix);
        break;
    default:
        value = NULL;
        ASSERT(0, "Unknown property table: " << table_number);
    }
    return value;
}

void destroy_properties_keys(char** keys)
{
    if (keys)
    {
        //FIXME which properties?
        VM_Global_State::loader_env->JavaProperties()->destroy(keys);
    }
}

int get_int_property(const char *property_name, int default_value, PropertyTable table_number)
{
    assert(property_name);
    char *value = get_property(property_name, table_number);
    int return_value = default_value;
    if (NULL != value)
    {
        return_value = atoi(value);
        destroy_property_value(value);
    }
    return return_value;
}

int64 get_numerical_property(const char *property_name, int64 default_value, PropertyTable table_number)
{
    assert(property_name);
    char *value = get_property(property_name, table_number);
    int64 return_value = default_value;
    if (NULL != value)
    {
        int64 size = atol(value);
        int sizeModifier = tolower(value[strlen(value) - 1]);
        destroy_property_value(value);

        size_t unit = 1;
        switch (sizeModifier) {
        case 'k': unit = 1024; break;
        case 'm': unit = 1024 * 1024; break;
        case 'g': unit = 1024 * 1024 * 1024;break;
        }

        return_value = size * unit;
        if (return_value / unit != size) {
            /* overflow happened */
            return_value = default_value;
        }
    }
    return return_value;
}
Boolean get_boolean_property(const char *property_name, Boolean default_value, PropertyTable table_number)
{
    assert(property_name);
    char *value = get_property(property_name, table_number);
    if (NULL == value)
    {
        return default_value;
    }
    Boolean return_value = default_value;
    if (0 == strcmp("no", value)
        || 0 == strcmp("off", value)
        || 0 == strcmp("false", value)
        || 0 == strcmp("0", value))
    {
        return_value = FALSE;
    }
    else if (0 == strcmp("yes", value)
             || 0 == strcmp("on", value)
             || 0 == strcmp("true", value)
             || 0 == strcmp("1", value))
    {
        return_value = TRUE;
    }
    destroy_property_value(value);
    return return_value;
}


static Annotation* lookup_annotation(AnnotationTable* table, Class* owner, Class* antn_type) {
    for (int i = table->length - 1; i >= 0; --i) {
        Annotation* antn = table->table[i];
        Type_Info_Handle tih = (Type_Info_Handle) type_desc_create_from_java_descriptor(antn->type->bytes, owner->get_class_loader());
        if (tih) {
            Class* type = type_info_get_class(tih);
            if (antn_type == type) {
                return antn;
            }
        }
    }
    LOG("No such annotation " << antn_type->get_name()->bytes);
    return NULL;
}


Boolean method_has_annotation(Method_Handle target, Class_Handle antn_type) {
    assert(target);
    assert(antn_type);
    if (target->get_declared_annotations()) {
        Annotation* antn = lookup_annotation(target->get_declared_annotations(), target->get_class(), antn_type);
        return antn!=NULL;
    }
    return false;
}
