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


#ifndef _VERIFIER_GRAPH_H_
#define _VERIFIER_GRAPH_H_
/**
 * @file
 * Control flow graph structures.
 */

#include "ver_real.h"

/**
 * Initial node mark in verifier graph.
 */
#define VERIFY_START_MARK       1


/**
 * Each node has a descriptive bitmap.
 */
enum vf_NodeType {
    VF_TYPE_NODE_CODE_RANGE  = 0,
    VF_TYPE_NODE_START_ENTRY = 1,
    VF_TYPE_NODE_END_ENTRY   = 2,
    VF_TYPE_NODE_HANDLER     = 3
};

/**
 * @ingroup Handles
 * The stack map handle vector handle.
 */
typedef const struct vf_MapVector* vf_MapVectorHandle;
/**
 * @ingroup Handles
 * The handle of the graph node.
 */
typedef const struct vf_Node* vf_NodeHandle;
/**
 * @ingroup Handles
 * The handle of the graph edge.
 */
typedef const struct vf_Edge* vf_EdgeHandle;
/// Declaration of enum with a descriptive bitmap.
typedef enum vf_NodeType vf_NodeType_t;

/**
 * Structure of stack map vector.
 */
struct vf_MapVector {
    vf_MapEntry_t *m_stack;         ///< stack map vector
    vf_MapEntry_t *m_local;         ///< locals map vector
    unsigned short m_number;        ///< number of locals
    unsigned short m_depth;         ///< stack depth
    unsigned short m_maxstack;      ///< max stack length
    unsigned short m_maxlocal;      ///< max local number
};

/**
 * Graph node structure.
 */
struct vf_Node
{
    vf_MapVector m_inMapVector;     ///< stack map IN node vector
    vf_MapVector m_outMapVector;    ///< stack map OUT node vector
    unsigned m_start;               ///< index of the first instruction at vf_Context.m_code
    unsigned m_end;                 ///< index of the last instruction at vf_Context.m_code
    unsigned m_inedge;              ///< the first incoming edge
    unsigned m_outedge;             ///< the first outcoming edge
    unsigned m_innum;               ///< number of incoming edges
    unsigned m_outnum;              ///< number of outcoming edges
    unsigned m_nodecount;           ///< node count in enumeration
    int m_stack;                    ///< stack depth
    int m_mark;                     ///< node mark
    vf_NodeType_t m_type;           ///< node type
    bool m_initialized : 1;         ///< reference in a local variable 
                                    ///< should be initialized
};

/**
 * Graph edge structure.
 */
struct vf_Edge
{
    unsigned m_start;       ///< start edge node
    unsigned m_end;         ///< end edge node
    unsigned m_innext;      ///< next incoming edge
    unsigned m_outnext;     ///< next outcoming edge
};

/**
 * Graph node container structure.
 */
struct vf_NodeContainer
{
    vf_NodeContainer* m_next;           ///< next container
    unsigned m_max;                     ///< max number of nodes in container
    unsigned m_used;                    ///< number of nodes in container
    vf_Node m_node[1];                  ///< array of contained nodes
};

/**
 * Graph edge container structure.
 */
struct vf_EdgeContainer
{
    vf_EdgeContainer* m_next;           ///< next container
    unsigned m_max;                     ///< max number of edges in container
    unsigned m_used;                    ///< number of edges in container
    vf_Edge m_edge[1];                  ///< array of contained edges
};

/**
 * Verifier control flow graph structure.
 */
class vf_Graph
{
public:
    /**
     * Control flow graph constructor.
     * @param node - number of graph nodes
     * @param edge - number of graph edges
     * @param pool - external memory pool
     * @note Function allocates memory for graph structure, nodes and edges.
     */
    vf_Graph( unsigned node, unsigned edge, vf_VerifyPool_t *pool )
            : m_nodes(NULL), m_edges(NULL), m_pool(pool), m_enum(NULL),
            m_nodenum(0), m_edgenum(1), m_enummax(0), m_enumcount(0),
            m_free(false)
    {
        CreateNodes( node );
        CreateEdges( edge );
        return;
    } // vf_Graph

    /**
     * Control flow graph destructor.
     * @note Function release memory for graph structure, nodes and edges.
     */
    ~vf_Graph()
    {
        if( m_free ) {
            vf_delete_pool( m_pool ); 
        }
        return;
    } // ~vf_Graph

    /**
     * Function create graph nodes.
     * @param count - number of graph nodes
     * @note Function allocates memory for graph nodes.
     */
    void CreateNodes( unsigned count );

    /**
     * Gets a graph node.
     *
     * @param[in] node_num  a node number, should be in range
     * @return a handle of the node
     */
    vf_NodeHandle GetNode( unsigned node_num );

