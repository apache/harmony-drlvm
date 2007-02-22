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
 * @author Pavel Rebriy, Alexei Fedotov
 * @version $Revision: 1.1.2.3.4.4 $
 */  


#ifndef _VERIFIER_REAL_H_
#define _VERIFIER_REAL_H_

/**
 * @file
 * Verifier internal interfaces.
 */

#include <fstream>
#include <sstream>
#include <assert.h>

#include "verifier.h"
#include "open/bytecodes.h"

#define LOG_DOMAIN "verifier"
#include "cxxlog.h"

using namespace std;

/**
 * Verifier defines.
 */
//===========================================================
/**
 * Define debug mode for verifier.
 */
#ifdef NDEBUG
#define _VERIFY_DEBUG 0
#else // NDEBUG
#define _VERIFY_DEBUG 1
#endif // NDEBUG

/**
 * Debugging and trace message defines.
 * @note Debug messages is available only in debug mode, 
 *       trace messages is available in any mode.
 * @note Verifier trace looks like: 'verifier' + component of verifier
 */
#if _VERIFY_DEBUG
#define VERIFY_DEBUG(info) ECHO("vf_debug: " << info)
#else // _VERIFY_DEBUG
#define VERIFY_DEBUG(info)
#endif // _VERIFY_DEBUG
#define VERIFY_TRACE(comp, mess) TRACE2("verifier." comp, mess)
#define VERIFY_REPORT(context, error_message )      \
    {                                               \
        stringstream stream;                        \
        stream << error_message;                    \
        vf_set_error_message( stream, (context) );  \
    }
#define VERIFY_REPORT_CLASS(context, method, error_message )        \
    VERIFY_REPORT(context,                                          \
        "(class: " << class_get_name( (context)->m_class )          \
        << ", method: " << method_get_name( method )                \
        << method_get_descriptor( method )                          \
        << ") " << error_message )
#define VERIFY_REPORT_METHOD(context, error_message )               \
    VERIFY_REPORT_CLASS(context, (context)->m_method, error_message )

/**
 * Define source code line and source file name parameters and arguments.
 */
#if _VERIFY_DEBUG
#define VERIFY_SOURCE_PARAMS                UNREF int line_, UNREF const char *file_
#define VERIFY_SOURCE_ARGS0                 __LINE__, __FILE__
#define VERIFY_SOURCE_ARGS1                 line_, file_
#define VERIFY_REPORT_SOURCE                file_ << "(" << line_ << "): "
#else  // _VERIFY_DEBUG
#define VERIFY_SOURCE_PARAMS                int
#define VERIFY_SOURCE_ARGS0                 0
#define VERIFY_SOURCE_ARGS1                 0
#define VERIFY_REPORT_SOURCE                ""
#endif // _VERIFY_DEBUG

//===========================================================
// Verifier contant pool checks
//===========================================================

/**
 * Constant pool checks.
 */
// for handler id = 0 is legal value
#define CHECK_HANDLER_CONST_POOL_ID( id, len, context )                                 \
    if( (id) >= (len) ) {                                                               \
        VERIFY_REPORT_METHOD( context, "Illegal constant pool index in handler" );      \
        return VER_ErrorHandler;                                                        \
    }
// for handler id = 0 is legal value
#define CHECK_HANDLER_CONST_POOL_CLASS( context, id )                                   \
    if( (id) && class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_Class ) {     \
        VERIFY_REPORT_METHOD( context, "Illegal type in constant pool for handler, "    \
            << id << ": CONSTANT_Class is expected" );                                  \
        return VER_ErrorHandler;                                                        \
    }
#define CHECK_CONST_POOL_ID( id, len, context )                                         \
    if( !(id) || (id) >= (len) ) {                                                      \
        VERIFY_REPORT_METHOD( context, "Illegal constant pool index" );                 \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_CLASS( context, id )                                           \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_Class ) {             \
        VERIFY_REPORT_METHOD( context, "Illegal type in constant pool, "                \
            << id << ": CONSTANT_Class is expected" );                                  \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_METHOD( context, id )                                          \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_Methodref ) {         \
        VERIFY_REPORT_METHOD( context, "Illegal type in constant pool, "                \
            << id << ": CONSTANT_Methodref is expected" );                              \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_INTERFACE( context, id )                                           \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_InterfaceMethodref ) {    \
        VERIFY_REPORT_METHOD( context, "Illegal type in constant pool, "                    \
            << id << ": CONSTANT_InterfaceMethodref is expected" );                         \
        return VER_ErrorConstantPool;                                                       \
    }
