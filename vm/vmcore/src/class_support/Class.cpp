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
 * @author Pavel Pervov
 * @version $Revision: 1.1.2.1.4.4 $
 */  

#define LOG_DOMAIN "class"
#include "cxxlog.h"

#include <cctype>

#include "Class.h"
#include "environment.h"
#include "lock_manager.h"
#include "exceptions.h"
#include "compile.h"
#include "open/gc.h"
#include "nogc.h"

// 20020923 Total number of allocations and total number of bytes for class-related data structures. 
// This includes any rounding added to make each item aligned (current alignment is to the next 16 byte boundary).
unsigned Class::num_nonempty_statics_allocations = 0;
unsigned Class::num_statics_allocations          = 0;
unsigned Class::num_vtable_allocations           = 0;
unsigned Class::num_hot_statics_allocations      = 0;
unsigned Class::num_hot_vtable_allocations       = 0;

unsigned Class::total_statics_bytes              = 0;
unsigned Class::total_vtable_bytes               = 0;
unsigned Class::total_hot_statics_bytes          = 0;
unsigned Class::total_hot_vtable_bytes           = 0;


// 2003-01-10: This is a temporary hack to make field compaction the default on IPF
// while not destabilizing JIT work on IA32.  Ultimately both fields should be
// true by default on all platforms.
#if defined _IPF_ || defined _EM64T_
bool Class::compact_fields = true;
bool Class::sort_fields    = true;
#else // !_IPF_
bool Class::compact_fields = false;
bool Class::sort_fields    = false;
#endif // !IPF_


//
// This function doesn't check for fields inherited from superclasses.
//
Field *class_lookup_field(Class *clss, const String* name, const String* desc)
{
    for (unsigned i=0; i<clss->n_fields; i++) {
        if (clss->fields[i].get_name() == name && clss->fields[i].get_descriptor() == desc)
            return &clss->fields[i];
    }
    return NULL;
} //class_lookup_field


Field* class_lookup_field_recursive(Class* clss,
                                    const char* name,
                                    const char* descr)
{
    String *field_name =
        VM_Global_State::loader_env->string_pool.lookup(name);
    String *field_descr =
        VM_Global_State::loader_env->string_pool.lookup(descr);

    return class_lookup_field_recursive(clss, field_name, field_descr);
}


Field* class_lookup_field_recursive(Class *clss,
                                        const String* name, const String* desc)
{
    // Step 1: lookup in self
    Field* field = class_lookup_field(clss, name, desc);
    if(field) return field;

    // Step 2: lookup in direct superinterfaces recursively
    for (int in = 0; in < clss->n_superinterfaces && !field; in++) {
        field = class_lookup_field_recursive(clss->superinterfaces[in].clss, name, desc);
        if(field) return field;
    }

    // Step 3: lookup in super classes recursively
    if(clss->super_class) {
        field = class_lookup_field_recursive(clss->super_class, name, desc);
    }

    return field;
} //class_lookup_field_recursive



Method *class_lookup_method(Class *clss, const String* name, const String* desc)
{
    for (unsigned i=0; i<clss->n_methods; i++) {
        if (clss->methods[i].get_name() == name && clss->methods[i].get_descriptor() == desc)
            return &clss->methods[i];
    }
    return NULL;
} //class_lookup_method



Method *class_lookup_method_recursive(Class *clss, const String* name, const String* desc)
{
    assert(clss);
    Method *m = 0;
    Class *oclss = clss;
    for(; clss && !m; clss = clss->super_class) {
        m = class_lookup_method(clss, name, desc);
    }
    if(m)return m;

    //if not found, search in interfaces, that means
    // clss itself is also interface
    Class_Superinterface *intfs = oclss->superinterfaces;
    for(int i = 0; i < oclss->n_superinterfaces; i++)
        if((m = class_lookup_method_recursive(intfs[i].clss, name, desc)))
            return m;
    return NULL;
} //class_lookup_method_recursive



