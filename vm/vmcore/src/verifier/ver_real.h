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
 * Defines a debug mode for verifier.
 */
#ifdef NDEBUG
#define _VF_DEBUG 0
#else // NDEBUG
#define _VF_DEBUG 1
#endif // NDEBUG

/**
 * Both debug and trace channels are available only at debug mode.
 * A debug channel is visible for a user while a trace channel should be
 * enabled via <code>-Xtrace:verifier</code> command line option.
 */
#if _VF_DEBUG
#define VF_DEBUG( info ) ECHO( "vf_debug: " << info )
#define VF_DUMP( category, expr ) { \
    static LogSite logSite = { UNKNOWN, NULL }; \
    if( logSite.state && is_trace_enabled( "vfdump." category, &logSite ) ) { \
        expr; \
    } \
}
#define VF_STOP assert(0)
#else
#define VF_DEBUG(info)
#define VF_DUMP(category, expr)
#define VF_STOP DIE("Aborted at " __FILE__ "(" << __LINE__ << ")")
#endif // _VF_DEBUG

#define VF_TRACE(comp, message) TRACE2("verifier." comp, message)
/**
 * Verifer dump domains. They can be specified in a command line in
 * a following form <code>-Xtrace:vfdump.&lt;domain&gt;</code>.
 */
/** Type constraints for a class. */
#define DUMP_CONSTRAINT "constraint"
/** An instruction array. */
#define DUMP_INSTR "instr"
/** A control flow graph. */
#define DUMP_GRAPH "graph"
/** A graph with cleaned dead nodes and inlined subroutines. */
#define DUMP_MOD "mod_graph"
/** A graph in a dot format. */
#define DUMP_DOT "dot"
/** A modified graph in a dot format. */
#define DUMP_DOT_MOD "mod_dot"
/** Node map vectors. */
#define DUMP_NODE_MAP "map.node"
/** Instruction map vectors. */
#define DUMP_INSTR_MAP "map.instr"
/** Merged map vectors. */
#define DUMP_MERGE_MAP "map.merge"
/** A path to subroutine <code>ret</code> instruction. */
#define DUMP_NODESTACK "node.stack"


/** Store a verification error in a verification context. */
#define VF_SET_CTX( ctx, error_message ) \
{ \
    stringstream stream; \
    stream << error_message; \
    vf_set_error_message( stream, ( ctx ) ); \
}

/** Dump a current class and method. */
#define VF_REPORT_CTX( ctx ) \
    "(class: " << class_get_name( (ctx)->m_class )              \
    << ", method: " << (ctx)->m_name << (ctx)->m_descriptor     \
    << ") "

/** Report a verification error. */
#define VF_REPORT( ctx, error_message ) \
    VF_SET_CTX( ctx, VF_REPORT_CTX( ctx ) << error_message )

/** Abort a verifier abnormally. */
#define VF_DIE( error_message ) \
    VF_DEBUG( "Verifer aborted: " << error_message ); \
    VF_STOP;

/**
 * Define source code line and source file name parameters and arguments.
 */
#if _VF_DEBUG
#define VF_SOURCE_PARAMS                UNREF int line_, UNREF const char *file_
#define VF_SOURCE_ARGS0                 __LINE__, __FILE__
#define VF_SOURCE_ARGS1                 line_, file_
#define VF_REPORT_SOURCE                file_ << "(" << line_ << "): "
#else // _VF_DEBUG
#define VF_SOURCE_PARAMS                int
#define VF_SOURCE_ARGS0                 0
#define VF_SOURCE_ARGS1                 0
#define VF_REPORT_SOURCE                ""
#endif // _VF_DEBUG

//===========================================================
// Verifier contant pool checks
//===========================================================

/**
 * Constant pool checks.
 */
// for handler id = 0 is legal value
#define CHECK_HANDLER_CONST_POOL_ID( id, len, ctx ) \
    if( (id) >= (len) ) { \
        VF_REPORT( ctx, "Illegal constant pool index in handler" ); \
        return VER_ErrorHandler; \
    }
// for handler id = 0 is legal value
#define CHECK_HANDLER_CONST_POOL_CLASS( ctx, id ) \
    if( (id) && class_get_cp_tag( (ctx)->m_class, (id) ) != _CONSTANT_Class ) { \
        VF_REPORT( ctx, "Illegal type in constant pool for handler, " \
            << id << ": CONSTANT_Class is expected" ); \
        return VER_ErrorHandler; \
    }
#define CHECK_CONST_POOL_ID( id, len, ctx ) \
    if( !(id) || (id) >= (len) ) { \
        VF_REPORT( ctx, "Illegal constant pool index" ); \
        return VER_ErrorConstantPool; \
    }
