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
 * @version $Revision: 1.1.2.5.4.3 $
 */  

#ifndef _ENVIRONMENT_H
#define _ENVIRONMENT_H

#include <apr_pools.h>
#include <apr_thread_mutex.h>

#include "open/hythread.h"
#include "open/compmgr.h"
#include "open/em_vm.h"
#include "mem_alloc.h"

#include "String_Pool.h"
#include "vm_core_types.h"
#include "object_handles.h"
#include "jvmti_internal.h"
#include "method_lookup.h"

typedef struct NSOTableItem NSOTableItem;
typedef struct DynamicCode DynamicCode;
typedef struct Assertion_Registry Assertion_Registry;

struct Global_Env {
  public:
     // Global VM states.
    enum VM_STATE { VM_INITIALIZING, VM_RUNNING, VM_SHUTDOWNING };

    apr_pool_t*               mem_pool; // memory pool
    BootstrapClassLoader*     bootstrap_class_loader;
    UserDefinedClassLoader*   system_class_loader;
    DebugUtilsTI*             TI;
    NSOTableItem*             nsoTable;
    void*                     portLib;  // Classlib's port library
    DynamicCode*              dcList;
    Assertion_Registry*       assert_reg;
    PoolManager*              GlobalCodeMemoryManager;
    PoolManager*              VTableMemoryManager;

    Method_Lookup_Table*      vm_methods;
    hythread_library_t        hythread_lib;
    String_Pool               string_pool;  // string table
    JavaVMInitArgs            vm_arguments;


    //
    // globals
    //
    bool is_hyperthreading_enabled; // VM automatically detects HT status at startup.
    bool use_lil_stubs;             // 20030307: Use LIL stubs instead of hand crafted ones.  Default off (IPF) on (IA32).
    bool compress_references;       // 20030311 Compress references in references and vector elements.
    bool strings_are_compressed;    // 2003-05-19: The VM searches the java.lang.String class for a "byte[] bvalue" field at startup,
                                    // as an indication that the Java class library supports compressed strings with 8-bit characters.
    bool use_large_pages;           // 20040109 Use large pages for class-related data such as vtables.
    size_t system_page_size;        // system page size according to use_large_pages value
    bool verify_all;                // psrebriy 20050815 Verify all classes including loaded by bootstrap class loader
    bool pin_interned_strings;      // if true, interned strings are never moved

    Lock_Manager *p_jit_a_method_lock;
    Lock_Manager *p_vtable_patch_lock;
    Lock_Manager *p_meth_addr_table_lock;
    Lock_Manager *p_method_call_lock;
    Lock_Manager *p_handle_lock;

    // If set to true by the "-compact_fields" command line option,
    // the VM will not pad out fields of less than 32 bits to four bytes.
    // However, fields will still be aligned to a natural boundary,
    // and the num_field_padding_bytes field will reflect those alignment padding bytes.
    bool compact_fields;

    // If set to true by the "-sort_fields" command line option,
    // the VM will sort fields by size before assigning their offset during
    // class preparation.
    bool sort_fields;

    // Base address of Java heap
    Byte* heap_base;
    // Ceiling of Java heap.
    // NOTE: we assume Java heap uses one continuous memory block.
    Byte* heap_end;

    // This will be set to either NULL or heap_base depending
    // on whether compressed references are used.
    Byte* managed_null;

    //
    // preloaded strings
    //

    String* JavaLangObject_String;
    String* JavaLangClass_String;
    String* Init_String;
    String* Clinit_String;
    String* FinalizeName_String;
    String* EnqueueName_String;
    String* VoidVoidDescriptor_String;
    String* VoidIntegerDescriptor_String;
    String* VoidBooleanDescriptor_String;
    String* Clonable_String;
    String* Serializable_String;

    String* JavaLangReflectMethod_String;
    String* JavaLangNullPointerException_String;
    String* JavaLangUnsatisfiedLinkError_String;
    String* JavaLangReflectConstructor_String;
    String* JavaLangReflectField_String;
    String* JavaLangIllegalArgumentException_String;
    String* JavaNioByteBuffer_String;
    String* JavaLangArrayIndexOutOfBoundsException_String;
    String* JavaLangThrowable_String;
    String* JavaLangNoClassDefFoundError_String;
    String* JavaLangString_String;
    String* JavaLangStringBuffer_String;

    String* Length_String;
    String* LoadClass_String;
    String* InitCause_String;
    String* FromStringConstructorDescriptor_String;
    String* LoadClassDescriptor_String;
    String* InitCauseDescriptor_String;