Method *class_lookup_method_recursive(Class *clss,
                                      const char *name,
                                      const char *descr)
{
    String *method_name =
        VM_Global_State::loader_env->string_pool.lookup(name);
    String *method_descr =
        VM_Global_State::loader_env->string_pool.lookup(descr);

    return class_lookup_method_recursive(clss, method_name, method_descr);
} //class_lookup_method_recursive


Method *class_lookup_method_init(Class *clss,
                            const char *descr)
{
    String *method_name = VM_Global_State::loader_env->Init_String;
    String *method_descr =
        VM_Global_State::loader_env->string_pool.lookup(descr);

    return class_lookup_method(clss, method_name, method_descr);
} //class_lookup_method_init

Method *class_lookup_method_clinit(Class *clss)
{
    return class_lookup_method(clss, VM_Global_State::loader_env->Clinit_String, VM_Global_State::loader_env->VoidVoidDescriptor_String);
} //class_lookup_method_clinit

Method *class_lookup_method(Class *clss,
                            const char *name,
                            const char *descr)
{
    String *method_name =
        VM_Global_State::loader_env->string_pool.lookup(name);
    String *method_descr =
        VM_Global_State::loader_env->string_pool.lookup(descr);

    return class_lookup_method(clss, method_name, method_descr);
} //class_lookup_method

Method *class_get_method_from_vt_offset(VTable *vt,
                                        unsigned offset)
{
    assert(vt);
    unsigned index = (offset - VTABLE_OVERHEAD) / sizeof(void*);
    return vt->clss->vtable_descriptors[index];
} // class_get_method_from_vt_offset

void* Field::get_address()
{
    assert(is_static());
    assert(is_offset_computed());
    return (char *)(get_class()->static_data_block) + get_offset();
} // Field::get_address


Method::Method()
{
    //
    // _vtable_patch may be in one of three states:
    // 1. NULL -- before a vtable is initialized or after all patching is done.
    // 2. Points to a vtable entry which must be patched.  
    //    This state can be recognized because *_vtable_patch == _code
    // 3. Otherwise it points to a list containing multiple vtable patch info.
    //
    _vtable_patch = 0;

    _code = NULL;

    _state = ST_NotCompiled;
    _jits = NULL;
    _side_effects = MSE_Unknown;
    _method_sig = 0;
    inline_records = 0;

    _notify_override_records   = NULL;
    _notify_recompiled_records = NULL;
    _index = 0;
    _max_stack=_max_locals=_n_exceptions=_n_handlers=0;
    _exceptions = NULL;
    _byte_code_length = 0;
    _byte_codes = NULL;
    _handlers = NULL;
    _flags.is_init = 0;
    _flags.is_clinit = 0;
    _flags.is_overridden = 0;
    _flags.is_finalize = 0;
    _flags.is_nop = FALSE;
    _flags.is_registered = 0;

    _line_number_table = NULL;
    _local_vars_table = NULL;
    _num_param_annotations = 0;
    _param_annotations = NULL;
    _default_value = NULL;

    pending_breakpoints = 0;
} //Method::Method

void Method::MethodClearInternals()
{
    if (_notify_recompiled_records != NULL)
    {
        Method_Change_Notification_Record *nr, *prev_nr;
        nr = _notify_recompiled_records;
        while(nr != NULL) {
            prev_nr = nr;
            nr = nr->next;
            STD_FREE(prev_nr);
        }
        _notify_recompiled_records = NULL;
    }   

    if (_line_number_table != NULL)
    {
        STD_FREE(_line_number_table);
        _line_number_table = NULL;
    }

    if (_byte_codes != NULL)
    {
        delete []_byte_codes;
        _byte_codes = NULL;
    }

    /*if (_local_vars_table != NULL)
    {
        STD_FREE(_local_vars_table);
        _local_vars_table = NULL;
    }*/

    if (_handlers != NULL)
    {
        delete []_handlers;
        _handlers = NULL;
    }

    if (_method_sig != 0)
    {
        _method_sig->reset();
        delete _method_sig;
    }

    if (_exceptions != NULL)
        delete []_exceptions;

    VTable_Patches *patch = NULL;
    while (_vtable_patch)
    {
        patch = _vtable_patch;
        _vtable_patch = _vtable_patch->next;
        STD_FREE(patch);
    }
}