    /**
     * Gets the first node.
     *
     * @return a handle of the node
     */
    vf_NodeHandle GetFirstNode()
    {
        return GetNode( 0 );
    } // GetFirstNode

    /**
     * Creates a new node of a specific type.
     * Node array must have enough free space for a new element.
     *
     * @param[in] m_type node type
     * @param[in] stack  a stack modifier (signed value)
     * @return a handle of the created node
     */
    vf_NodeHandle NewNode( vf_NodeType_t m_type, int stack );

    /**
     * Creates a new node for a bytecode range.
     * Node array must have enough free space for a new element.
     *
     * @param[in] start   an instruction start index
     * @param[in] end     an instruction end index
     * @param[in] stack   a stack modifier (signed value)
     * @return a handle of the created node
     */
    vf_NodeHandle NewNode( unsigned start,
                           unsigned end,
                           int stack)
    {
        // get node
        vf_Node* node = (vf_Node*) NewNode( VF_TYPE_NODE_CODE_RANGE, stack );
        node->m_start = start;
        node->m_end = end;
        return node;
    } // NewNode( start, end, len )

    /**
     * Gets graph edge.
     * Parameter <i>edge_num</i> must be in range.
     *
     * @param[in] edge_num  - edge number
     *
     * @return The pointer to edge structure.
     */
    vf_EdgeHandle GetEdge( unsigned edge_num );

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
     * Gets the first code instruction of a node.
     * @param node_num the node number
     * @return a number of the first code instruction of graph node
     * @note Assertion is raised if <i>node_num</i> is not a
     * code range node.
     */
    unsigned GetNodeFirstInstr( unsigned node_num )
    {
        vf_NodeHandle node = GetNode( node_num );
        assert( VF_TYPE_NODE_CODE_RANGE == node->m_type );
        return node->m_start;
    } // GetNodeFirstInstr

    /**
     * Gets the last code instruction of a node.
     * @param node_num the node number
     * @return a number of the last code instruction of graph node
     * @note Assertion is raised if <i>node_num</i> is not a
     * code range node.
     */
    unsigned GetNodeLastInstr( unsigned node_num )
    {
        vf_NodeHandle node = GetNode( node_num );
        assert( VF_TYPE_NODE_CODE_RANGE == node->m_type );
        return node->m_end;
    } // GetNodeLastInstr

    /**
     * Gets a bytecode length of a graph node instructions.
     * @param context a verifier context handle
     * @param node a node handle
     * @return a number of the last code instruction of graph node
     * @note Assertion is raised if <i>node_num</i> is not a
     * code range node.
     */
    unsigned GetNodeBytecodeLen( vf_ContextHandle context,
                                 vf_NodeHandle node)
    {
        assert( VF_TYPE_NODE_CODE_RANGE == node->m_type );
        unsigned char* code_end = (node->m_end + 1 == context->m_codeNum)
            ? method_get_bytecode( context->m_method )
                + method_get_code_length( context->m_method )
            : context->m_code[node->m_end + 1].m_addr;

        unsigned len = code_end - context->m_code[node->m_start].m_addr;
        return len;
    } // GetNodeBytecodeLen

    /**
     * Gets a stack modifier of a graph node.
     * @param node_num a node number
     * @return a stack modifier of graph node
     */
    int GetNodeStackModifier( unsigned node_num )
    {
        return GetNode( node_num )->m_stack;
    } // GetNodeStackModifier

    /**
     * Sets graph node stack modifier.
     * @param node_num a graph node number
     * @param stack    a stack modifier (signed value)
     */
    void SetNodeStackModifier( unsigned node_num, int stack )
    {
        vf_Node* node = (vf_Node*) GetNode( node_num );
        node->m_stack = stack;
    } // SetNodeStackModifier

    /**
     * Counts graph nodes.
     * @return a number of graph nodes
     */
    unsigned GetNodeCount()
    {
        return m_nodenum;
    } // GetNodeCount

    /**
     * Counts graph edges excluding
     * the reserved edge.
     * @return a number of graph edges
     */
    unsigned GetEdgeCount()
    {
        return m_edgenum - 1;
    } // GetEdgeCount

    /**
     * Marks a graph node with a given number.
     * @param node_num a graph node number
     * @param mark     a mark
     */
    void SetNodeMark( unsigned node_num, int mark )
    {
        vf_Node* node = (vf_Node*) GetNode( node_num );
        node->m_mark = mark;
    } // SetNodeMark

    /**
     * Gets a graph node mark.
     * @param  node_num a node number
     * @return a graph node mark
     */
    int GetNodeMark( unsigned node_num )
    {
        return GetNode( node_num )->m_mark;
    } // GetNodeMark