#define CHECK_CONST_POOL_CLASS( ctx, id ) \
    if( class_get_cp_tag( (ctx)->m_class, (id) ) != _CONSTANT_Class ) { \
        VF_REPORT( ctx, "Illegal type in constant pool, " \
            << id << ": CONSTANT_Class is expected" ); \
        return VER_ErrorConstantPool; \
    }
#define CHECK_CONST_POOL_METHOD( ctx, id ) \
    if( class_get_cp_tag( (ctx)->m_class, (id) ) != _CONSTANT_Methodref ) { \
        VF_REPORT( ctx, "Illegal type in constant pool, " \
            << id << ": CONSTANT_Methodref is expected" ); \
        return VER_ErrorConstantPool; \
    }
#define CHECK_CONST_POOL_INTERFACE( ctx, id ) \
    if( class_get_cp_tag( (ctx)->m_class, (id) ) != _CONSTANT_InterfaceMethodref ) { \
        VF_REPORT( ctx, "Illegal type in constant pool, " \
            << id << ": CONSTANT_InterfaceMethodref is expected" ); \
        return VER_ErrorConstantPool; \
    }
#define CHECK_CONST_POOL_FIELD( ctx, id ) \
    if( class_get_cp_tag( (ctx)->m_class, (id) ) != _CONSTANT_Fieldref ) { \
        VF_REPORT( ctx, "Illegal type in constant pool, " \
            << id << ": CONSTANT_Fieldref is expected" ); \
        return VER_ErrorConstantPool; \
    }
#define CHECK_CONST_POOL_TYPE( ctx, id ) \
    if( class_get_cp_tag( (ctx)->m_class, (id) ) != _CONSTANT_NameAndType ) { \
        VF_REPORT( ctx, "Illegal type in constant pool, " \
            << id << ": CONSTANT_NameAndType is expected" ); \
        return VER_ErrorConstantPool; \
    }
#define CHECK_CONST_POOL_STRING( ctx, id ) \
    if( class_get_cp_tag( (ctx)->m_class, (id) ) != _CONSTANT_String ) { \
        VF_REPORT( ctx, "Illegal type in constant pool, " \
            << id << ": CONSTANT_String is expected" ); \
        return VER_ErrorConstantPool; \
    }
#define CHECK_CONST_POOL_UTF8( ctx, id ) \
    if( class_get_cp_tag( (ctx)->m_class, (id) ) != _CONSTANT_Utf8) { \
        VF_REPORT( ctx, "Illegal type in constant pool, " \
            << id << ": CONSTANT_Utf8 is expected" ); \
        return VER_ErrorConstantPool; \
    }

//===========================================================
// Verifier enums
//===========================================================

/**
 * Verifier error codes.
 */
typedef Verifier_Result vf_Result;

/**
 * Constraint check types enum.
 */
typedef enum
{
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
} vf_CheckConstraint;

/** 
 * Stack map entry types enum.
 */
typedef enum
{
    // original types
    SM_TOP = 0,
    SM_INT,
    SM_FLOAT,
    SM_NULL,
    SM_REF,
    SM_UNINITIALIZED,
    SM_LONG_LO,
    SM_DOUBLE_LO,
    SM_LONG_HI,
    SM_DOUBLE_HI,
    SM_RETURN_ADDR,
    // additional types
    SM_ANY,                     // matches any value
    SM_COPY_0,
    SM_COPY_1,
    SM_COPY_2,
    SM_COPY_3,
    SM_UP_ARRAY,
    SM_WORD,
    SM_WORD2_LO,
    SM_WORD2_HI,
    SM_NUMBER
} vf_MapType;

/**
 * Each instruction has a descriptive bitmap.
 */
typedef enum
{
    VF_INSTR_NONE = 0,
    VF_INSTR_RETURN = 1,
    VF_INSTR_THROW = 2,
    VF_INSTR_JSR = 3,
    VF_INSTR_RET = 4
} vf_InstrType;

//===========================================================
// Verifier predefined structures
//===========================================================

/**
 * @ingroup Handles
 * A handle of a verifier context.
 */
typedef const struct vf_Context *vf_ContextHandle;
/**
 * @ingroup Handles
 * A handle of a subroutine information.
 */
typedef const struct vf_SubContext *vf_SubContextHandle;
/**
 * @ingroup Handles
 * A graph handle.
 */
typedef const class vf_Graph *vf_GraphHandle;
/**
 * @ingroup Handles
 * A handle of a graph node.
 */
typedef const struct vf_Node *vf_NodeHandle;
/**
 * @ingroup Handles
 * A map vector handle.
 */
typedef const struct vf_MapVector *vf_MapVectorHandle;
/**
 * @ingroup Handles
 * A handle of an instruction.
 */
typedef const struct vf_Instr *vf_InstrHandle;

//===========================================================
// Code instruction structures.
//===========================================================