#define CHECK_CONST_POOL_FIELD( context, id )                                           \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_Fieldref ) {          \
        VERIFY_REPORT_METHOD( context, "Illegal type in constant pool, "                \
            << id << ": CONSTANT_Fieldref is expected" );                               \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_TYPE( context, id )                                            \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_NameAndType ) {       \
        VERIFY_REPORT_METHOD( context, "Illegal type in constant pool, "                \
            << id << ": CONSTANT_NameAndType is expected" );                            \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_STRING( context, id )                                          \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_String ) {            \
        VERIFY_REPORT_METHOD( context, "Illegal type in constant pool, "                \
            << id << ": CONSTANT_String is expected" );                                 \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_UTF8( context, id )                                            \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_Utf8) {               \
        VERIFY_REPORT_METHOD( context, "Illegal type in constant pool, "                \
            << id << ": CONSTANT_Utf8 is expected" );                                   \
        return VER_ErrorConstantPool;                                                   \
    }

//===========================================================
// Verifier enums
//===========================================================

/**
 * Constraint check types enum.
 */
typedef enum {
    VF_CHECK_NONE = 0,
    VF_CHECK_PARAM,
    VF_CHECK_ASSIGN,
    VF_CHECK_ASSIGN_WEAK,
    VF_CHECK_CAST,
    VF_CHECK_SUPER,
    VF_CHECK_EQUAL,
    VF_CHECK_ARRAY,
    VF_CHECK_REF_ARRAY,
    VF_CHECK_ACCESS_FIELD,
    VF_CHECK_ACCESS_METHOD,
    VF_CHECK_DIRECT_SUPER,
    VF_CHECK_INVOKESPECIAL,
    VF_CHECK_UNINITIALIZED_THIS,
    VF_CHECK_NUM
} vf_CheckConstraint_t;

/** 
 * Stack map entry types enum.
 */
typedef enum {
    // original types
    SM_TOP = 0,
    SM_INT,
    SM_FLOAT,
    SM_NULL,
    SM_REF,
    SM_UNINITIALIZED,
    SM_UNINITIALIZED_THIS,
    SM_LONG_LO,
    SM_DOUBLE_LO,
    SM_LONG_HI,
    SM_DOUBLE_HI,
    SM_RETURN_ADDR,
    // additional types
    SM_COPY_0,
    SM_COPY_1,
    SM_COPY_2,
    SM_COPY_3,
    SM_UP_ARRAY,
    SM_RETURN,
    SM_TERMINATE,
    SM_WORD,
    SM_WORD2_LO,
    SM_WORD2_HI,
    SM_NUMBER
} vf_MapType_t;

/**
 * Each instruction has a descriptive bitmap.
 */
typedef enum {
    VF_TYPE_INSTR_NONE        = 0,
    VF_TYPE_INSTR_RETURN      = 1,
    VF_TYPE_INSTR_THROW       = 2,
    VF_TYPE_INSTR_SUBROUTINE  = 3
} vf_CodeType;

//===========================================================
// Verifier predefined structures
//===========================================================

/**
 * @ingroup Handles
 * The handle of the verifier context.
 */
typedef const struct vf_Context* vf_ContextHandle;

/// Verifier context structure.
typedef struct vf_Context vf_Context_t;
/// Verifier structure for a byte of a bytecode.
typedef struct vf_BCode vf_BCode_t;
/// Verifier valid types structure.
typedef struct vf_ValidType vf_ValidType_t;
/// Verifier stack map entry structure.
typedef struct vf_MapEntry vf_MapEntry_t;
/// Verifier code instruction structure.
typedef struct vf_Code vf_Code_t;
/// Verifier constant pool parse structure.
typedef union vf_Parse vf_Parse_t;
/// Verifier graph structure declaration.
class vf_Graph;
/// Verifier type constraint structure.
typedef struct vf_TypeConstraint vf_TypeConstraint_t;
/// Verifier hash table structure.
typedef struct vf_Hash vf_Hash_t;
/// Verifier hash table entry structure.
typedef struct vf_HashEntry vf_HashEntry_t;
/// Verifier pool structure.
typedef struct vf_VerifyPool vf_VerifyPool_t;
/// Verifier pool internal structure.
typedef struct vf_VerifyPoolInternal vf_VerifyPoolInternal_t;
/// Verifier structure of class loader constraint data.
typedef struct vf_ClassLoaderData vf_ClassLoaderData_t;


//===========================================================
// Verifier memory management functions diclarations
//===========================================================

/**
 * Memory pool entry size.
 */
#define VERIFY_POOL_ENTRY_SIZE  0x2000