    /**
     * Checks if a node is marked.
     * @param  node_num a graph node number
     * @return <code>true</code> if node is marked, <code>false</code> otherwise
     */
    bool IsNodeMarked( unsigned node_num )
    {
        return 0 != GetNode( node_num )->m_mark;
    } // IsNodeMarked

    /**
     * Function removes node mark.
     */
    void CleanNodesMark()
    {
        // clean node's mark
        assert( m_nodes );
        vf_NodeContainer* nodes = m_nodes;
        while( nodes != NULL ) {
            for( unsigned index = 0; index < nodes->m_used; index++ ) {
                nodes->m_node[index].m_mark = 0;
            }
            nodes = nodes->m_next;
        }
        return;
    } // CleanNodesMark

    /**
     * Sets local variable reference initialization flag for a anode.
     * @param node_num a graph node number
     * @param flag     a node flag
     */
    void SetNodeInitFlag( unsigned node_num, bool flag )
    {
        vf_Node* node = (vf_Node*) GetNode( node_num );
        node->m_initialized = flag;
    } // SetNodeInitFlag

    /**
     * Gets an initialization flag for local variable reference for a given node.
     * @param node_num a graph node number
     * @return <code>true</code> if the local variable reference has
     * to be initialized, <code>false</code> otherwise.
     */
    bool GetNodeInitFlag( unsigned node_num )
    {
        return GetNode( node_num )->m_initialized;
    } // GetNodeInitFlag

    /**
     * Function creates <i>IN</i> data flow vector of a node.
     * @param node_num  a graph node number
     * @param example   a handle of copied data flow vector
     * @param need_copy a copy flag
     * @note If copy flag <code>true</code>, incoming vector is copied to <i>IN</i>,
     *       else if copy flag <code>false</code>, <i>IN</i> vector is created
     *       with parameters of current vector.
     */
    void SetNodeInVector( unsigned node_num,
                          vf_MapVectorHandle example,
                          bool need_copy)
    {
        SetVector( GetNodeInVector( node_num ), example, need_copy );
    } // SetNodeInVector

    /**
     * Creates <i>OUT</i> data flow vector of a node.
     * @param node_num  a graph node number
     * @param example   a handle of copied data flow vector
     * @param need_copy a copy flag
     * @note If copy flag <code>true</code>, incoming vector is copied to <i>OUT</i>,
     *       else if copy flag <code>false</code>, <i>OUT</i> vector is created
     *       with parameters of current vector.
     */
    void SetNodeOutVector( unsigned node_num,
                           vf_MapVectorHandle example,
                           bool need_copy )
    {
        SetVector( GetNodeOutVector( node_num ), example, need_copy );
    } // SetNodeOutVector

    /**
     * Gets <i>IN</i> data flow vector for the node.
     * @param node_num graph node number
     * @return a reference to <i>IN</i> data flow stack map vector
     * @note Assertion is raised if <i>node_num</i> is out of range.
     * @see vf_MapVector_t
     */
    vf_MapVectorHandle GetNodeInVector( unsigned node_num )
    {
        return &( GetNode( node_num )->m_inMapVector );
    } // GetNodeInVector

    /**
     * Gets <i>OUT</i> data flow vector for the node.
     * @param node_num graph node number
     * @return a reference to <i>OUT</i> data flow stack map vector
     * @see vf_MapVector_t
     */
    vf_MapVectorHandle GetNodeOutVector( unsigned node_num )
    {
        return &( GetNode( node_num )->m_outMapVector );
    } // GetNodeOutVector

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
    unsigned GetEdgeNextInEdge( unsigned edge_num )
    {
        return GetEdge( edge_num )->m_innext;
    } // GetEdgeNextInEdge

    /**
     * Function receives next <i>OUT</i> edge of graph node.
     * @param edge_num - number of graph edge
     * @return Number of next <i>OUT</i> edge of node.
     * @note Assertion is raised if <i>edge_num</i> is out of range.
     */
    unsigned GetEdgeNextOutEdge( unsigned edge_num )
    {
        return GetEdge( edge_num )->m_outnext;
    } // GetEdgeNextOutEdge

    /**
     * Function receives start graph node of edge.
     * @param edge_num - number of graph edge
     * @return Number start graph node of edge.
     * @note Assertion is raised if <i>edge_num</i> is out of range.
     */
    unsigned GetEdgeStartNode( unsigned edge_num )
    {
        return GetEdge( edge_num )->m_start;
    } // GetEdgeStartNode

    /**
     * Function receives end graph node of edge.
     * @param edge_num - number of graph edge
     * @return Number end graph node of edge.
     * @note Assertion is raised if <i>edge_num</i> is out of range.
     */
    unsigned GetEdgeEndNode( unsigned edge_num )
    {
        return GetEdge( edge_num )->m_end;
    } // GetEdgeStartNode