/**
 * Valid types structure.
 */
struct vf_ValidType
{
    unsigned number;            ///< number of types
    const char *string[1];      ///< type array
};

/** 
 * Stack map entry structure.
 */
struct vf_MapEntry
{
    vf_ValidType *m_vtype;      ///< valid type for reference
    union
    {
        unsigned m_new;         ///< program count of opcode new for uninitialized
        unsigned m_pc;          ///< program count of return address for subroutine
    };
    union
    {
        unsigned short m_local; ///< number of local variable
        unsigned short m_index; ///< constant pool index for access check
    };
    unsigned short m_type:5;    ///< stack map type @see vf_MapType
    unsigned short m_ctype:4;   ///< constraint type @see vf_CheckConstraint
    unsigned short m_is_local:1;        ///< local variable modify flag
    ///< true - modify local, false - modify stack
};

/**
 * A bytecode instruction.
 */
struct vf_Instr
{
    unsigned char *m_addr;      ///< a start of a bytecode instruction
    unsigned *m_off;            ///< an array of instruction branches
    vf_MapEntry *m_invector;    ///< a stack map IN instruction vector
    vf_MapEntry *m_outvector;   ///< a stack map OUT instruction vector
    unsigned m_offcount;        ///< a number of instruction branches
    union
    {
        vf_NodeHandle m_node;   ///< contains a node handle if the corresponding
        ///< basic block starts at the instruction, <code>NULL</code> otherwise
        bool m_is_bb_start;     ///< if this is a begin of a basic block
    };
    unsigned short m_minstack;  ///< a minimal stack for instruction
    unsigned short m_inlen;     ///< an IN stack vector length
    unsigned short m_outlen;    ///< an OUT stack vector length
    short m_stack;              ///< a stack change
    vf_InstrType m_type;        ///< an instruction type @see vf_InstrType
};

/**
 * A byte of a bytecode.
 */
struct vf_BCode
{
    vf_InstrHandle m_instr;     ///< a pointer to vf_Context.m_instr
    bool m_is_bb_start:1;       ///< <code>true</code> if the block starts here
};

/** 
 * Constant pool parse structure.
 */
union vf_Parse
{
    struct
    {
        vf_MapEntry *m_invector;        ///< method IN stack vector
        vf_MapEntry *m_outvector;       ///< method OUT stack vector
        const char *m_name;     ///< invoked method name
        unsigned short m_inlen; ///< IN vector length
        unsigned short m_outlen;        ///< OUT vector length
    } method;
    struct
    {
        vf_MapEntry *f_vector;  ///< field type map vector
        unsigned short f_vlen;  ///< field type map vector len
    } field;
};

//===========================================================
// Pool structures.
//===========================================================

/**
 * Internal structure of memory pool.
 */
struct vf_PoolInternal
{
    void *m_memory;             ///< pool entry memory
    char *m_free;               ///< free space in pool entry
    vf_PoolInternal *m_next;    ///< next pool entry
    size_t m_freesize;          ///< size of free space in pool entry
};

/**
 * Head structure of memory pool.
 */
struct vf_Pool
{
    vf_PoolInternal *m_pool;    ///< pool entry
    size_t m_memory;            ///< allocated memory size
    size_t m_used;              ///< used memory size
    size_t m_maxuse;            ///< max used memory size
};

//===========================================================
// Verifier memory management functions diclarations
//===========================================================

/**
 * Memory pool entry size.
 */
#define VF_POOL_ENTRY_SIZE  0x2000

/**
 * Macros hide source line and source file name function arguments.
 */
#define vf_calloc(number, size_element) \
    vf_calloc_func((number), (size_element), VF_SOURCE_ARGS0)
#define vf_malloc(size) \
    vf_malloc_func((size), VF_SOURCE_ARGS0)
#define vf_free(pointer) \
    vf_free_func((pointer), VF_SOURCE_ARGS0)
#define vf_realloc(pointer, resize) \
    vf_realloc_func((pointer), (resize), VF_SOURCE_ARGS0)
#define vf_create_pool() \
    vf_create_pool_func(VF_SOURCE_ARGS0)
#define vf_palloc(pool, size) \
    vf_palloc_func((pool), (size), VF_SOURCE_ARGS0)
#define vf_clean_pool(pool) \
    vf_clean_pool_func((pool), VF_SOURCE_ARGS0)
#define vf_delete_pool(pool) \
    vf_delete_pool_func((pool), VF_SOURCE_ARGS0)

/**
 * Function allocates an array in memory with elements initialized to zero. 
 * @param number        - number of elements
 * @param element_size  - size of element
 * @note <i>VF_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @return Pointer to allocated memory block.
 * @see VF_SOURCE_PARAMS
 * @see vf_calloc
 * @note Assertion is raised if <i>number</i> or <i>element_size</i>
 *       are equal to zero or if out of memory error is arisen.
 * @note Trace is available with argument <b>verifier:memory</b>.
 */
