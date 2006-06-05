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
 * @author Pavel Pervov
 * @version $Revision: 1.1.2.3.4.4 $
 */  

#define LOG_DOMAIN util::CLASS_LOGGER
#include "cxxlog.h"

#include "environment.h"
#include "String_Pool.h"
#include "Class.h"
#include "nogc.h"
#include "GlobalClassLoaderIterator.h"
#include "open/thread.h"
#include "verifier.h"
#include "native_overrides.h"

Global_Env::Global_Env(tl::MemoryPool & mp, Properties & prop):
mem_pool(mp),
bootstrap_class_loader(NULL),
system_class_loader(NULL),
properties(prop),
bootstrapping(false),
ready_for_exceptions(false)
{
    JavaLangThrowable_String = string_pool.lookup("java/lang/Throwable");
    JavaLangArrayIndexOutOfBoundsException_String = string_pool.lookup("java/lang/ArrayIndexOutOfBoundsException");
    JavaNioByteBuffer_String = string_pool.lookup("java/nio/ByteBuffer");
    JavaLangIllegalArgumentException_String = string_pool.lookup("java/lang/IllegalArgumentException");
    JavaLangReflectField_String = string_pool.lookup("java/lang/reflect/Field");
    JavaLangReflectConstructor_String = string_pool.lookup("java/lang/reflect/Constructor");
    JavaLangReflectMethod_String = string_pool.lookup("java/lang/reflect/Method");
    JavaLangUnsatisfiedLinkError_String = string_pool.lookup("java/lang/UnsatisfiedLinkError");
    JavaLangNullPointerException_String = string_pool.lookup("java/lang/NullPointerException");
    JavaLangString_String = string_pool.lookup("java/lang/String");
    JavaLangObject_String = string_pool.lookup("java/lang/Object");
    JavaLangClass_String = string_pool.lookup("java/lang/Class");
    Init_String = string_pool.lookup("<init>");
    Clinit_String = string_pool.lookup("<clinit>");
    FinalizeName_String = string_pool.lookup("finalize");
    VoidVoidDescriptor_String = string_pool.lookup("()V");
    ByteDescriptor_String = string_pool.lookup("B");
    CharDescriptor_String = string_pool.lookup("C");
    DoubleDescriptor_String = string_pool.lookup("D");
    FloatDescriptor_String = string_pool.lookup("F");
    IntDescriptor_String = string_pool.lookup("I");
    LongDescriptor_String = string_pool.lookup("J");
    ShortDescriptor_String = string_pool.lookup("S");
    BooleanDescriptor_String = string_pool.lookup("Z");
    Clonable_String = string_pool.lookup("java/lang/Cloneable");
    Serializable_String = string_pool.lookup("java/io/Serializable");

    Length_String = string_pool.lookup("length");
    LoadClass_String = string_pool.lookup("loadClass");
    InitCause_String = string_pool.lookup("initCause");
    FromStringConstructorDescriptor_String = string_pool.lookup("(Ljava/lang/String;)V");
    LoadClassDescriptor_String = string_pool.lookup("(Ljava/lang/String;)Ljava/lang/Class;");
    InitCauseDescriptor_String = string_pool.lookup("(Ljava/lang/Throwable;)Ljava/lang/Throwable;");

    //
    // globals
    //

    // HT support from command line disabled. VM should automatically detect HT status at startup.
    is_hyperthreading_enabled = false;
    use_lil_stubs = true;
#if defined _IPF_ || defined _EM64T_
    compress_references = true;
#else // !_IPF_
    compress_references = false;
#endif // !_IPF_

    strings_are_compressed = false;
    use_large_pages = false;
    verify_all = false;
    pin_interned_strings = false; 

    //
    // preloaded classes
    //
    Boolean_Class = NULL;
    Char_Class = NULL;
    Float_Class = NULL;
    Double_Class = NULL;
    Byte_Class = NULL;
    Short_Class = NULL;
    Int_Class = NULL;
    Long_Class = NULL;
    ArrayOfBoolean_Class = NULL;
    ArrayOfChar_Class = NULL;
    ArrayOfFloat_Class = NULL;
    ArrayOfDouble_Class = NULL;
    ArrayOfByte_Class = NULL;
    ArrayOfShort_Class = NULL;
    ArrayOfInt_Class = NULL;
    ArrayOfLong_Class = NULL;
    JavaLangObject_Class = NULL;
    JavaLangString_Class = NULL;
    JavaLangClass_Class = NULL;
    java_lang_Throwable_Class = NULL;
    java_lang_Error_Class = NULL;
    java_lang_ExceptionInInitializerError_Class = NULL;
    java_lang_NullPointerException_Class = NULL;
    java_lang_ArrayIndexOutOfBoundsException_Class = NULL;
    java_lang_ArrayStoreException_Class = NULL;
    java_lang_ArithmeticException_Class = NULL;
    java_lang_ClassCastException_Class = NULL;
    java_lang_OutOfMemoryError_Class = NULL;
    java_lang_OutOfMemoryError = NULL;
    java_io_Serializable_Class = NULL;
    java_lang_Cloneable_Class = NULL;
    java_lang_Thread_Class = NULL;
    java_lang_ThreadGroup_Class = NULL;

    java_lang_reflect_Constructor_Class = NULL;
    java_lang_reflect_Field_Class = NULL;
    java_lang_reflect_Method_Class = NULL;

    JavaLangString_VTable = NULL;
    JavaLangString_allocation_handle = 0;

    vm_class_offset = 0;
    shutting_down = 0;

    TI = NULL;

    nsoTable = nso_init_lookup_table(&this->string_pool);
}       //Global_Env::Global_Env

void Global_Env::EnvClearInternals()
{
    // No GC should work during iteration
    tmn_suspend_disable();
    GlobalClassLoaderIterator ClIterator;
    ClassLoader *cl = ClIterator.first();
    while(cl) {
        ClassLoader* cltmp = cl;
        cl = ClIterator.next();
        delete cltmp;
    }
    tmn_suspend_enable();

    if (TI)
        delete TI;

    nso_clear_lookup_table(nsoTable);
    nsoTable = NULL;
}

Class* Global_Env::LoadCoreClass(const String* s)
{
    Class* clss =
        bootstrap_class_loader->LoadVerifyAndPrepareClass(this, s);
    if(clss == NULL) {
        // print error diagnostics and exit VM
        WARN("Failed to load bootstrap class " << s->bytes);
        LOGGER_EXIT(1);
    }
    return clss;
}
