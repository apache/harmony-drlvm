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
 * @author Pavel Rebriy
 * @version $Revision: 1.1.2.3.4.4 $
 */  


#ifndef _VERIFIER_REAL_H_
#define _VERIFIER_REAL_H_

#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <assert.h>

#include "verifier.h"
#include "open/bytecodes.h"

#define LOG_DOMAIN "verifier"
#include "cxxlog.h"

using namespace std;

/**
 * Set namespace Verifier.
 */
namespace Verifier {

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
 * Initial node mark in verifier graph.
 */
#define VERIFY_START_MARK       1

/**
 * Memory pool entry size.
 */
#define VERIFY_POOL_ENTRY_SIZE  0x2000

/**
 * Constant pool checks.
 */
// for handler id = 0 is legal value
#define CHECK_HANDLER_CONST_POOL_ID( id, len, context )                                 \
    if( (id) >= (len) ) {                                                               \
        VERIFY_REPORT( context, "(class: " << class_get_name( (context)->m_class )      \
            << ", method: " << method_get_name( (context)->m_method )                   \
            << method_get_descriptor( (context)->m_method )                             \
            << ") Illegal constant pool index in handler" );                            \
        return VER_ErrorHandler;                                                        \
    }
// for handler id = 0 is legal value
#define CHECK_HANDLER_CONST_POOL_CLASS( context, id )                                   \
    if( (id) && class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_Class ) {     \
        VERIFY_REPORT( context, "(class: " << class_get_name( (context)->m_class )      \
            << ", method: " << method_get_name( (context)->m_method )                   \
            << method_get_descriptor( (context)->m_method )                             \
            << ") Illegal type in constant pool for handler, "                          \
            << id << ": CONSTANT_Class is expected" );                                  \
        return VER_ErrorHandler;                                                        \
    }
#define CHECK_CONST_POOL_ID( id, len, context )                                         \
    if( !(id) || (id) >= (len) ) {                                                      \
        VERIFY_REPORT( context, "(class: " << class_get_name( (context)->m_class )      \
            << ", method: " << method_get_name( (context)->m_method )                   \
            << method_get_descriptor( (context)->m_method )                             \
            << ") Illegal constant pool index" );                                       \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_CLASS( context, id )                                           \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_Class ) {             \
        VERIFY_REPORT( context, "(class: " << class_get_name( (context)->m_class )      \
            << ", method: " << method_get_name( (context)->m_method )                   \
            << method_get_descriptor( (context)->m_method )                             \
            << ") Illegal type in constant pool, "                                      \
            << id << ": CONSTANT_Class is expected" );                                  \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_METHOD( context, id )                                          \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_Methodref ) {         \
        VERIFY_REPORT( context, "(class: " << class_get_name( (context)->m_class )      \
            << ", method: " << method_get_name( (context)->m_method )                   \
            << method_get_descriptor( (context)->m_method )                             \
            << ") Illegal type in constant pool, "                                      \
            << id << ": CONSTANT_Methodref is expected" );                              \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_INTERFACE( context, id )                                           \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_InterfaceMethodref ) {    \
        VERIFY_REPORT( context, "(class: " << class_get_name( (context)->m_class )          \
            << ", method: " << method_get_name( (context)->m_method )                       \
            << method_get_descriptor( (context)->m_method )                                 \
            << ") Illegal type in constant pool, "                                          \
            << id << ": CONSTANT_InterfaceMethodref is expected" );                         \
        return VER_ErrorConstantPool;                                                       \
    }
#define CHECK_CONST_POOL_FIELD( context, id )                                           \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_Fieldref ) {          \
        VERIFY_REPORT( context, "(class: " << class_get_name( (context)->m_class )      \
            << ", method: " << method_get_name( (context)->m_method )                   \
            << method_get_descriptor( (context)->m_method )                             \
            << ") Illegal type in constant pool, "                                      \
            << id << ": CONSTANT_Fieldref is expected" );                               \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_TYPE( context, id )                                            \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_NameAndType ) {       \
        VERIFY_REPORT( context, "(class: " << class_get_name( (context)->m_class )      \
            << ", method: " << method_get_name( (context)->m_method )                   \
            << method_get_descriptor( (context)->m_method )                             \
            << ") Illegal type in constant pool, "                                      \
            << id << ": CONSTANT_NameAndType is expected" );                            \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_STRING( context, id )                                          \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_String ) {            \
        VERIFY_REPORT( context, "(class: " << class_get_name( (context)->m_class )      \
            << ", method: " << method_get_name( (context)->m_method )                   \
            << method_get_descriptor( (context)->m_method )                             \
            << ") Illegal type in constant pool, "                                      \
            << id << ": CONSTANT_String is expected" );                                 \
        return VER_ErrorConstantPool;                                                   \
    }