void Method::lock()
{
    _class->m_lock->_lock();
}

void Method::unlock()
{
    _class->m_lock->_unlock();
}


////////////////////////////////////////////////////////////////////
// beginpointers between struct Class and java.lang.Class


// 20020419
// Given a struct Class, find its corresponding java.lang.Class instance.
//
// To split struct Class and java.lang.Class into two separate data
// structures, we have to replace all places in the VM source code where
// we map struct Class to java.lang.Class and vice versa by performing a cast.
// Here's an example from FindClass()
// --- begin old code
//        new_handle->java_reference = (ManagedObject *)clss;
// --- end old code
// --- begin new code
//        tmn_suspend_disable();       //---------------------------------v
//        new_handle->java_reference = struct_Class_to_java_lang_Class(clss);
//        tmn_suspend_enable();        //---------------------------------^
// --- end new code
// NB: in the past instances of java.lang.Class were guaranteed to be
// allocated in the fixed space.  Instances of java.lang.Class are now treated 
// by GC the same way as any other Java objects and the GC may choose to move them.
// This is the reason for disabling GC in the example above.
//
ManagedObject *struct_Class_to_java_lang_Class(Class *clss)
{
//sundr    printf("struct to class %s, %p, %p\n", clss->name->bytes, clss, clss->super_class);
    assert(!hythread_is_suspend_enabled());
    assert(clss);
    ManagedObject** hjlc = clss->class_handle;
    assert(hjlc);
    ManagedObject* jlc  = *hjlc;
    assert(jlc != NULL);
    assert(jlc->vt());
    assert(jlc->vt()->clss == VM_Global_State::loader_env->JavaLangClass_Class);
    assert(java_lang_Class_to_struct_Class(jlc) == clss);    // else clss's java.lang.Class had a bad "back" pointer
    return jlc;
} //struct_Class_to_java_lang_Class

/**
 * this function returns the reference to the Class->java_lang_Class
 *
 * @note The returned handle is unsafe in respect to DeleteLocalRef, 
 *       and should not be passed to user JNI functions.
 */
jobject struct_Class_to_java_lang_Class_Handle(Class *clss) {
    // used only for protecting assertions
  #ifndef NDEBUG
    tmn_suspend_disable_recursive();
  #endif
    assert(clss);
    assert(clss->class_handle);
    assert(*(clss->class_handle));
    ManagedObject* UNUSED jlc = *(clss->class_handle);
    assert(jlc->vt());
    //assert(jlc->vt()->clss == VM_Global_State::loader_env->JavaLangClass_Class);
  #ifndef NDEBUG
    // gc disabling was needed only to protect assertions
   tmn_suspend_enable_recursive();
  #endif
    // salikh 2005-04-11
    // this operation is safe because
    //    1 Class.java_lang_Class is enumerated during GC
    //    2 no access to pointer is being made, only slot address is taken
    // 
    // However, it would be unsafe to pass this local reference to JNI code,
    // as user may want to call DeleteLocalRef on it, and it would reset
    // java_lang_Class field of the struct Class
    // return (jclass)(&clss->java_lang_Class);
    //
    // ppervov 2005-04-18
    // redone struct Class to contain class handle instead of raw ManagedObject*
    return (jclass)(clss->class_handle);
}

/* The following two utility functions to ease
   conversion between struct Class and jclass
*/
jclass struct_Class_to_jclass(Class *c)
{
    assert(hythread_is_suspend_enabled());
    tmn_suspend_disable(); // ------------------------vvv
    ObjectHandle h = oh_allocate_local_handle();
    h->object = struct_Class_to_java_lang_Class(c);
    tmn_suspend_enable(); // -------------------------^^^
    return (jclass)h;
}

