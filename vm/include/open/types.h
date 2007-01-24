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

#ifndef _VM_TYPES_H_
#define _VM_TYPES_H_

#include "common.h"

#define MAX_UINT32 0xffffffff
/**
 * DLL stuff
 */
#if defined(PLATFORM_POSIX) || (defined(USE_STATIC_GC) && defined(BUILDING_GC))

#define VMEXPORT
#define VMIMPORT
#define JITEXPORT
#define EMEXPORT

#else  // !PLATFORM_POSIX

#if defined(BUILDING_VM) && !defined(STATIC_BUILD)
#define VMEXPORT __declspec(dllexport)
#define JITEXPORT __declspec(dllimport)
#define EMEXPORT __declspec(dllimport)
#define VMIMPORT __declspec(dllimport)
#else  // !BUILDING_VM
#define VMEXPORT __declspec(dllimport)
#define JITEXPORT __declspec(dllexport)
#define EMEXPORT __declspec(dllexport)
#define VMIMPORT __declspec(dllexport)
#endif // !BUILDING_VM

#endif // !PLATFORM_POSIX

/**
 * Various Numeric Types
 */

// Boolean, uint8, int8, uint16, int16, uint32, int32, uint64, int64,
// POINTER_SIZED_INT

// We can't use bool in non-C++ code
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
typedef unsigned Boolean;

typedef unsigned char    Byte;

//::For long long int, add the LL 
#ifndef __WORDSIZE // exclude remark #193: zero used for undefined preprocessing identifier
#define __WORDSIZE 0
#endif
# if __WORDSIZE == 64 || defined(WIN32)
#  define __INT64_C(c)  c ## L
#  define __UINT64_C(c) c ## UL
# else
#  define __INT64_C(c)  c ## LL
#  define __UINT64_C(c) c ## ULL
# endif

#ifdef PLATFORM_POSIX
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

typedef   signed char  int8;
typedef   signed short int16;
typedef   signed int  int32; 
typedef   signed long long int64;

#else //!PLATFORM_POSIX

#ifndef __INSURE__
// these give Insure++ problems:
typedef unsigned __int8  uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef   signed __int8  int8;
typedef   signed __int16 int16;
#else
// so use these definitions instead with Insure++:
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef   signed char int8;
typedef   signed short int16;
#endif
typedef unsigned __int64 uint64;
typedef   signed __int64 int64;
#if (_MSC_VER != 1100 || !defined _WINSOCK2API_)
#ifndef __INSURE__
// this gives Insure++ problems:
typedef   signed __int32 int32;
#else
// so use this definition instead with Insure++:
typedef signed int int32;
#endif // __INSURE__
#endif

#endif //!PLATFORM_POSIX

/**
 * The integer datatype on the platform that can hold a pointer.
 * ? 02/7/25: addded <code>POINTER_SIZE_SINT</code>, since in some cases
 * a pointer-size integer has to be signed (for example, when specifying LIL offsets)
 */
 #ifdef POINTER64
#define POINTER_SIZE_INT uint64
#define POINTER_SIZE_SINT int64
#ifdef PLATFORM_NT
#define PI_FMT "I64"
#else 
#define PI_FMT "ll"
#endif
#else
#define POINTER_SIZE_INT uint32
#define POINTER_SIZE_SINT int32
#define PI_FMT ""
#endif // POINTER64

/**
 * <code>VM_Data_Type</code>
 */