#define CHECK_CONST_POOL_UTF8( context, id )                                            \
    if( class_get_cp_tag( (context)->m_class, (id) ) != _CONSTANT_Utf8) {               \
        VERIFY_REPORT( context, "(class: " << class_get_name( (context)->m_class )      \
            << ", method: " << method_get_name( (context)->m_method )                   \
            << method_get_descriptor( (context)->m_method )                             \
            << ") Illegal type in constant pool, "                                      \
            << id << ": CONSTANT_Utf8 is expected" );                                   \
        return VER_ErrorConstantPool;                                                   \
    }

/**
 * Verifier enums
 */
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
 * Code instruction flags enum.
 */
typedef enum {
    VF_FLAG_NONE = 0,
    VF_FLAG_BEGIN_BASIC_BLOCK = 1,
    VF_FLAG_START_ENTRY = 2,
    VF_FLAG_END_ENTRY = 4,
    VF_FLAG_HANDLER = 8,
    VF_FLAG_RETURN = 16,
    VF_FLAG_THROW = 32,
    VF_FLAG_SUBROUTINE = 64
} vf_InstructionFlag_t;

/**
 * Verifier structures.
 */
//===========================================================
/**
 * Predefined structures
 */
/// Verifier context structure.
typedef struct vf_Context vf_Context_t;
/// Verifier code instruction structure.
typedef struct vf_Code_s vf_Code_t;
/// Graph node container structure
typedef struct vf_NodeContainer_s vf_NodeContainer_t;
/// Graph edge container structure
typedef struct vf_EdgeContainer_s vf_EdgeContainer_t;
/// Verifier graph structure.
typedef struct vf_Graph vf_Graph_t;
/// Verifier graph node structure.
typedef struct vf_Node_s vf_Node_t;
/// Verifier graph edge structure.
typedef struct vf_Edge_s vf_Edge_t;
/// Verifier type constraint structure.
typedef struct vf_TypeConstraint_s vf_TypeConstraint_t;
/// Verifier hash table structure.
typedef struct vf_Hash vf_Hash_t;
/// Verifier hash table entry structure.
typedef struct vf_HashEntry_s vf_HashEntry_t;
/// Verifier pool structure.
typedef struct vf_VerifyPool_s vf_VerifyPool_t;
/// Verifier pool internal structure.
typedef struct vf_VerifyPoolInternal_s vf_VerifyPoolInternal_t;

/** 
 * Code instruction structures.
 */
//===========================================================
/**
 * Initial struct of bytecode instruction.
 */
typedef struct {
    unsigned *m_off;            ///< array of branches
    unsigned m_instr;           ///< number of code instruction @see vf_Code_s
    unsigned m_offcount : 31;   ///< number of branches
    unsigned m_mark : 1;        ///< control flow branch flag
} vf_Instr_t;

/**
 * Valid types structure.
 */
typedef struct {
    unsigned number;          ///< number of types
    const char *string[1];    ///< type array
} vf_ValidType_t;

/** 
 * Stack map entry structure.
 */
typedef struct {
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
} vf_MapEntry_t;

/**
 * Complete struct of bytecode instructions.
 */
struct vf_Code_s {
    unsigned char *m_addr;          ///< address of bytecode instruction
    unsigned *m_off;                ///< array of instruction branches
    unsigned char *m_handler;       ///< array of instruction handlers
    vf_MapEntry_t *m_invector;      ///< stack map IN instruction vector
    vf_MapEntry_t *m_outvector;     ///< stack map OUT instruction vector
    unsigned m_offcount;            ///< number of instruction branches
    unsigned short m_minstack;      ///< minimal stack for instruction
    unsigned short m_inlen;         ///< stack map IN instruction vector length
    unsigned short m_outlen;        ///< stack map OUT instruction vector length
    short m_stack;                  ///< stack change for instruction
    unsigned m_base : 7;            ///< instruction flag @see vf_InstructionFlag_t
};