Class *jclass_to_struct_Class(jclass jc)
{
    Class *c;
    tmn_suspend_disable_recursive();
    c = java_lang_Class_to_struct_Class(jc->object);
    tmn_suspend_enable_recursive();
    return c;
}

Class *jobject_to_struct_Class(jobject jobj) 
{
    tmn_suspend_disable();
    assert(jobj->object);
    assert(jobj->object->vt());
    Class *clss = jobj->object->vt()->clss;
    assert(clss);
    tmn_suspend_enable();
    return clss;
}

// Given a class instance, find its corresponding struct Class.
Class *java_lang_Class_to_struct_Class(ManagedObject *jlc)
{
    assert(!hythread_is_suspend_enabled());
    assert(jlc != NULL);
    assert(jlc->vt());
    //assert(jlc->vt()->clss == VM_Global_State::loader_env->JavaLangClass_Class);

    assert(VM_Global_State::loader_env->vm_class_offset != 0);
    Class **vm_class_ptr = (Class **)(((Byte *)jlc) + VM_Global_State::loader_env->vm_class_offset);
    assert(vm_class_ptr != NULL);

    Class* clss = *vm_class_ptr;
    assert(clss != NULL);
    assert(clss->class_handle);
    assert(*(clss->class_handle) == jlc);
    assert(clss != (Class*) jlc);           // else the two structures still overlap!
    return clss;
} //java_lang_Class_to_struct_Class



void set_struct_Class_field_in_java_lang_Class(const Global_Env* env, ManagedObject** jlc, Class *clss)
{
    assert(managed_object_is_java_lang_class(*jlc));
    assert(env->vm_class_offset != 0);

    Class** vm_class_ptr = (Class **)(((Byte *)(*jlc)) + env->vm_class_offset);
    *vm_class_ptr = clss;
} //set_struct_Class_field_in_java_lang_Class


void class_report_failure(Class* target, uint16 cp_index, jthrowable exn)
{
    assert(cp_index > 0 && cp_index < target->cp_size);
    assert(hythread_is_suspend_enabled());
    if (exn_raised()) {
        TRACE2("classloader.error", "runtime exception in classloading");
        return;
    }
    assert(exn);

    tmn_suspend_disable();
    target->m_lock->_lock();
    // vvv - This should be atomic change
    if (!cp_in_error(target->const_pool, cp_index)) {
        cp_set_error(target->const_pool, cp_index);
        target->const_pool[cp_index].error.cause = ((ObjectHandle)exn)->object;
        target->const_pool[cp_index].error.next = target->m_failedResolution;
        assert(&(target->const_pool[cp_index]) != target->m_failedResolution);
        target->m_failedResolution = &(target->const_pool[cp_index]);
    }
    // ^^^
    target->m_lock->_unlock();
    tmn_suspend_enable();
}


jthrowable class_get_linking_error(Class* clss, unsigned index)
{
    assert(cp_in_error(clss->const_pool, index));
    return (jthrowable)(&(clss->const_pool[index].error.cause));
}

String* class_get_java_name(Class* clss, Global_Env* env) 
{
    unsigned len = clss->name->len + 1;
    char * name = (char*) STD_ALLOCA(len);
    char * tmp_name = name;
    memcpy(name, clss->name->bytes, len);
    while (tmp_name = strchr(tmp_name, '/')) {
        *tmp_name = '.';
        ++tmp_name;
    }
    return VM_Global_State::loader_env->string_pool.lookup(name);
}