void *vf_calloc_func( unsigned number, size_t element_size,
                      VF_SOURCE_PARAMS );

/**
 * Function allocates memory blocks.
 * @param size - size of memory block
 * @note <i>VF_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @return Pointer to allocated memory block.
 * @see VF_SOURCE_PARAMS
 * @see vf_malloc
 * @note Assertion is raised if <i>size</i> is equal to zero
 *       or if out of memory error is arisen.
 * @note Trace is available with argument <b>verifier:memory</b>.
 */
void *vf_malloc_func( size_t size, VF_SOURCE_PARAMS );

/**
 * Function releases allocated memory blocks.
 * @param pointer - pointer to allocated block
 * @note <i>VF_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @see VF_SOURCE_PARAMS
 * @see vf_free
 * @note Assertion is raised if <i>pointer</i> is equal to null.
 * @note Trace is available with argument <b>verifier:memory</b>.
 */
void vf_free_func( void *pointer, VF_SOURCE_PARAMS );

/**
 * Function reallocates memory blocks.
 * @param pointer   - pointer to allocated block
 * @param resize    - new size of memory block
 * @note <i>VF_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @return Pointer to reallocated memory block.
 * @see VF_SOURCE_PARAMS
 * @see vf_realloc
 * @note Assertion is raised if <i>resize</i> is equal to zero
 *       or if out of memory error is arisen.
 * @note If <i>pointer</i> is equal to null function works like vf_malloc_func.
 * @note Trace is available with argument <b>verifier:memory</b>.
 */
void *vf_realloc_func( void *pointer, size_t resize, VF_SOURCE_PARAMS );

/**
 * Function creates memory pool structure.
 * @note <i>VF_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @return Pointer to created memory pool.
 * @see VF_SOURCE_PARAMS
 * @see vf_create_pool
 * @see vf_Pool
 * @note Trace is available with argument <b>verifier:memory:pool</b>.
 */
vf_Pool *vf_create_pool_func( VF_SOURCE_PARAMS );

/**
 * Function allocates memory blocks in current pool.
 * @param pool - pointer to memory pool structure
 * @param size - memory block size
 * @note <i>VF_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @return Pointer to allocated memory block.
 * @see VF_SOURCE_PARAMS
 * @see vf_palloc
 * @see vf_Pool
 * @note Trace is available with argument <b>verifier:memory:pool</b>.
 */
void *vf_palloc_func( vf_Pool *pool, size_t size, VF_SOURCE_PARAMS );

/**
 * Function cleans given pool.
 * @param pool - memory pool structure
 * @note <i>VF_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @see VF_SOURCE_PARAMS
 * @see vf_clean_pool
 * @see vf_Pool
 * @note Trace is available with argument <b>verifier:memory:pool</b>.
 */
void vf_clean_pool_func( vf_Pool *pool, VF_SOURCE_PARAMS );

/**
 * Function releases memory from given pool.
 * @param pool - memory pool structure
 * @note <i>VF_SOURCE_PARAMS</i> - debug parameters source line and source file name
 * @see VF_SOURCE_PARAMS
 * @see vf_delete_pool
 * @see vf_Pool
 * @note Trace is available with argument <b>verifier:memory:pool</b>.
 */
void vf_delete_pool_func( vf_Pool *pool, VF_SOURCE_PARAMS );

//===========================================================
// Verifier hash table structures.
//===========================================================

/**
 * Structure of hash entry.
 */
struct vf_HashEntry
{
    const char *key;            ///< hash entry key
    void *data;                 ///< pointer to hash entry data
    vf_HashEntry *next;         ///< next hash entry
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
    vf_Hash ();

    /**
     * Hash table constructor.
     * @param pool - external memory pool
     * @note Function allocates memory for hash pool and hash table.
     */
    vf_Hash ( vf_Pool *pool );

    /**
     * Hash table destructor.
     * @note Function release memory for hash pool and hash table.
     */
           ~vf_Hash ();

    /**
     * Function looks up hash entry which is identical to given hash key.
     * @param key - given hash key
     * @return Hash entry which is identical to given hash key.
     * @see vf_HashEntry
     */
    vf_HashEntry *Lookup( const char *key );

    /**
     * Function looks up hash entry which is identical to given hash key.
     * @param key - given hash key
     * @param len - hash key length
     * @return Hash entry which is identical to given hash key.
     * @see vf_HashEntry
     */
    vf_HashEntry *Lookup( const char *key, size_t len );