/** 
 * Constant pool parse structure.
 */
typedef union {
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
} vf_Parse_t;

/**
 * Verifier control flow graph structures.
 */
//===========================================================
/**
 * Structure of stack map vector.
 */
typedef struct {
    vf_MapEntry_t *m_stack;         ///< stack map vector
    vf_MapEntry_t *m_local;         ///< locals map vector
    unsigned short m_number;        ///< number of locals
    unsigned short m_deep;          ///< stack deep
    unsigned short m_maxstack;      ///< max stack length
    unsigned short m_maxlocal;      ///< max local number
} vf_MapVector_t;

/**
 * Graph node structure.
 */
struct vf_Node_s
{
    vf_MapVector_t m_invector;          ///< stack map IN node vector
    vf_MapVector_t m_outvector;         ///< stack map OUT node vector
    unsigned m_start;                   ///< beginning of the first instruction of the BB
    unsigned m_end;                     ///< beginning of the last instruction of the BB.
    unsigned m_inedge;                  ///< the first of incoming node edges
    unsigned m_outedge;                 ///< the first of outcoming node edges
    unsigned m_len;                     ///< length of the basic block
    unsigned m_innum;                   ///< number of incoming edges
    unsigned m_outnum;                  ///< number of outcoming edges
    unsigned m_nodecount;               ///< node count in enumeration
    int m_stack;                        ///< stack deep of node
    int m_mark;                         ///< node mark
    bool m_initialized;                 ///< reference in local variable should be initialized
};

/**
 * Graph edge structure.
 */
struct vf_Edge_s
{
    unsigned m_start;       ///< start edge node
    unsigned m_end;         ///< end edge node
    unsigned m_innext;      ///< next incoming edge
    unsigned m_outnext;     ///< next outcoming edge
};

/**
 * Graph node container structure.
 */
struct vf_NodeContainer_s
{
    vf_NodeContainer_t* m_next;     ///< next container
    unsigned m_max;                 ///< max number of nodes in container
    unsigned m_used;                ///< number of nodes in container
    vf_Node_t m_node[1];            ///< array of container nodes
};

/**
 * Graph edge container structure.
 */
struct vf_EdgeContainer_s
{
    vf_EdgeContainer_t* m_next;     ///< next container
    unsigned m_max;                 ///< max number of edges in container
    unsigned m_used;                ///< number of edges in container
    vf_Edge_t m_edge[1];            ///< array of container edges
};

/**
 * Verifier control flow graph structure.
 */
struct vf_Graph
{
public:

    /**
     * Control flow graph constructor.
     * @param node - number of graph nodes
     * @param edge - number of graph edges
     * @note Function allocates memory for graph structure, nodes and edges.
     */
    vf_Graph( unsigned node, unsigned edge );

    /**
     * Control flow graph constructor.
     * @param node - number of graph nodes
     * @param edge - number of graph edges
     * @param pool - external memory pool
     * @note Function allocates memory for graph structure, nodes and edges.
     */
    vf_Graph( unsigned node, unsigned edge, vf_VerifyPool_t *pool );

    /**
     * Control flow graph destructor.
     * @note Function release memory for graph structure, nodes and edges.
     */
    ~vf_Graph();

    /**
     * Function create graph nodes.
     * @param count - number of graph nodes
     * @note Function allocates memory for graph nodes.
     */
    void CreateNodes( unsigned count );

    /**
     * Gets graph node.
     * Parameter <i>node_num</i> must be in range.
     *
     * @param[in] node_num  - node number
     *
     * @return The pointer to node structure.
     */
    vf_Node_t* GetNode( unsigned node_num );

    /**
     * Creates a new node and sets data to it.
     * Node array must have enough free space for a new element.
     *
     * @param[in] begin_instr   - begin code instruction of node
     * @param[in] end_instr     - end code instruction of code
     * @param[in] bytecode_len  - bytecode length of node instructions
     */
    void NewNode( unsigned begin_instr,
                  unsigned end_instr,
                  unsigned bytecode_len);