String* class_get_simple_name(Class* clss, Global_Env* env)
{
    if (!clss->simple_name) 
    {
        if (clss->is_array) 
        {
            String* simple_base_name = class_get_simple_name(clss->array_base_class, env);
            unsigned len = simple_base_name->len;
            unsigned dims = clss->n_dimensions;
            char * buf = (char*)STD_ALLOCA(dims * 2 + len);
            strcpy(buf, simple_base_name->bytes);
            while (dims-- > 0) {
                buf[len++] = '[';
                buf[len++] = ']';
            }
            clss->simple_name = env->string_pool.lookup(buf, len);
        } 
        else 
        {
            const char* fn = clss->name->bytes;
            const char* start;
            if (clss->enclosing_class_index) 
            {
                const char* enclosing_name = const_pool_get_class_name(clss, 
                    clss->enclosing_class_index);
                start = fn + strlen(enclosing_name);
                while (*start == '$' || isdigit(*start)) start++;
            } 
            else 
            {
                start = strrchr(fn, '/');
            }

            if (start) {
                clss->simple_name = env->string_pool.lookup(start + 1);
            } else {
                clss->simple_name = const_cast<String*> (clss->name);
            }
        }
    }
    return clss->simple_name;
}

void vm_notify_live_object_class(Class_Handle clss)
{
    if(!clss->m_markBit) {
        clss->m_markBit = 1;
        mark_classloader(clss->class_loader);
    }
}

// end pointers between struct Class and java.lang.Class
////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////
// begin Support for compressed and raw reference pointers

Byte *Class::heap_base = (Byte *)NULL;
Byte *Class::heap_end  = (Byte *)NULL;
Byte *Class::managed_null = (Byte *)NULL;


bool is_compressed_reference(COMPRESSED_REFERENCE compressed_ref) 
{
    // A compressed reference is an offset into the heap.
    uint64 heap_max_size = (Class::heap_end - Class::heap_base);
    return ((uint64) compressed_ref) < heap_max_size;
} // is_compressed_reference



bool is_null_compressed_reference(COMPRESSED_REFERENCE compressed_ref)
{
    // Null compressed references are represented as 0.
    return (compressed_ref == 0); 
} //is_null_compressed_reference



COMPRESSED_REFERENCE compress_reference(ManagedObject *obj) {
    assert(VM_Global_State::loader_env->compress_references);
     COMPRESSED_REFERENCE compressed_ref;
     if(obj == NULL)
         compressed_ref = 0;
     else
         compressed_ref = (COMPRESSED_REFERENCE)((POINTER_SIZE_INT)obj - (POINTER_SIZE_INT)Class::heap_base);
    assert(is_compressed_reference(compressed_ref));
    return compressed_ref;
} //compress_reference



ManagedObject *uncompress_compressed_reference(COMPRESSED_REFERENCE compressed_ref) {
    assert(VM_Global_State::loader_env->compress_references);
    assert(is_compressed_reference(compressed_ref));
    if (compressed_ref == 0) {
        return NULL;
    } else {
        return (ManagedObject *)(Class::heap_base + compressed_ref);
    }
} //uncompress_compressed_reference



// Given the address of a slot containing a reference, returns the raw reference pointer whether the slot held
// a compressed or uncompressed.reference.
ManagedObject *get_raw_reference_pointer(ManagedObject **slot_addr)
{
    ManagedObject *obj = NULL;
    if (VM_Global_State::loader_env->compress_references) {
        COMPRESSED_REFERENCE offset = *((COMPRESSED_REFERENCE *)slot_addr);
        assert(is_compressed_reference(offset));
        if (offset != 0) {
            obj = (ManagedObject *)(Class::heap_base + offset);
        }
    } else {
        obj = *slot_addr;
    }
    return obj;
} //get_raw_reference_pointer


// end Support for compressed and raw reference pointers
////////////////////////////////////////////////////////////////////