    /**
     * Function creates hash entry which is identical to given hash key.
     * @param key - given hash key
     * @return Hash entry which are identical to given hash key.
     * @see vf_HashEntry
     * @note Created hash key and hash entry is allocated into hash memory pool.
     */
    vf_HashEntry *NewHashEntry( const char *key );

    /**
     * Function creates hash entry which is identical to given hash key.
     * @param key - given hash key
     * @param len - hash key length
     * @return Hash entry which are identical to given hash key.
     * @see vf_HashEntry
     * @note Created hash key and hash entry is allocated into hash memory pool.
     */
    vf_HashEntry *NewHashEntry( const char *key, size_t len );

  private:
    vf_Pool *m_pool;            ///< hash memory pool
    vf_HashEntry **m_hash;      ///< hash table
    const unsigned HASH_SIZE;   ///< hash table size
    bool m_free;                ///< need to free pool

    /**
     * Checks key identity.
     * @param hash_entry - checked hash entry
     * @param key        - checked key
     * @return If keys are identical function returns <code>true</code>,
     *         else returns <code>false</code>.
     * @see vf_HashEntry
     */
    bool CheckKey( vf_HashEntry *hash_entry, const char *key );

    /**
     * Checks key identity.
     * @param hash_entry - checked hash entry
     * @param key        - checked key
     * @param len        - checked key length
     * @return If keys are identical function returns <code>true</code>,
     *         else returns <code>false</code>.
     * @see vf_HashEntry
     */
    bool CheckKey( vf_HashEntry *hash_entry, const char *key, size_t len );

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
};                              // struct vf_Hash

//===========================================================
// Verifier type constraint structures.
//===========================================================

/**
 * A list of constraints.
 */
struct vf_TypeConstraint
{
    const char *m_source;           ///< constraint source class name
    const char *m_target;           ///< constraint target class name
    method_handler m_method;        ///< constraint for method
    const char *m_name;             ///< constraint method name
    const char *m_descriptor;       ///< constraint method descriptor
    vf_TypeConstraint *m_next;      ///< next constraint
    unsigned short m_index;         ///< constant pool index
    unsigned short m_check_type;    ///< constraint check type @see vf_CheckConstraint
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
    vf_TypePool ();

    /**
     * Type constraint collection destructor.
     * @note Function release memory for collection memory pool and hash table.
     */
    ~vf_TypePool ();

    /**
     * Function creates valid type which is identical to given class.
     * @param type - given class name
     * @param len  - class name length
     * @return Created valid type structure.
     * @see vf_ValidType
     */
    vf_ValidType *NewType( const char *type, size_t len );

    /**
     * Function creates valid type which is identical to an element of a given array type.
     * @param array - array valid type
     * @return Created valid type of a given array element.
     * @see vf_ValidType
     */
    vf_ValidType *NewArrayElemType( vf_ValidType *array );

    /**
     * Checks types and create constraints if it's necessarily.
     * @param required      - required type
     * @param available     - available type
     * @param index         - constant pool index
     * @param check_type    - constraint check type
     * @return If available type satisfy to required type
     *         function will return <code>false</code> (is it error?),
     *         else will return <code>true</code> (is it error?).
     * @see vf_ValidType
     * @see vf_CheckConstraint
     */
    bool CheckTypes( vf_ValidType *required,
                     vf_ValidType *available,
                     unsigned short index, vf_CheckConstraint check_type );

    /**
     * Function merges two valid types.
     * @param first  - first merged type
     * @param second - second merged type
     * @return Function returns created valid type corresponding with merge result.
     * @return Function returns <code>NULL</code> if vector wasn't merged.
     * @see vf_ValidType
     */
    vf_ValidType *MergeTypes( vf_ValidType *first, vf_ValidType *second );

    /**
     * Dumps constraint collection in stream.
     * @param out - pointer to output stream
     * @note If <i>out</i> is equal to null, output stream is <i>cerr</i>.
     */
    void DumpTypeConstraints( ostream *out );

    /**
     * Function returns the methods constraints array.
     * @return Array of the methods constraints.
     * @see vf_TypeConstraint
     */
    vf_TypeConstraint *GetClassConstraint();

    /**
     * Sets current context method.
     * @param ctx - current verifier context
     */
    void SetMethod( vf_ContextHandle ctx );

    /**
     * Sets restriction from target class to source class.
     * @param target        - target class name
     * @param source        - source class name
     * @param index         - constant pool index
     * @param check_type    - constraint check type
     * @see vf_CheckConstraint
     */
    void SetRestriction( const char *target,
                         const char *source,
                         unsigned short index,
                         vf_CheckConstraint check_type );

  private:
    vf_Pool *m_pool;            ///< collection memory pool
    vf_Hash *m_Hash;            ///< hash table
    method_handler m_method;    ///< current context method
    const char *m_name;         ///< current context method name
    const char *m_descriptor;   ///< current context method descriptor
    vf_TypeConstraint *m_restriction;   ///< array of the class constraints
};                              // vf_TypePool