    /**
     * Function set data to graph node.
     * @param node_num      - number of graph node
     * @param begin_instr   - begin code instruction of node
     * @param end_instr     - end code instruction of code
     * @param bytecode_len  - bytecode length of node instructions
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    void SetNode( unsigned node_num,
                  unsigned begin_instr,
                  unsigned end_instr,
                  unsigned bytecode_len );

    /**
     * Gets graph edge.
     * Parameter <i>edge_num</i> must be in range.
     *
     * @param[in] edge_num  - edge number
     *
     * @return The pointer to edge structure.
     */
    vf_Edge_t* GetEdge( unsigned edge_num );

    /**
     * Creates a new edge for graph nodes.
     *
     * @param start_node    - start edge node
     * @param end_node      - end edge node
     * @note Edge is set as out edge for start_node and as in edge for end_node.
     */
    void NewEdge( unsigned start_node,
                  unsigned end_node);

    /**
     * Function receive first code instruction of graph node.
     * @param node_num - graph node number
     * @return Number of first code instruction of graph node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeFirstInstr( unsigned node_num );

    /**
     * Function receive last code instruction of graph node.
     * @param node_num - graph node number
     * @return Number of last code instruction of graph node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeLastInstr( unsigned node_num );

    /**
     * Function receive bytecode length of graph node instructions.
     * @param node_num - graph node number
     * @return Bytecode length of graph node instructions.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeByteCodeLen( unsigned node_num );

    /**
     * Function receive stack modifier of graph.
     * @param node_num - graph node number
     * @return Stack modifier of graph node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    int GetNodeStackModifier( unsigned node_num );

    /**
     * Function sets graph node stack modifier.
     * @param node_num - graph node number
     * @param stack    - stack modifier (signed value)
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    void SetNodeStackModifier( unsigned node_num, int stack );

    /**
     * Function returns number of graph nodes.
     * @return Number of graph nodes.
     */
    unsigned GetNodeNumber();

    /**
     * Function marks graph node.
     * @param node_num - graph node number
     * @param mark     - node mark
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    void SetNodeMark( unsigned node_num, int mark );

    /**
     * Function returns graph node mark.
     * @param  node_num - graph node number
     * @return Function returns graph node mark.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    int GetNodeMark( unsigned node_num );

    /**
     * Function checks if node is marked.
     * @param  node_num - graph node number
     * @return Returns <code>true</code> if node is marked, else returns <code>false</code>.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    bool IsNodeMarked( unsigned node_num );

    /**
     * Function removes node mark.
     */
    void CleanNodesMark();

    /**
     * Sets local variable reference initialization flag for node.
     * @param node_num - graph node number
     * @param flag     - node flag
     */
    void SetNodeInitFlag( unsigned node_num, bool flag );

    /**
     * Gets local variable reference initialization flag for node.
     * @param node_num - graph node number
     * @return Returns <code>true</code> if local variable reference have
     *  to be initialized else returns <code>false</code>.
     */
    bool GetNodeInitFlag( unsigned node_num );

    /**
     * Function creates <i>IN</i> data flow vector of node.
     * @param node_num  - graph node number
     * @param example   - current data flow vector
     * @param need_copy - copy flag
     * @note If copy flag <code>true</code>, incoming vector is copied to <i>IN</i>,
     *       else if copy flag <code>false</code>, <i>IN</i> vector is created
     *       with parameters of current vector.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     * @see vf_MapVector_t
     */
    void SetNodeInVector( unsigned node_num,
                          vf_MapVector_t *example,
                          bool need_copy);

    /**
     * Function creates <i>OUT</i> data flow vector of node.
     * @param node_num  - graph node number
     * @param example   - current data flow vector
     * @param need_copy - copy flag
     * @note If copy flag <code>true</code>, incoming vector is copied to <i>OUT</i>,
     *       else if copy flag <code>false</code>, <i>OUT</i> vector is created
     *       with parameters of current vector.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     * @see vf_MapVector_t
     */
    void SetNodeOutVector( unsigned node_num,
                           vf_MapVector_t *example,
                           bool need_copy );