typedef
enum VM_Data_Type {
    VM_DATA_TYPE_INT8    = 'B',
    VM_DATA_TYPE_UINT8   = 'b',
    VM_DATA_TYPE_INT16   = 'S',
    VM_DATA_TYPE_UINT16  = 's',
    VM_DATA_TYPE_INT32   = 'I',
    VM_DATA_TYPE_UINT32  = 'i',
    VM_DATA_TYPE_INT64   = 'J',
    VM_DATA_TYPE_UINT64  = 'j',
    VM_DATA_TYPE_INTPTR  = 'N',
    VM_DATA_TYPE_UINTPTR = 'n',
    VM_DATA_TYPE_F8      = 'D',
    VM_DATA_TYPE_F4      = 'F',
    VM_DATA_TYPE_BOOLEAN = 'Z',
    VM_DATA_TYPE_CHAR    = 'C',
    VM_DATA_TYPE_CLASS   = 'L',
    VM_DATA_TYPE_ARRAY   = '[',
    VM_DATA_TYPE_VOID    = 'V',
    VM_DATA_TYPE_MP      = 'P',        // managed pointers
    VM_DATA_TYPE_UP      = 'p',        // unmanaged pointers
    VM_DATA_TYPE_VALUE   = 'K',
    //
    VM_DATA_TYPE_STRING  = '$',        // deprecated
    //
    VM_DATA_TYPE_INVALID = '?',
    VM_DATA_TYPE_END     = ')'         // For the iterator
} VM_Data_Type; //VM_Data_Type

/**
 * (? 20030317) These defines are deprecated.
 * Use <code>VM_Data_Type</code> in all new code.
 */
#define Java_Type           VM_Data_Type
#define JAVA_TYPE_BYTE      VM_DATA_TYPE_INT8
#define JAVA_TYPE_CHAR      VM_DATA_TYPE_CHAR
#define JAVA_TYPE_DOUBLE    VM_DATA_TYPE_F8
#define JAVA_TYPE_FLOAT     VM_DATA_TYPE_F4
#define JAVA_TYPE_INT       VM_DATA_TYPE_INT32
#define JAVA_TYPE_LONG      VM_DATA_TYPE_INT64
#define JAVA_TYPE_SHORT     VM_DATA_TYPE_INT16
#define JAVA_TYPE_BOOLEAN   VM_DATA_TYPE_BOOLEAN
#define JAVA_TYPE_CLASS     VM_DATA_TYPE_CLASS
#define JAVA_TYPE_ARRAY     VM_DATA_TYPE_ARRAY
#define JAVA_TYPE_VOID      VM_DATA_TYPE_VOID
#define JAVA_TYPE_STRING    VM_DATA_TYPE_STRING
#define JAVA_TYPE_INVALID   VM_DATA_TYPE_INVALID
#define JAVA_TYPE_END       VM_DATA_TYPE_END

/**
 * Handles for Various VM Structures.
 *
 * This header file is also used in pure C sources,
 * thus we use struct instead of classes.
 */
typedef struct Class *Class_Handle;
typedef struct VTable *VTable_Handle;
typedef struct Field *Field_Handle;
typedef struct Method *Method_Handle;
typedef struct Method_Signature *Method_Signature_Handle;
typedef struct TypeDesc *Type_Info_Handle;
typedef POINTER_SIZE_INT Allocation_Handle;
typedef POINTER_SIZE_INT Runtime_Type_Handle;
typedef void* NativeCodePtr;
typedef struct ClassLoader* ClassLoaderHandle;
typedef struct ManagedObject* ManagedPointer;

/**
 * Fields of these types are not directly accessible from the core VM.
 *  typedef struct ManagedObject Java_java_lang_Class;
 *  typedef ManagedObject Java_java_lang_System;
 */
typedef struct ManagedObject Java_java_lang_Throwable;
typedef struct ManagedObject Java_java_lang_Thread;
typedef struct ManagedObject Java_java_io_FileInputStream;
typedef struct ManagedObject Java_java_lang_String;


/**
 * Used for opaques accesses to managed arrays. This handle points
 * to an array in the managed heap, so handling must be careful to account
 * for the possiblity of a moving GC.
 */
typedef void *Vector_Handle;

typedef void *Managed_Object_Handle;

typedef void *GC_Enumeration_Handle;

/**
 * For disable warnings in release version.
 * warning type: warning: unused variable <type_of variable> <variable>
 */


#endif //!_VM_TYPES_H_