//===========================================================
// Verifier type constraint structures.
//===========================================================

void vf_clean_pool_func( vf_Pool *pool, VF_SOURCE_PARAMS );

/**
 * Verification context.
 */
struct vf_Context
{
  public:
    /**
     * Verifier context constructor
     */
    vf_Context ():m_class( NULL ), m_type( NULL ), m_error( NULL ),
        m_method( NULL ), m_name(NULL), m_descriptor(NULL), m_graph( NULL ),
        m_pool( NULL ), m_instr( NULL ), m_last_instr( NULL ), m_retnum( 0 ),
        m_verify_all( false )
    {
        vf_ContextVType zero2 = { 0 };
        m_vtype = zero2;
    }

    void SetMethod( method_handler method )
    {
        assert(method);
        m_method = method;
        m_name = method_get_name(method);
        m_descriptor = method_get_descriptor(method);

        // get method parameters
        m_len = method_get_code_length( method );
        m_bytes = method_get_bytecode( method );
        m_handlers = method_get_exc_handler_number( method );

        // get method limitations
        m_maxlocal = method_get_max_local( method );
        m_maxstack = method_get_max_stack( method );

        // cache in the context if the method is a constructor
        m_is_constructor = (memcmp( m_name, "<init>", 7 ) == 0);
    }

    /**
     * Verifier context destructor
     */
    ~vf_Context ()
    {
        if( m_pool ) {
            vf_delete_pool( m_pool );
        }
        if( m_type ) {
            delete m_type;
        }
    }

    /**
     * Clears context.
     */
    void ClearContext()
    {
        m_method = NULL;
        m_name = NULL;
        m_descriptor = NULL;
        m_is_constructor = false;
        m_graph = NULL;
        m_instr = NULL;
        m_bc = NULL;
        m_last_instr = NULL;
        m_retnum = 0;
        vf_clean_pool( m_pool );
    }                           // vf_ClearContext

  public:
    vf_TypePool *m_type;        ///< context type constraint collection
    char *m_error;              ///< context error message
    vf_Graph *m_graph;          ///< control flow graph
    vf_Pool *m_pool;            ///< memory pool
    vf_InstrHandle m_instr;     ///< method instructions
    vf_InstrHandle m_last_instr;        ///< the pointer follows
    ///< the last instruction at <code>m_instr</code>
    vf_BCode *m_bc;             ///< bytecode to instruction mapping
    unsigned m_retnum;          ///< a number of <code>ret</code>s

    // Cached method info.
    class_handler m_class;      ///< a context class
    method_handler m_method;    ///< a context method
    const char *m_name;         ///< a context method name
    const char *m_descriptor;   ///< a context method descriptor
    unsigned m_len;             ///< bytecode length
    unsigned char *m_bytes;     ///< bytecode location
    unsigned short m_handlers;  ///< a number of exception handlers
    unsigned short m_maxstack;  ///< max stack length
    unsigned short m_maxlocal;  ///< max local number
    bool m_is_constructor;      ///< <code>true</code> if the
                                ///< method is a constructor

    // Subrotine info
    vf_SubContext *m_sub_ctx;           ///< aggregate subroutine info
    vf_MapVector *m_map;                ///< a stack map for control flow
                                        ///< analysis, vectors themselves are
                                        ///< allocated from the graph pool
    vf_MapEntry *m_method_invector;     ///< method parameters
    unsigned short m_method_inlen;      ///< a length of <code>m_method_invector</code>
    vf_MapEntry *m_method_outvector;    ///< method return value
    unsigned short m_method_outlen;     ///< a length of <code>m_method_outvector</code>

    // Data flow analisys info
    vf_MapEntry *m_buf;         ///< used to store intermediate stack states
                                ///< during data flow analysis

    bool m_verify_all;          ///< if <code>true</code> need to verify more checks

    /**
     * Contains useful valid types.
     */
    struct vf_ContextVType
    {
        vf_ValidType *m_class;          ///< a given class
        vf_ValidType *m_throwable;      ///< java/lang/Throwable
        vf_ValidType *m_object;         ///< java/lang/Object
        vf_ValidType *m_array;          ///< [Ljava/lang/Object;
        vf_ValidType *m_clone;          ///< java/lang/Cloneable
        vf_ValidType *m_serialize;      ///< java/io/Serializable
    } m_vtype;

};                              // struct vf_Context

//===========================================================
// Other structures.
//===========================================================

/**
 * Structure of class loader constraint data.
 */
struct vf_ClassLoaderData
{
    vf_Pool *pool;              ///< constraint memory pool
    vf_Hash *string;            ///< string pool hash table
};


//===========================================================
// Verifier debug data
//===========================================================