    /**
     * Function receives <i>IN</i> data flow vector of node.
     * @param node_num  - graph node number
     * @return <i>IN</i> data flow stack map vector of node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     * @see vf_MapVector_t
     */
    vf_MapVector_t * GetNodeInVector( unsigned node_num );

    /**
     * Function receives <i>OUT</i> data flow vector of node.
     * @param node_num  - graph node number
     * @return <i>OUT</i> data flow stack map vector of node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     * @see vf_MapVector_t
     */
    vf_MapVector_t * GetNodeOutVector( unsigned node_num );

    /**
     * Function creates graph edges.
     * @param number - number of edges 
     */
    void CreateEdges( unsigned number );

    /**
     * Function receives next <i>IN</i> edge of graph node.
     * @param edge_num - number of graph edge
     * @return Number of next <i>IN</i> edge of node.
     * @note Assertion is raised if <i>edge_num</i> is out of range.
     */
    unsigned GetEdgeNextInEdge( unsigned edge_num );

    /**
     * Function receives next <i>OUT</i> edge of graph node.
     * @param edge_num - number of graph edge
     * @return Number of next <i>OUT</i> edge of node.
     * @note Assertion is raised if <i>edge_num</i> is out of range.
     */
    unsigned GetEdgeNextOutEdge( unsigned edge_num );

    /**
     * Function receives start graph node of edge.
     * @param edge_num - number of graph edge
     * @return Number start graph node of edge.
     * @note Assertion is raised if <i>edge_num</i> is out of range.
     */
    unsigned GetEdgeStartNode( unsigned edge_num );

    /**
     * Function receives end graph node of edge.
     * @param edge_num - number of graph edge
     * @return Number end graph node of edge.
     * @note Assertion is raised if <i>edge_num</i> is out of range.
     */
    unsigned GetEdgeEndNode( unsigned edge_num );

    /**
     * Function receives number of <i>IN</i> edges of graph node.
     * @param node_num - number of graph node
     * @return Number of <i>IN</i> edges of graph node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeInEdgeNumber( unsigned node_num );

    /**
     * Function receives number of <i>OUT</i> edges of graph node.
     * @param node_num - number of graph node
     * @return Number of <i>OUT</i> edges of graph node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeOutEdgeNumber( unsigned node_num );

    /**
     * Function receives first <i>IN</i> edge of graph node.
     * @param node_num - number of graph node
     * @return First <i>IN</i> edges of node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeFirstInEdge( unsigned node_num );

    /**
     * Function receives first <i>OUT</i> edge of graph node.
     * @param node_num - number of graph node
     * @return First <i>OUT</i> edges of node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeFirstOutEdge( unsigned node_num );

    /**
     * Function allocates memory in graph memory pool.
     * @param size - size of memory block
     * @return Allocated memory pointer
     *         or <code>NULL</code>if memory allocation is failed.
     */
    void * AllocMemory( unsigned size );

    /**
     * Function cleans graph node enumeration, creates new graph
     * enumeration structure and sets first enumeration element.
     * @param node_num - first enumeration node
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    void SetStartCountNode( unsigned node_num );

    /**
     * Function receives number of enumerated nodes.
     * @return Function returns number of enumerated nodes.
     */
    unsigned GetEnumCount();

    /**
     * Function sets next enumeration element in graph enumeration structure.
     * @param node_num - next enumeration node
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    void SetNextCountNode( unsigned node_num );

    /**
     * Function receives first enumerated graph node.
     * @return First enumerated graph node in graph enumeration structure.
     */
    unsigned GetStartCountNode();

    /**
     * Function receives graph node relevant to enumeration element.
     * @param count - given enumeration element
     * @return Graph node relevant to enumeration element.
     * @note Assertion is raised if <i>count</i> is out of range.
     */
    unsigned GetCountElementNode( unsigned count );

    /**
     * Function receives graph node enumeration count.
     * @param node_num - given node
     * @return Graph node enumeration count.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeCountElement( unsigned node_num );

    /**
     * Function prints graph structure in <code>stderr</code>.
     * @param context - current verifier context
     * @note Function is valid in debug mode.
     * @see vf_Context_t
     */
    void DumpGraph( vf_Context_t *context );