VTable *create_vtable(Class *p_class, unsigned n_vtable_entries)
{
    unsigned vtable_size = VTABLE_OVERHEAD + n_vtable_entries * sizeof(void *);

    // Always allocate vtable data from vtable_data_pool
    void *p_gc_hdr = allocate_vtable_data_from_pool(vtable_size);

#ifdef VM_STATS
        // For allocation statistics, include any rounding added to make each item aligned (current alignment is to the next 16 byte boundary).
        unsigned num_bytes = (vtable_size + 15) & ~15;
        // 20020923 Total number of allocations and total number of bytes for class-related data structures.
        Class::num_vtable_allocations++;
        Class::total_vtable_bytes += num_bytes;
#endif
    assert(p_gc_hdr);
    memset(p_gc_hdr, 0, vtable_size);

    VTable *vtable = (VTable *)p_gc_hdr;

    if(p_class && p_class->super_class) {
        p_class->depth = p_class->super_class->depth + 1;
        memcpy(&vtable->superclasses,
               &p_class->super_class->vtable->superclasses,
               sizeof(vtable->superclasses));
        for(int i = 0; i < vm_max_fast_instanceof_depth(); i++) {
            if(vtable->superclasses[i] == NULL) {
                vtable->superclasses[i] = p_class;
                break;
            }
        }
    }
    if (p_class->depth > 0 &&
        p_class->depth < vm_max_fast_instanceof_depth() &&
        !p_class->is_array &&
        !class_is_interface(p_class))
    {
        p_class->is_suitable_for_fast_instanceof = 1;
    }
    return vtable;
} //create_vtable

// Function registers a number of native methods to a given class.
bool
class_register_methods(Class_Handle klass,
                       const JNINativeMethod* methods,
                       int num_methods)
{
    // get class and string pool
    String_Pool &pool = VM_Global_State::loader_env->string_pool;
    for( int index = 0; index < num_methods; index++ ) {
        // look up strings in string pool
        const String *name = pool.lookup(methods[index].name);
        const String *desc = pool.lookup(methods[index].signature);

        // find method from class
        bool not_found = true;
        for( int count = 0; count < klass->n_methods; count++ ) {
            Method *class_method = &klass->methods[count];
            const String *method_name = class_method->get_name();
            const String *method_desc = class_method->get_descriptor();
            if( method_name == name && method_desc == desc )
            {
                // trace
                TRACE2("class.native", "Register native method: "
                    << klass->name->bytes << "." << name->bytes << desc->bytes);

                // found method
                not_found = false;

                // Calling callback for NativeMethodBind event
                NativeCodePtr native_addr = methods[index].fnPtr;
                jvmti_process_native_method_bind_event( (jmethodID) class_method, native_addr, &native_addr);

                // lock class
                klass->m_lock->_lock();
                class_method->set_code_addr( native_addr );
                class_method->set_registered( true );
                klass->m_lock->_unlock();
                break;
            }
        }
        if( not_found ) {
            // create error string "<class_name>.<method_name><method_descriptor>
            int clen = strlen(klass->name->bytes);
            int mlen = strlen(name->bytes);
            int dlen = strlen(desc->bytes);
            int len = clen + 1 + mlen + dlen;
            char *error = (char*)STD_ALLOCA(len + 1);
            memcpy(error, klass->name->bytes, clen);
            error[clen] = '.';
            memcpy(error + clen + 1, name->bytes, mlen);
            memcpy(error + clen + 1 + mlen, desc->bytes, dlen);
            error[len] = '\0';

            // trace
            TRACE2("class.native", "Native could not be registered: "
                << klass->name->bytes << "." << name->bytes << desc->bytes);

            // raise an exception
            jthrowable exc_object = exn_create("java/lang/NoSuchMethodError", error);
            exn_raise_object(exc_object);
            return true;
        }
    }
    return false;
} // class_register_methods

// Function unregisters a native methods of a given class.
bool
class_unregister_methods(Class_Handle klass)
{
    // lock class
    klass->m_lock->_lock();
    for( int count = 0; count < klass->n_methods; count++ ) {
        Method *method = &klass->methods[count];
        if( method->is_registered() ) {
            // trace
            TRACE2("class.native", "Unregister native method: "
                << klass->name << "." << method->get_name()->bytes 
                << method->get_descriptor()->bytes);

            // reset registered flag
            method->set_registered( false );
        }
    }
    // unlock class
    klass->m_lock->_unlock();
    return false;
} // class_unregister_methods