/**
 * Array of opcode names. Available in debug mode.
 */
#if _VF_DEBUG
extern const char *vf_opcode_names[];
#endif // _VF_DEBUG

//===========================================================
// Verifier functions.
//===========================================================

/**
 * Function creates bytecode control flow graph.
 * @param ctx - verifier context
 * @return Result of graph creation.
 * @see vf_Context
 * @see vf_Result
 */
vf_Result vf_create_graph( vf_Context *ctx );

/**
 * Checks control flow and data flow of graph.
 *
 * @param[in] context a verifier context
 *
 * @return a result of graph checks
 */
vf_Result vf_check_graph( vf_Context *ctx );

/**
 * Provides data flow checks of verifier graph structure.
 * @param ctx - verifier context
 * @return Check result.
 * @see vf_Context
 * @see vf_Result
 */
vf_Result vf_check_graph_data_flow( vf_Context *ctx );

/**
 * Parses method, class or field descriptors.
 * @param descr  descriptor of method, class or field
 * @param inlen  returned number of <i>IN</i> descriptor parameters 
 * @param outlen returned number of <i>OUT</i> descriptor parameters (for method)
 * @note Assertion is raised if <i>descr</i> or <i>inlen</i> are equal to <code>NULL</code>.
 * @note Parameter <i>outlen</i> may be equal to null (for class or field descriptor).
 */
void vf_parse_description( const char *descr, unsigned short *inlen,
                           unsigned short *outlen );

/**
 * Parses a descriptor and sets input and output data flow vectors.
 * @param descr     a descriptor of method, class or field
 * @param inlen     a number of entries for <i>IN</i> data flow vector
 * @param add       an additional number of entries to <i>IN</i> data flow vector
 * @param outlen    a number of entries for <i>OUT</i> data flow vector (for method)
 * @param invector  a pointer to <i>IN</i> data flow vector
 * @param outvector a pointer to <i>OUT</i> data flow vector
 * @param ctx   a verifier context
 * @note Assertion is raised if <i>descr</i> is equal to null.
 * @note Parameter <i>invector</i> may be equal to null 
 *       if sum of parameters <i>inlen + add</i> is equal to zero.
 * @note Parameter <i>outvector</i> may be equal to null 
 *       if parameter <i>outlen</i> is equal to zero.
 */
void
vf_set_description_vector( const char *descr,
                           unsigned short inlen,
                           unsigned short add,
                           unsigned short outlen,
                           vf_MapEntry **invector,
                           vf_MapEntry **outvector, vf_ContextHandle ctx );

/**
 * Gets a class name from a constant pool.
 *
 * @param[in] klass  a handle of the class
 * @param[in] index  the entry index
 *
 * @return a pointer to UTF8 constant pool entry
 */
static inline const char *
vf_get_cp_class_name( class_handler klass, unsigned short index )
{
    unsigned short class_name_index =
        class_get_cp_class_name_index( klass, index );
    const char *name = class_get_cp_utf8_bytes( klass, class_name_index );
    return name;
}                               // vf_get_cp_class_name

/**
 * Creates a valid type from a given class name.
 * @param class_name a class name
 * @param ctx    a verifier context
 * @return a valid type structure corresponding to the given class name
 */
vf_ValidType *vf_create_class_valid_type( const char *class_name,
                                          vf_ContextHandle ctx );

/**
 * Provides constraint checks for current class.
 * @param ctx - verifier context
 * @return Check result.
 * @note Provides only checks with loaded classes.
 * @note All unchecked constraints are saved to class loader verify data.
 * @see vf_Context
 * @see vf_Result
 */
vf_Result vf_check_class_constraints( vf_Context *ctx );

/**
 * Function compares two valid types.
 * @param type1 - first checked type
 * @param type2 - second checked type
 * @return If types are equal returns <code>true</code>, else returns <code>false</code>.
 * @see vf_ValidType
 */
bool vf_is_types_equal( vf_ValidType *type1, vf_ValidType *type2 );

/**
 * Checks access to protected field/method.
 * If function cannot check constraint because of any class isn't loaded,
 * function return unloaded error to store restriction to the class
 * for future constraint check.
 * @param super_name    - name of super class
 * @param instance_name - name of instance class
 * @param index         - constant pool index
 * @param check_type    - access check type
 * @param ctx          - verifier context
 * @return Check result.
 * @note Provides only checks with loaded classes.
 * @see vf_CheckConstraint
 * @see vf_Context
 * @see vf_Result
 */
vf_Result vf_check_access_constraint( const char *super_name,   // name of super class
                                      const char *instance_name,        // name of instance class
                                      unsigned short index,     // constant pool index
                                      vf_CheckConstraint check_type,    // access check type
                                      vf_Context *ctx );        // verification context