    /**
     * Function dumps verifier graph in file in DOT format.
     * @param context - current verifier context
     * @note Function is valid in debug mode.
     * @note File name is created from class and method names with .dot extension.
     * @see vf_Context_t
     */
    void DumpDotGraph( vf_Context_t *context );

private:
    vf_NodeContainer_t *m_nodes;        ///< array of nodes
    vf_EdgeContainer_t *m_edges;        ///< array of edges
    vf_VerifyPool_t *m_pool;            ///< graph memory pool
    unsigned *m_enum;                   ///< graph node enumeration structure
    unsigned m_nodenum;                 ///< number of nodes
    unsigned m_edgenum;                 ///< number of edges
    unsigned m_enummax;                 ///< max number of enumerated elements
    unsigned m_enumcount;               ///< number of enumerated elements
    bool m_free;                        ///< need to free pool

    /**
     * Function prints graph node in <code>stderr</code>.
     * @param node_num  - number of graph node
     * @param context      - current verifier context
     * @note Function is valid in debug mode.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     * @see vf_Context_t
     */
    void DumpNode( unsigned node_num, vf_Context_t *context );

    /**
     * Function prints graph node instruction in stream.
     * @param node_num   - number of graph node
     * @param context    - current verifier context
     * @note Function is valid in debug mode.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     * @see vf_Context_t
     */
    void DumpNodeInternal( unsigned node_num,
                           vf_Context_t *context);


    /**
     * Function dumps graph header in file in DOT format.
     * @param graph_name    - graph name
     * @param fout          - file stream
     * @note Function is valid in debug mode.
     * @note Graph name is created from class and method names.
     */
    void DumpDotHeader( char *graph_name, ofstream &fout );

    /**
     * Function dumps graph node in file in DOT format.
     * @param node_num - number of graph node
     * @param fout     - file stream
     * @param context  - current verifier context
     * @note Function is valid in debug mode.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     * @see vf_Context_t
     */
    void DumpDotNode( unsigned node_num, ofstream &fout, vf_Context_t *context );

    /**
     * Function dumps graph node instruction in file stream in DOT format.
     * @param node_num   - number of graph node
     * @param next_node  - separator between nodes in stream
     * @param next_instr - separator between instructions in stream
     * @param fout       - output file stream
     * @param context    - current verifier context
     * @note Function is valid in debug mode.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     * @see vf_Context_t
     */
    void DumpDotNodeInternal( unsigned node_num,
                              char *next_node,
                              char *next_instr,
                              ofstream &fout,
                              vf_Context_t *context);

    /**
     * Function dumps graph end in file in DOT format.
     * @param fout - output file stream
     * @note Function is valid in debug mode.
     */
    void DumpDotEnd( ofstream &fout );

}; // struct vf_Graph

/**
 * Verifier hash table structures.
 */
//===========================================================

/**
 * Structure of hash entry.
 */
struct vf_HashEntry_s {
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
    vf_HashEntry_t * NewHashEntry( const char *key, unsigned len );

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
    bool CheckKey( vf_HashEntry_t *hash_entry, const char *key, unsigned len );

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
    unsigned HashFunc( const char *key, unsigned len );
}; // struct vf_Hash

/**
 * Verifier type constraint structures.
 */
//===========================================================
/**
 * Verifier type constraint structure.
 */