/**
 * Macros hide source line and source file name function arguments.
 */
#define vf_error()                                                  \
    vf_error_func(VERIFY_SOURCE_ARGS0)
#define vf_calloc(number, size_element)                             \
    vf_calloc_func((number), (size_element), VERIFY_SOURCE_ARGS0)
#define vf_malloc(size)                                             \
    vf_malloc_func((size), VERIFY_SOURCE_ARGS0)
#define vf_free(pointer)                                            \
    vf_free_func((pointer), VERIFY_SOURCE_ARGS0)
#define vf_realloc(pointer, resize)                                 \
    vf_realloc_func((pointer), (resize), VERIFY_SOURCE_ARGS0)
#define vf_create_pool()                                            \
    vf_create_pool_func(VERIFY_SOURCE_ARGS0)
#define vf_alloc_pool_memory(pool, size)                            \
    vf_alloc_pool_memory_func((pool), (size), VERIFY_SOURCE_ARGS0)
#define vf_clean_pool_memory(pool)                                  \
    vf_clean_pool_memory_func((pool), VERIFY_SOURCE_ARGS0)
#define vf_delete_pool(pool)                                        \
    vf_delete_pool_func((pool), VERIFY_SOURCE_ARGS0)

/**
 * Function performs abend exit from VM.
 * @note <i>VERIFY_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @see VERIFY_SOURCE_PARAMS
 * @see vf_error
 */
void
vf_error_func( VERIFY_SOURCE_PARAMS );

/**
 * Function allocates an array in memory with elements initialized to zero. 
 * @param number        - number of elements
 * @param element_size  - size of element
 * @note <i>VERIFY_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @return Pointer to allocated memory block.
 * @see VERIFY_SOURCE_PARAMS
 * @see vf_calloc
 * @note Assertion is raised if <i>number</i> or <i>element_size</i>
 *       are equal to zero or if out of memory error is arisen.
 * @note Trace is available with argument <b>verifier:memory</b>.
 */
void *
vf_calloc_func( unsigned number, size_t element_size, VERIFY_SOURCE_PARAMS );

/**
 * Function allocates memory blocks.
 * @param size - size of memory block
 * @note <i>VERIFY_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @return Pointer to allocated memory block.
 * @see VERIFY_SOURCE_PARAMS
 * @see vf_malloc
 * @note Assertion is raised if <i>size</i> is equal to zero
 *       or if out of memory error is arisen.
 * @note Trace is available with argument <b>verifier:memory</b>.
 */
void *
vf_malloc_func( size_t size, VERIFY_SOURCE_PARAMS );

/**
 * Function releases allocated memory blocks.
 * @param pointer - pointer to allocated block
 * @note <i>VERIFY_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @see VERIFY_SOURCE_PARAMS
 * @see vf_free
 * @note Assertion is raised if <i>pointer</i> is equal to null.
 * @note Trace is available with argument <b>verifier:memory</b>.
 */
void
vf_free_func( void *pointer, VERIFY_SOURCE_PARAMS );

/**
 * Function reallocates memory blocks.
 * @param pointer   - pointer to allocated block
 * @param resize    - new size of memory block
 * @note <i>VERIFY_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @return Pointer to reallocated memory block.
 * @see VERIFY_SOURCE_PARAMS
 * @see vf_realloc
 * @note Assertion is raised if <i>resize</i> is equal to zero
 *       or if out of memory error is arisen.
 * @note If <i>pointer</i> is equal to null function works like vf_malloc_func.
 * @note Trace is available with argument <b>verifier:memory</b>.
 */
void *
vf_realloc_func( void *pointer, size_t resize, VERIFY_SOURCE_PARAMS );

/**
 * Function creates memory pool structure.
 * @note <i>VERIFY_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @return Pointer to created memory pool.
 * @see VERIFY_SOURCE_PARAMS
 * @see vf_create_pool
 * @see vf_VerifyPool_t
 * @note Trace is available with argument <b>verifier:memory:pool</b>.
 */
vf_VerifyPool_t *
vf_create_pool_func( VERIFY_SOURCE_PARAMS );

/**
 * Function allocates memory blocks in current pool.
 * @param pool - pointer to memory pool structure
 * @param size - memory block size
 * @note <i>VERIFY_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @return Pointer to allocated memory block.
 * @see VERIFY_SOURCE_PARAMS
 * @see vf_alloc_pool_memory
 * @see vf_VerifyPool_t
 * @note Trace is available with argument <b>verifier:memory:pool</b>.
 */
void *
vf_alloc_pool_memory_func( vf_VerifyPool_t *pool, size_t size, VERIFY_SOURCE_PARAMS );