/**
 * Sets error message of a verifier.
 *
 * @param[in] stream stringstream object with a message
 * @param[in] ctx   a verifier context
 */
static inline void
vf_set_error_message( stringstream & stream, vf_Context *ctx )
{
    if( ctx->m_error ) {
        // free old message
        vf_free( ctx->m_error );
    }
    // create message
    size_t len = stream.str().length();
    if( len ) {
        ctx->m_error = (char*)vf_malloc( len + 1 );
        memcpy( ctx->m_error, stream.str().c_str(), len );
        ctx->m_error[len] = '\0';
    } else {
        ctx->m_error = NULL;
    }
}                               // vf_set_error_message

/**
 * Checks a version of a class file.
 *
 * @param[in] context a verifier context
 *
 * @return <code>true</code> if a class version is less than 1.4
 */
static inline bool
vf_is_class_version_14( vf_ContextHandle ctx )
{
    return ( class_get_version( ctx->m_class ) < 49 ) ? true : false;
}                               // vf_is_class_version_14

/**
 * Returns branch target for a given branch number.
 *
 * @param[in] instr        a reference to instruction
 * @param[in] branch_num  a branch number
 *
 * @return an absolute bytecode position to which the executuion
 * branches
 */
static inline int
vf_get_instr_branch( vf_InstrHandle instr, unsigned branch_num )
{
    assert( branch_num < instr->m_offcount );
    return instr->m_off[branch_num];
}                               // vf_get_instruction_branch

/**
 * Allocates memory for a new stack map vector.
 *
 * @param[out] vector a reference to vector
 * @param[in]  len    vector length
 * @param[in]  pool   memory pool
 */
static inline void
vf_new_vector( vf_MapEntry **vector, unsigned len, vf_Pool *pool )
{
    // create new vector
    ( *vector ) =    (vf_MapEntry*)vf_palloc( pool,
                                              len * sizeof( vf_MapEntry ) );
}                               // vf_new_vector

/**
 * Sets a reference data type for a given stack map vector entry.
 *
 * @param[in, out] vector a reference to a stack map
 * @param[in]      num    stack map index
 * @param[in]      type   reference type
 */
static inline void
vf_set_vector_stack_entry_ref( vf_MapEntry *vector,
                               unsigned num, vf_ValidType *type )
{
    // set a stack map vector entry by ref
    vector[num].m_type = SM_REF;
    vector[num].m_vtype = type;
}                               // vf_set_vector_stack_entry_ref

/**
 * Gets an instruction index.
 * @param[in] instr an instruction handle
 * @param[in] ctx a verification context
 * @return    an instruction index
 */
static inline unsigned
vf_get_instr_index( vf_InstrHandle instr, vf_ContextHandle ctx )
{
    return ( unsigned )( instr - ctx->m_instr );
}

/**
 * Calculates an index of instruction.
 * @param[in] pc  a bytecode index at instruction start
 * @param[in] ctx a verification context
 * @return    an instruction index
 */
static inline unsigned
vf_bc_to_instr_index( unsigned short pc, vf_ContextHandle ctx )
{
    vf_InstrHandle instr = ctx->m_bc[pc].m_instr;
    assert( instr );
    return vf_get_instr_index( instr, ctx );
}

/**
 * For a given instruction gets a start pointer of the next instruction or the
 * pointer which follows the last bytecode of the method for the last instruction.
 * @param[in] instr an instruction handle
 * @param[in] ctx   a verification context
 * @return a pointer which follows a given instruction
 */
static inline const unsigned char *
vf_get_instr_end( vf_InstrHandle instr, vf_ContextHandle ctx )
{
    if( instr + 1 == ctx->m_last_instr ) {
        return ctx->m_bytes + ctx->m_len;
    }
    return ( instr + 1 )->m_addr;
}

/**
 * Calls destructor of graph, if any.
 *
 * @param[in] context a context of verifier
 */
void vf_free_graph( vf_Context *context );

/**
 * Mark subroutine code.
 *
 * <p>Here is the algorithm sketch: we define subroutine boundaries with
 * simplified data flow analysis and duplicate subroutine nodes
 * in the call graph. For more details check a
 * <a href="http://wiki.apache.org/harmony/Subroutine_Verification">wiki
 * page</a>.</p>
 *
 * @param ctx a verifier context
 * @return VER_OK if no graph structure inconsistencies were detected during marking,
 * an error code otherwise
 */
vf_Result vf_mark_subroutines( vf_Context *ctx );

/**
 * Inline subroutines in the call graph.
 *
 * @param ctx a verifier context
 * @return VER_OK if subroutines were inlined successfully,
 * an error code otherwise
 */
vf_Result vf_inline_subroutines( vf_Context *ctx );

#endif // _VERIFIER_REAL_H_