struct vf_TypeConstraint_s {
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
    vf_ValidType_t * NewType( const char *type, unsigned len );

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

/**
 * Verifier type constraint structures.
 */
//===========================================================
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
    void ClearContext();

public:
    class_handler m_class;              ///< context class
    vf_TypePool *m_type;                ///< context type constraint collection
    char *m_error;                      ///< context error message
    method_handler m_method;            ///< context method
    vf_Graph_t *m_graph;                ///< context control flow graph
    vf_VerifyPool_t *m_pool;            ///< context memory pool
    vf_Code_t *m_code;                  ///< context code instruction of method
    unsigned m_codeNum;                 ///< code instruction number
    unsigned m_nodeNum;                 ///< graph node number
    unsigned m_edgeNum;                 ///< graph edge number

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

/**
 * Other structures.
 */
//===========================================================
/**
 * Head structure of memory pool.
 */
struct vf_VerifyPool_s {
    vf_VerifyPoolInternal_t *m_pool;    ///< pool entry
    unsigned m_memory;                  ///< allocated memory size
    unsigned m_used;                    ///< used memory size
    unsigned m_maxuse;                  ///< max used memory size
};

/**
 * Internal structure of memory pool.
 */
struct vf_VerifyPoolInternal_s {
    void *m_memory;                     ///< pool entry memory
    char *m_free;                       ///< free space in pool entry
    vf_VerifyPoolInternal_t *m_next;    ///< next pool entry
    unsigned m_freesize;                ///< size of free space in pool entry
};

/**
 * Structure of class loader constraint data.
 */
typedef struct {
    vf_VerifyPool_t *pool;  ///< constraint memory pool
    vf_Hash_t *string;      ///< string pool hash table
} vf_ClassLoaderData_t;

/**
 * Verifier data
 */
//===========================================================
/**
 * Array of opcode names. Available in debug mode.
 */
#if _VERIFY_DEBUG
extern const char *vf_opcode_names[];
#endif // _VERIFY_DEBUG

/**
 * Verifier functions.
 */
//===========================================================
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
vf_calloc_func( unsigned number, unsigned element_size, VERIFY_SOURCE_PARAMS );

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
vf_malloc_func( unsigned size, VERIFY_SOURCE_PARAMS );

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
vf_realloc_func( void *pointer, unsigned resize, VERIFY_SOURCE_PARAMS );

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
vf_alloc_pool_memory_func( vf_VerifyPool_t *pool, unsigned size, VERIFY_SOURCE_PARAMS );

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
 * Function provides some checks of control flow and data flow structures of graph.
 * @param context - verifier context
 * @return Checks result.
 * @see vf_Context_t
 * @see Verifier_Result
 */
Verifier_Result
vf_graph_checks( vf_Context_t *context );

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
 * Function clears verifier context.
 */
inline void
vf_Context::ClearContext()
{
    m_method = NULL;
    m_graph = NULL;
    m_code = NULL;
    m_codeNum = 0;
    m_nodeNum = 0;
    m_edgeNum = 0;
    vf_clean_pool_memory( m_pool );
} // vf_Context::vf_ClearContext

/**
 * Function checks basic block flag for given instruction.
 * @param code - bytecode instruction
 * @return If given instruction has basic block flag returns <code>true</code>,
 *         else returns <code>false</code>.
 * @see vf_Code_t
 */
static inline bool
vf_is_begin_basic_block( vf_Code_t *code )
{
    return (code->m_base & VF_FLAG_BEGIN_BASIC_BLOCK) != 0;
} // vf_is_begin_basic_block

/**
 * Function checks flags for given instruction.
 * @param code - bytecode instruction
 * @param flag - checked flags
 * @return If given instruction has checked flag returns <code>true</code>,
 *         else returns <code>false</code>.
 * @see vf_Code_t
 */
static inline bool
vf_is_instruction_has_flags( vf_Code_t *code,
                             unsigned flag)
{
    return ((unsigned)code->m_base & flag) != 0;
} // vf_is_instruction_has_flags

/**
 * Function checks what given instruction has any flags.
 * @param code - bytecode instruction
 * @return If given instruction has any flags returns <code>true</code>,
 *         else returns <code>false</code>.
 * @see vf_Code_t
 */
static inline bool
vf_is_instruction_has_any_flags( vf_Code_t *code )
{
    return (code->m_base != 0);
} // vf_is_instruction_has_flags


/**
 * Function sets error message of verifier.
 * @param stream - stringstream object with message
 * @param ctex   - verifier context
 * @see vf_Context_t
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
    int len = stream.str().length();
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
 * Checks version of class file
 * @param context - verifier context
 */
static inline bool
vf_is_class_version_14( vf_Context_t *context )
{
    return (class_get_version(context->m_class) < 49) ? true : false;
} // vf_is_class_version_14

} // namespace Verifier

using namespace Verifier;

#endif // _VERIFIER_REAL_H_