/**
 * Function cleans given pool.
 * @param pool - memory pool structure
 * @note <i>VERIFY_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @see VERIFY_SOURCE_PARAMS
 * @see vf_clean_pool_memory
 * @see vf_VerifyPool_t
 * @note Trace is available with argument <b>verifier:memory:pool</b>.
 */
void
vf_clean_pool_memory_func( vf_VerifyPool_t *pool, VERIFY_SOURCE_PARAMS );

/**
 * Function releases memory from given pool.
 * @param pool - memory pool structure
 * @note <i>VERIFY_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @see VERIFY_SOURCE_PARAMS
 * @see vf_delete_pool
 * @see vf_VerifyPool_t
 * @note Trace is available with argument <b>verifier:memory:pool</b>.
 */
void
vf_delete_pool_func( vf_VerifyPool_t *pool, VERIFY_SOURCE_PARAMS );

//===========================================================
// Code instruction structures.
//===========================================================

/**
 * A byte of a bytecode.
 */
struct vf_BCode {
    unsigned m_instr;       ///< a number + 1 of code instruction at
                            ///< vf_Context.m_code
    unsigned m_mark : 1;    ///< control flow branch flag
};

/**
 * Valid types structure.
 */
struct vf_ValidType {
    unsigned number;          ///< number of types
    const char *string[1];    ///< type array
};

/** 
 * Stack map entry structure.
 */
struct vf_MapEntry {
    vf_ValidType_t *m_vtype;        ///< valid type for reference
    union {
        unsigned m_new;             ///< program count of opcode new for uninitialized
        unsigned m_pc;              ///< program count of return address for subroutine
    };
    union {
        unsigned short m_local;     ///< number of local variable
        unsigned short m_index;     ///< constant pool index for access check
    };
    unsigned short m_type : 5;      ///< stack map type @see vf_MapType_t
    unsigned short m_ctype : 4;     ///< constraint type @see vf_CheckConstraint_t
    unsigned short m_is_local : 1;  ///< local variable modify flag
                                    ///< true - modify local, false - modify stack
};

/**
 * Complete struct of bytecode instructions.
 */
struct vf_Code {
    unsigned char *m_addr;          ///< address of bytecode instruction
    unsigned *m_off;                ///< array of instruction branches
    vf_MapEntry_t *m_invector;      ///< stack map IN instruction vector
    vf_MapEntry_t *m_outvector;     ///< stack map OUT instruction vector
    unsigned m_offcount;            ///< number of instruction branches
    unsigned short m_minstack;      ///< minimal stack for instruction
    unsigned short m_inlen;         ///< stack map IN instruction vector length
    unsigned short m_outlen;        ///< stack map OUT instruction vector length
    short m_stack;                  ///< stack change for instruction
    vf_CodeType m_type;             ///< instruction flag @see vf_CodeType_t
    bool m_basic_block_start : 1;   ///< begin of a basic block
};

/** 
 * Constant pool parse structure.
 */
union vf_Parse {
    struct {
        vf_MapEntry_t *m_invector;   ///< method IN stack vector
        vf_MapEntry_t *m_outvector;  ///< method OUT stack vector
        const char *m_name;          ///< invoked method name
        int m_inlen;                 ///< IN vector length
        int m_outlen;                ///< OUT vector length
    } method;
    struct {
        vf_MapEntry_t *f_vector;     ///< field type map vector
        int f_vlen;                  ///< field type map vector len
    } field;
};

//===========================================================
// Verifier hash table structures.
//===========================================================

/**
 * Structure of hash entry.
 */
struct vf_HashEntry {
    const char *key;            ///< hash entry key
    void *data;                 ///< pointer to hash entry data
    vf_HashEntry_t *next;       ///< next hash entry
};

/**
 * Verifier hash table structure.
 */
struct vf_Hash
{
public:
    /**
     * Hash table constructor.
     * @note Function allocates memory for hash pool and hash table.
     */
    vf_Hash();

    /**
     * Hash table constructor.
     * @param pool - external memory pool
     * @note Function allocates memory for hash pool and hash table.
     */
    vf_Hash( vf_VerifyPool_t *pool);

    /**
     * Hash table destructor.
     * @note Function release memory for hash pool and hash table.
     */
    ~vf_Hash();

    /**
     * Function looks up hash entry which is identical to given hash key.
     * @param key - given hash key
     * @return Hash entry which is identical to given hash key.
     * @see vf_HashEntry_t
     */
    vf_HashEntry_t * Lookup( const char *key );