    /**
     * Function receives number of <i>IN</i> edges of graph node.
     * @param node_num - number of graph node
     * @return Number of <i>IN</i> edges of graph node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeInEdgeNumber( unsigned node_num )
    {
        return GetNode( node_num )->m_innum;
    } // GetNodeInEdgeNumber

    /**
     * Function receives number of <i>OUT</i> edges of graph node.
     * @param node_num - number of graph node
     * @return Number of <i>OUT</i> edges of graph node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeOutEdgeNumber( unsigned node_num )
    {
        return GetNode( node_num )->m_outnum;
    } // GetNodeOutEdgeNumber

    /**
     * Function receives first <i>IN</i> edge of graph node.
     * @param node_num - number of graph node
     * @return First <i>IN</i> edges of node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeFirstInEdge( unsigned node_num )
    {
        return GetNode( node_num )->m_inedge;
    } // GetNodeFirstInEdge

    /**
     * Function receives first <i>OUT</i> edge of graph node.
     * @param node_num - number of graph node
     * @return First <i>OUT</i> edges of node.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeFirstOutEdge( unsigned node_num )
    {
        return GetNode( node_num )->m_outedge;
    } // GetNodeFirstOutEdge

    /**
     * Allocates memory from the graph memory pool.
     * @param size a size of memory block
     * @return a pointer to allocated memory,
     *         or <code>NULL</code> if memory allocation failed.
     */
    void* AllocMemory( unsigned size ) {
        assert( size );
        return vf_alloc_pool_memory( m_pool, size );
    } // AllocMemory

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
    unsigned GetEnumCount()
    {
        return m_enumcount;
    } // SetStartCountNode

    /**
     * Function sets next enumeration element in graph enumeration structure.
     * @param node_num - next enumeration node
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    void SetNextCountNode( unsigned node_num )
    {
        // check enumeration count is in range
        assert( m_enumcount < m_nodenum );

        // set enumeration element for node
        m_enum[m_enumcount] = node_num;

        // set node enumeration number and increase number of enumerated nodes
        vf_Node* node = (vf_Node*) GetNode( node_num );
        node->m_nodecount = m_enumcount++;
        return;
    } // SetNextCountNode

    /**
     * Function receives first enumerated graph node.
     * @return First enumerated graph node in graph enumeration structure.
     */
    unsigned GetStartCountNode()
    {
        return m_enum[0];
    } // GetStartCountNode
    
    /**
     * Function receives graph node relevant to enumeration element.
     * @param count - given enumeration element
     * @return Graph node relevant to enumeration element.
     * @note Assertion is raised if <i>count</i> is out of range.
     */
    unsigned GetCountElementNode( unsigned count )
    {
        // check element is in range.
        assert( count < m_nodenum );
        return m_enum[count];
    } // GetCountElementNode

    /**
     * Function receives graph node enumeration count.
     * @param node_num - given node
     * @return Graph node enumeration count.
     * @note Assertion is raised if <i>node_num</i> is out of range.
     */
    unsigned GetNodeCountElement( unsigned node_num )
    {
        return GetNode( node_num )->m_nodecount;
    } // GetNodeCountElement

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
    vf_NodeContainer* m_nodes;          ///< array of nodes
    vf_EdgeContainer* m_edges;          ///< array of edges
    vf_VerifyPool_t *m_pool;            ///< graph memory pool
    unsigned *m_enum;                   ///< graph node enumeration structure
    unsigned m_nodenum;                 ///< number of nodes
    unsigned m_edgenum;                 ///< number of edges
    unsigned m_enummax;                 ///< max number of enumerated elements
    unsigned m_enumcount;               ///< number of enumerated elements
    bool m_free;                        ///< need to free pool

    /**
     * Creates a data flow vector from a given example.
     * @param vector_handle a vector to set
     * @param example   a handle of an example vector
     * @param need_copy a copy flag
     * @note If copy flag <code>true</code>, incoming vector is copied to,
     *       otherwise <i>IN</i> vector is created
     *       with parameters of current vector.
     */
    void SetVector( vf_MapVectorHandle vector_handle,
                    vf_MapVectorHandle example,
                    bool need_copy);

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

}; // vf_Graph

/**
 * Checks if a code range node ends with a specific
 * instruction type.
 *
 * @param[in] context a verifier context
 * @param[in] node_index the node identifier
 *
 * @return a type of the last instruction of code range node
 */
static inline vf_CodeType
vf_get_last_instruction_type( vf_ContextHandle context,
                              unsigned node_index)
{
    vf_NodeHandle node = context->m_graph->GetNode( node_index );
    assert( VF_TYPE_NODE_CODE_RANGE == node->m_type );
    return context->m_code[node->m_end].m_type;
}

#endif // _VERIFIER_GRAPH_H_