    //
    // preloaded classes
    //
    Class* Boolean_Class;
    Class* Char_Class;
    Class* Float_Class;
    Class* Double_Class;
    Class* Byte_Class;
    Class* Short_Class;
    Class* Int_Class;
    Class* Long_Class;
    Class* Void_Class;

    Class* ArrayOfBoolean_Class;
    Class* ArrayOfChar_Class;
    Class* ArrayOfFloat_Class;
    Class* ArrayOfDouble_Class;
    Class* ArrayOfByte_Class;
    Class* ArrayOfShort_Class;
    Class* ArrayOfInt_Class;
    Class* ArrayOfLong_Class;
    
    Class* JavaLangObject_Class;
    Class* JavaLangString_Class;
    Class* JavaLangClass_Class;

    Class* java_lang_Throwable_Class;
    Class* java_lang_StackTraceElement_Class;
    Class* java_lang_Error_Class;
    Class* java_lang_ExceptionInInitializerError_Class;
    Class* java_lang_NullPointerException_Class;
    Class* java_lang_StackOverflowError_Class;

    Class* java_lang_ClassNotFoundException_Class;
    Class* java_lang_NoClassDefFoundError_Class;
    
    Class* java_lang_ArrayIndexOutOfBoundsException_Class;
    Class* java_lang_ArrayStoreException_Class;
    Class* java_lang_ArithmeticException_Class;
    Class* java_lang_ClassCastException_Class;
    Class* java_lang_OutOfMemoryError_Class;
    Class* java_lang_ThreadDeath_Class;

    ObjectHandle java_lang_Object;
    ObjectHandle java_lang_OutOfMemoryError;
    ObjectHandle java_lang_ThreadDeath;
    // object of java.lang.Error class used for JVMTI JIT PopFrame support
    ObjectHandle popFrameException;

    Class* java_io_Serializable_Class;
    Class* java_lang_Cloneable_Class;
    Class* java_lang_Thread_Class;
    Class* java_lang_ThreadGroup_Class;
    Class* java_util_Date_Class;
    Class* java_util_Properties_Class;
    Class* java_lang_Runtime_Class; 

    Class* java_lang_reflect_Constructor_Class;
    Class* java_lang_reflect_Field_Class;
    Class* java_lang_reflect_Method_Class;

    Class* finalizer_thread;
    // pointers to 2 static fields in FinalizerThread class. 
    jboolean* finalizer_shutdown;
    jboolean* finalizer_on_exit;
    Class* java_lang_EMThreadSupport_Class;

    // VTable for the java_lang_String class
    VTable* JavaLangString_VTable;

    // Offset to the vm_class field in java.lang.Class;
    unsigned vm_class_offset;

    // The VM state. See VM_STATE enum above.
    volatile int vm_state;

    // FIXME
    // The whole environemt will be refactored to VM instance
    // The following contains a cached copy of EM interface table
    OpenComponentManagerHandle cm;
    OpenInstanceHandle em_instance;
    OpenEmVmHandle em_interface;

    Global_Env(apr_pool_t * pool);
    ~Global_Env();

    void * operator new(size_t size, apr_pool_t * pool) {
        return apr_palloc(pool, sizeof(Global_Env));
    }

    void operator delete(void *) {}

    void operator delete(void * mem, apr_pool_t * pool) {};

    //
    // determine bootstrapping of root classes
    //
    bool InBootstrap() const { return bootstrapping; }
    void StartVMBootstrap() {
        assert(!bootstrapping);
        bootstrapping = true;
    }
    void FinishVMBootstrap() {
        assert(bootstrapping);
        bootstrapping = false;
    }

    int isVmInitializing() {
        return vm_state == VM_INITIALIZING;
    }

    int isVmRunning() {
        return vm_state == VM_RUNNING;
    }

    int IsVmShutdowning() {
        return vm_state == VM_SHUTDOWNING;
    }

    //load a class via bootstrap classloader
    Class* LoadCoreClass(const String* name);
    Class* LoadCoreClass(const char* name);

    /** 
    * Set "Ready For Exceptions" state.
    * This function must be called as, soon as VM becomes able to create 
    * exception objects. I.e. all required classes (such as "java/lang/Trowable")
    * are loaded .
    */
    void ReadyForExceptions()
    {
        ready_for_exceptions = true;
    }

    /** 
    * Get "Ready For Exceptions" state.
    * @return true, if VM is able to create exception objects.
    */
    bool IsReadyForExceptions() const
    {
        return ready_for_exceptions;
    }

    Properties* JavaProperties() {
        return m_java_properties;
    }

    Properties* VmProperties() {
        return m_vm_properties;
    }

private:
    bool bootstrapping;
    bool ready_for_exceptions;
    Properties* m_java_properties;
    Properties* m_vm_properties;
};

#endif // _ENVIRONMENT_H
