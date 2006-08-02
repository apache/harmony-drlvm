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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.1.2.5.4.3 $
 */  

#ifndef _ENVIRONMENT_H
#define _ENVIRONMENT_H

#include "tl/memory_pool.h"
#include "String_Pool.h"
#include "vm_core_types.h"
#include "object_handles.h"
#include "jvmti_internal.h"
#include "open/compmgr.h"
#include "open/em_vm.h"

typedef struct NSOTableItem NSOTableItem;

struct Global_Env {
 public:
    tl::MemoryPool&           mem_pool; // memory pool
    String_Pool               string_pool;  // string table
    BootstrapClassLoader*     bootstrap_class_loader;
    JavaVMInitArgs            vm_arguments;
    UserDefinedClassLoader*   system_class_loader;
    Properties&               properties;
    DebugUtilsTI*             TI;
    NSOTableItem*             nsoTable;
    void*                     portLib;  // Classlib's port library
    
    //
    // globals
    //
    bool is_hyperthreading_enabled; // VM automatically detects HT status at startup.
    bool use_lil_stubs;             // 20030307: Use LIL stubs instead of hand crafted ones.  Default off (IPF) on (IA32).
    bool compress_references;       // 20030311 Compress references in references and vector elements.
    bool strings_are_compressed;    // 2003-05-19: The VM searches the java.lang.String class for a "byte[] bvalue" field at startup,
                                    // as an indication that the Java class library supports compressed strings with 8-bit characters.
    bool use_large_pages;           // 20040109 Use large pages for class-related data such as vtables.
    bool verify_all;                // psrebriy 20050815 Verify all classes including loaded by bootstrap class loader
    bool pin_interned_strings;      // if true, interned strings are never moved

    //
    // preloaded strings
    //

    String* JavaLangObject_String;
    String* JavaLangClass_String;
    String* Init_String;
    String* Clinit_String;
    String* FinalizeName_String;
    String* VoidVoidDescriptor_String;
    String* ByteDescriptor_String;
    String* CharDescriptor_String;
    String* DoubleDescriptor_String;
    String* FloatDescriptor_String;
    String* IntDescriptor_String;
    String* LongDescriptor_String;
    String* ShortDescriptor_String;
    String* BooleanDescriptor_String;
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
    String* JavaLangString_String;

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
    ObjectHandle java_lang_OutOfMemoryError;
    
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
    Allocation_Handle JavaLangString_allocation_handle;

    // Offset to the vm_class field in java.lang.Class;
    unsigned vm_class_offset;
    /**
     * Shutting down state.
     * 0 - working
     * 1 - shutting down
     * 2 - deadly errors in shutdown
     */
    int shutting_down;
    
    // FIXME
    // The whole environemt will be refactored to VM instance
    // The following contains a cached copy of EM interface table
    OpenComponentManagerHandle cm;
    OpenInstanceHandle em_instance;
    OpenEmVmHandle em_interface;

    //
    // constructor
    //
    Global_Env(tl::MemoryPool& mm, Properties& prop);
    // function is used instead of destructor to uninitialize manually
    void EnvClearInternals();

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

    //load a class via bootstrap classloader
    Class* LoadCoreClass(const String* name);

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

private:
    bool bootstrapping;
    bool ready_for_exceptions;
};

#endif // _ENVIRONMENT_H