    /**
     * Function looks up hash entry which is identical to given hash key.
     * @param key - given hash key
     * @param len - hash key length
     * @return Hash entry which is identical to given hash key.
     * @see vf_HashEntry_t
     */
    vf_HashEntry_t * Lookup( const char *key, unsigned len );

    /**
     * Function creates hash entry which is identical to given hash key.
     * @param key - given hash key
     * @return Hash entry which are identical to given hash key.
     * @see vf_HashEntry_t
     * @note Created hash key and hash entry is allocated into hash memory pool.
     */
    vf_HashEntry_t * NewHashEntry( const char *key );

    /**
     * Function creates hash entry which is identical to given hash key.
     * @param key - given hash key
     * @param len - hash key length
     * @return Hash entry which are identical to given hash key.
     * @see vf_HashEntry_t
     * @note Created hash key and hash entry is allocated into hash memory pool.
     */
    vf_HashEntry_t * NewHashEntry( const char *key, size_t len );

private:
    vf_VerifyPool_t *m_pool;    ///< hash memory pool
    vf_HashEntry_t **m_hash;    ///< hash table
    const unsigned HASH_SIZE;   ///< hash table size
    bool m_free;                ///< need to free pool

    /**
     * Function checks key identity.
     * @param hash_entry - checked hash entry
     * @param key        - checked key
     * @return If keys are identical function returns <code>true</code>,
     *         else returns <code>false</code>.
     * @see vf_HashEntry_t
     */
    bool CheckKey( vf_HashEntry_t *hash_entry, const char *key );

    /**
     * Function checks key identity.
     * @param hash_entry - checked hash entry
     * @param key        - checked key
     * @param len        - checked key length
     * @return If keys are identical function returns <code>true</code>,
     *         else returns <code>false</code>.
     * @see vf_HashEntry_t
     */
    bool CheckKey( vf_HashEntry_t *hash_entry, const char *key, size_t len );

    /**
     * Hash function.
     * @param key - key for hash function
     * @return Hash index relevant to key.
     */
    unsigned HashFunc( const char *key );

    /**
     * Hash function.
     * @param key - key for hash function
     * @param len - key length
     * @return Hash index relevant to key.
     */
    unsigned HashFunc( const char *key, size_t len );
}; // struct vf_Hash

//===========================================================
// Verifier type constraint structures.
//===========================================================

/**
 * Verifier type constraint structure.
 */
struct vf_TypeConstraint {
    const char *source;         ///< constraint source class name
    const char *target;         ///< constraint target class name
    method_handler method;      ///< constraint for method
    vf_TypeConstraint_t *next;  ///< next constraint
    unsigned short index;       ///< constant pool index
    unsigned short check_type;  ///< constraint check type @see vf_CheckConstraint_t
};

/**
 * Structure of type constraint collection.
 */
struct vf_TypePool
{
public:
    /**
     * Type constraint collection constructor.
     * @note Function allocates memory for collection memory pool and hash table.
     */
    vf_TypePool();

    /**
     * Type constraint collection destructor.
     * @note Function release memory for collection memory pool and hash table.
     */
    ~vf_TypePool();

    /**
     * Function creates valid type which is identical to given class.
     * @param type - given class name
     * @param len  - class name length
     * @return Created valid type structure.
     * @see vf_ValidType_t
     */
    vf_ValidType_t * NewType( const char *type, size_t len );

    /**
     * Function creates valid type which is identical to an element of a given array type.
     * @param array - array valid type
     * @return Created valid type of a given array element.
     * @see vf_ValidType_t
     */
    vf_ValidType_t * NewArrayElemType( vf_ValidType_t *array );

    /**
     * Function checks types and create constraints if it's necessarily.
     * @param required      - required type
     * @param available     - available type
     * @param index         - constant pool index
     * @param check_type    - constraint check type
     * @return If available type satisfy to required type
     *         function will return <code>false</code> (is it error?),
     *         else will return <code>true</code> (is it error?).
     * @see vf_ValidType_t
     * @see vf_CheckConstraint_t
     */
    bool CheckTypes( vf_ValidType_t *required,
                     vf_ValidType_t *available,
                     unsigned short index,
                     vf_CheckConstraint_t check_type);

    /**
     * Function merges two valid types.
     * @param first  - first merged type
     * @param second - second merged type
     * @return Function returns created valid type corresponding with merge result.
     * @return Function returns <code>NULL</code> if vector wasn't merged.
     * @see vf_ValidType_t
     */
    vf_ValidType_t * MergeTypes( vf_ValidType_t *first, vf_ValidType_t *second );

    /**
     * Function dumps constraint collection in stream.
     * @param out - pointer to output stream
     * @note If <i>out</i> is equal to null, output stream is <i>cerr</i>.
     */
    void DumpTypeConstraints( ostream *out );

    /**
     * Function returns the methods constraints array.
     * @return Array of the methods constraints.
     * @see vf_TypeConstraint_t
     */
    vf_TypeConstraint_t * GetClassConstraint();

    /**
     * Function sets current context method.
     * @param method - current context method
     */
    void SetMethod( method_handler method );

    /**
     * Function sets restriction from target class to source class.
     * @param target        - target class name
     * @param source        - source class name
     * @param index         - constant pool index
     * @param check_type    - constraint check type
     * @see vf_CheckConstraint_t
     */
    void SetRestriction( const char *target,
                         const char *source, 
                         unsigned short index,
                         vf_CheckConstraint_t check_type );

private:
    vf_VerifyPool_t *m_pool;                ///< collection memory pool
    vf_Hash_t *m_Hash;                      ///< hash table
    method_handler m_method;                ///< current context method
    vf_TypeConstraint_t *m_restriction;     ///< array of the class constraints
}; // vf_TypePool

//===========================================================
// Verifier type constraint structures.
//===========================================================

void vf_clean_pool_memory_func( vf_VerifyPool_t *pool, VERIFY_SOURCE_PARAMS );

/**
 * Structure of verifier context
 */
struct vf_Context
{
public:
    /**
     * Verifier context constructor
     */
    vf_Context() : m_class(NULL), m_type(NULL), m_error(NULL),
        m_method(NULL), m_graph(NULL), m_pool(NULL), m_code(NULL), 
        m_codeNum(0), m_nodeNum(0), m_edgeNum(0)
    {
        vf_ContextDump zero1 = {0};
        vf_ContextVType zero2 = {0};

        m_dump = zero1;
        m_vtype = zero2;
    }

    /**
     * Verifier context destructor
     */
    ~vf_Context() {}

    /**
     * Function clears context.
     */
    void ClearContext()
    {
        m_method = NULL;
        m_is_constructor = false;
        m_graph = NULL;
        m_code = NULL;
        m_bc   = NULL;
        m_codeNum = 0;
        m_nodeNum = 0;
        m_edgeNum = 0;
        vf_clean_pool_memory( m_pool );
    } // vf_ClearContext

public:
    vf_TypePool *m_type;                ///< context type constraint collection
    char *m_error;                      ///< context error message
    vf_Graph *m_graph;                  ///< context control flow graph
    vf_VerifyPool_t *m_pool;            ///< context memory pool
    vf_Code_t *m_code;                  ///< context code instruction of method
    vf_BCode_t* m_bc;                   ///< bytecode to code mapping
    class_handler m_class;              ///< context class
    method_handler m_method;            ///< context method
    unsigned m_codeNum;                 ///< code instruction number
    unsigned m_nodeNum;                 ///< graph node number
    unsigned m_edgeNum;                 ///< graph edge number
    bool m_is_constructor;              ///< <code>true</code> if the
                                        ///< method is a constructor

    /**
     * Structure contains useful valid types
     */
    struct vf_ContextVType {
        vf_ValidType_t *m_class;        ///< context a given class valid type
        vf_ValidType_t *m_throwable;    ///< context java/lang/Throwable valid type
        vf_ValidType_t *m_object;       ///< context java/lang/Object valid type
        vf_ValidType_t *m_array;        ///< context [Ljava/lang/Object; valid type
        vf_ValidType_t *m_clone;        ///< context java/lang/Cloneable valid type
        vf_ValidType_t *m_serialize;    ///< context java/io/Serializable valid type
    } m_vtype;

    /**
     * Structure contains debug dump flags
     */
    struct vf_ContextDump {
        unsigned m_verify : 1;          ///< verify all flag
        unsigned m_with_subroutine : 1; ///< verified method has subroutine
        unsigned m_constraint : 1;      ///< dump type constraints for class
        unsigned m_code : 1;            ///< print code array in stream
        unsigned m_graph : 1;           ///< print original control flow graph
        unsigned m_mod_graph : 1;       ///< print modified control flow graph
        unsigned m_dot_graph : 1;       ///< dump original control flow graph in file in DOT format
        unsigned m_dot_mod_graph : 1;   ///< dump modified control flow graph in file in DOT format
        unsigned m_node_vector : 1;     ///< print data flow node vectors
        unsigned m_code_vector : 1;     ///< print data flow node code instruction vectors
        unsigned m_merge_vector : 1;    ///< print result of merge data flow vectors
    } m_dump;

}; // struct vf_Context

//===========================================================
// Other structures.
//===========================================================

/**
 * Head structure of memory pool.
 */
struct vf_VerifyPool {
    vf_VerifyPoolInternal_t *m_pool;    ///< pool entry
    size_t m_memory;                    ///< allocated memory size
    size_t m_used;                      ///< used memory size
    size_t m_maxuse;                    ///< max used memory size
};

/**
 * Internal structure of memory pool.
 */
struct vf_VerifyPoolInternal {
    void *m_memory;                     ///< pool entry memory
    char *m_free;                       ///< free space in pool entry
    vf_VerifyPoolInternal_t *m_next;    ///< next pool entry
    size_t m_freesize;                  ///< size of free space in pool entry
};

/**
 * Structure of class loader constraint data.
 */
struct vf_ClassLoaderData {
    vf_VerifyPool_t *pool;  ///< constraint memory pool
    vf_Hash_t *string;      ///< string pool hash table
};

//===========================================================
// Verifier debug data
//===========================================================

/**
 * Array of opcode names. Available in debug mode.
 */
#if _VERIFY_DEBUG
extern const char *vf_opcode_names[];
#endif // _VERIFY_DEBUG

//===========================================================
// Verifier functions.
//===========================================================

/**
 * Function creates bytecode control flow graph.
 * @param context - verifier context
 * @return Result of graph creation.
 * @see vf_Context_t
 * @see Verifier_Result
 */
Verifier_Result
vf_create_graph( vf_Context_t *context );

/**
 * Checks control flow and data flow of graph.
 *
 * @param[in] context a verifier context
 *
 * @return a result of graph checks
 */
Verifier_Result
vf_check_graph( vf_Context_t *context );

/**
 * Function provides data flow checks of verifier graph structure.
 * @param context - verifier context
 * @return Check result.
 * @see vf_Context_t
 * @see Verifier_Result
 */
Verifier_Result
vf_check_graph_data_flow( vf_Context_t *context );

/**
 * Function parses method, class or field descriptors.
 * @param descr  - descriptor of method, class or field
 * @param inlen  - returned number of <i>IN</i> descriptor parameters 
 * @param outlen - returned number of <i>OUT</i> descriptor parameters (for method)
 * @note Assertion is raised if <i>descr</i> or <i>inlen</i> are equal to null.
 * @note Parameter <i>outlen</i> may be equal to null (for class or field descriptor).
 */
void
vf_parse_description( const char *descr, int *inlen, int *outlen);

/**
 * Function parses descriptor and sets input and output data flow vectors.
 * @param descr     - descriptor of method, class or field
 * @param inlen     - number of entries for <i>IN</i> data flow vector
 * @param add       - additional number of entries to <i>IN</i> data flow vector
 * @param outlen    - number of entries for <i>OUT</i> data flow vector (for method)
 * @param invector  - pointer to <i>IN</i> data flow vector
 * @param outvector - pointer to <i>OUT</i> data flow vector
 * @param context   - verifier context
 * @note Assertion is raised if <i>descr</i> is equal to null.
 * @note Parameter <i>invector</i> may be equal to null 
 *       if sum of parameters <i>inlen + add</i> is equal to zero.
 * @note Parameter <i>outvector</i> may be equal to null 
 *       if parameter <i>outlen</i> is equal to zero.
 * @see vf_MapEntry_t
 * @see vf_Context_t
 */
void
vf_set_description_vector( const char *descr,
                           int inlen,
                           int add,
                           int outlen,
                           vf_MapEntry_t **invector,
                           vf_MapEntry_t **outvector,
                           vf_Context_t *context);

/**
 * Gets a class name from a constant pool.
 *
 * @param[in] klass  a handle of the class
 * @param[in] index  the entry index
 *
 * @return a pointer to UTF8 constant pool entry
 */
static inline const char*
vf_get_cp_class_name( class_handler klass,
                      unsigned short index )
{
    unsigned short class_name_index =
        class_get_cp_class_name_index(klass, index);
    const char* name = class_get_cp_utf8_bytes(klass, class_name_index);
    return name;
} // vf_get_cp_class_name

/**
 * Function creates valid type by given class name.
 * @param class_name - class name
 * @param context    - verifier context
 * @return Valid type structure corresponding to given class name.
 * @see vf_ValidType_t
 * @see vf_Context_t
 */
vf_ValidType_t *
vf_create_class_valid_type( const char *class_name, vf_Context_t *context );

/**
 * Function provides constraint checks for current class.
 * @param context - verifier context
 * @return Check result.
 * @note Provides only checks with loaded classes.
 * @note All unchecked constraints are saved to class loader verify data.
 * @see vf_Context_t
 * @see Verifier_Result
 */
Verifier_Result
vf_check_class_constraints( vf_Context_t *context );

/**
 * Function compares two valid types.
 * @param type1 - first checked type
 * @param type2 - second checked type
 * @return If types are equal returns <code>true</code>, else returns <code>false</code>.
 * @see vf_ValidType_t
 */
bool
vf_is_types_equal( vf_ValidType_t *type1, vf_ValidType_t *type2 );

/**
 * Function checks access to protected field/method.
 * If function cannot check constraint because of any class isn't loaded,
 * function return unloaded error to store restriction to the class
 * for future constraint check.
 * @param super_name    - name of super class
 * @param instance_name - name of instance class
 * @param index         - constant pool index
 * @param check_type    - access check type
 * @param ctex          - verifier context
 * @return Check result.
 * @note Provides only checks with loaded classes.
 * @see vf_CheckConstraint_t
 * @see vf_Context_t
 * @see Verifier_Result
 */
Verifier_Result
vf_check_access_constraint( const char *super_name,             // name of super class
                            const char *instance_name,          // name of instance class
                            unsigned short index,               // constant pool index
                            vf_CheckConstraint_t check_type,    // access check type
                            vf_Context_t *ctex);                // verifier context

/**
 * Sets error message of verifier.
 *
 * @param[in] stream stringstream object with a message
 * @param[in] ctex   a verifier context
 */
static inline void
vf_set_error_message( stringstream &stream,
                      vf_Context_t *ctex)
{
    if( ctex->m_error ) {
        // free old message
        vf_free( ctex->m_error );
    }
    // create message
    size_t len = stream.str().length();
    if( len ) {
        ctex->m_error = (char*)vf_malloc( len + 1 );
        memcpy( ctex->m_error, stream.str().c_str(), len );
        ctex->m_error[len] = '\0';
    } else {
        ctex->m_error = NULL;
    }
    return;
} // vf_set_error_message

/**
 * Checks a version of a class file.
 *
 * @param[in] context a verifier context
 *
 * @return <code>true</code> if a class version is less than 1.4
 */
static inline bool
vf_is_class_version_14( vf_Context_t *context )
{
    return (class_get_version(context->m_class) < 49) ? true : false;
} // vf_is_class_version_14

/**
 * Returns branch target for a given branch number.
 *
 * @param[in] code        a reference to instruction
 * @param[in] branch_num  a branch number
 *
 * @return an absolute bytecode position to which the executuion
 * branches
 */
static inline int
vf_get_code_branch( vf_Code_t* code, unsigned branch_num )
{
    assert(branch_num < code->m_offcount);
    return code->m_off[branch_num];
} // vf_get_instruction_branch

/**
 * Allocates memory for a new stack map vector.
 *
 * @param[out] vector a reference to vector
 * @param[in]  len    vector length
 * @param[in]  pool   memory pool
 */
static inline void 
vf_new_vector( vf_MapEntry_t** vector,
               unsigned len,
               vf_VerifyPool_t *pool)
{
    // create new vector
    (*vector) = (vf_MapEntry_t*) vf_alloc_pool_memory(pool, 
        len * sizeof(vf_MapEntry_t));
    return;
} // vf_new_vector

/**
 * Sets a reference data type for a given stack map vector entry.
 *
 * @param[in, out] vector a reference to a stack map
 * @param[in]      num    stack map index
 * @param[in]      type   reference type
 */
static inline void
vf_set_vector_stack_entry_ref( vf_MapEntry_t* vector,
                               unsigned num,
                               vf_ValidType_t* type)
{
    // set a stack map vector entry by ref
    vector[num].m_type = SM_REF;
    vector[num].m_vtype = type;
} // vf_set_vector_stack_entry_ref

/**
 * Calls destructor of graph, if any.
 *
 * @param[in] context a context of verifier
 */
void vf_free_graph( vf_Context_t* context );

/**
 * Inline subroutines in the call graph.
 *
 * <p>Here is the algorithm sketch: we define subroutine boundaries with
 * simplified data flow analysis and duplicate subroutine nodes
 * in the call graph. For more details check a
 * <a href="http://wiki.apache.org/harmony/Subroutine_Verification">wiki
 * page</a>.</p>
 *
 * @param[in] context a verifier context
 * @return VER_OK if subroutines were inlined successfully,
 * an error code otherwise
 */
Verifier_Result
vf_inline_subroutines(vf_ContextHandle context);

#endif // _VERIFIER_REAL_H_
