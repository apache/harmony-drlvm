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
 * @version $Revision: 1.1.2.1.4.4 $
 */  


#include "ver_real.h"

/**
 * Debug flag macros
 */
// Macro prints original control flow graph
#define PRINT_ORIGINAL_GRAPH   0
// Macro prints modified control flow graph
#define PRINT_MODIFIED_GRAPH   0
// Macro dumps original control flow graph in file in DOT format
#define DUMP_ORIGINAL_GRAPH    0
// Macro dumps modified control flow graph in file in DOT format
#define DUMP_MODIFIED_GRAPH    0

/**
 * Set namespace Verifier
 */
namespace Verifier {

/**
 * Function evaluates stack deep of graph node.
 */
static int
vf_get_node_stack_deep( vf_Code_t *begin,   // begin code instruction of node
                        vf_Code_t *end);    // end code instruction of node

/**
 * Function checks graph nodes stack deep consistency. It's recursive function.
 * Function returns result of check.
 */
static Verifier_Result
vf_check_stack_deep( unsigned nodenum,       // graph node number
                     int stack_deep,         // initial stack deep of node
                     unsigned maxstack,      // maximal stack
                     unsigned *count,        // pointer to checked node count
                     vf_Context_t *ctex);    // verifier context

/************************************************************
 ******************* Graph Implementation *******************
 ************************************************************/

/**
 * Control flow graph constructor.
 */
vf_Graph::vf_Graph( unsigned node,      // number of nodes
                    unsigned edge)      // number of edges
                    : m_nodes(NULL), m_edges(NULL), m_enum(NULL), m_nodenum(0),
                    m_edgenum(1), m_enummax(0), m_enumcount(0), m_free(true)
{
    m_pool = vf_create_pool();
    CreateNodes( node );
    CreateEdges( edge );
    return;
} // vf_Graph::vf_Graph

/**
 * Control flow graph constructor.
 */
vf_Graph::vf_Graph( unsigned node,          // number of nodes
                    unsigned edge,          // number of edges
                    vf_VerifyPool_t *pool)  // external pool
                    : m_nodes(NULL), m_edges(NULL), m_pool(pool), m_enum(NULL),
                    m_nodenum(0), m_edgenum(1), m_enummax(0), m_enumcount(0),
                    m_free(false)
{
    CreateNodes( node );
    CreateEdges( edge );
    return;
} // vf_Graph::vf_Graph

/**
 * Control flow graph destructor.
 */
vf_Graph::~vf_Graph()
{
    if( m_free ) {
        vf_delete_pool( m_pool ); 
    }
    return;
} // vf_Graph::~vf_Graph

/**
 * Function create graph nodes.
 */
void
vf_Graph::CreateNodes( unsigned number )    // number of nodes
{
    assert(number > 0);
    vf_NodeContainer_t* nodes;
    nodes = (vf_NodeContainer_t*)AllocMemory( sizeof(vf_NodeContainer_t)
            + (number - 1) * sizeof(vf_Node_t) );
    nodes->m_max = number;
    if( m_nodes == NULL ) {
        m_nodes = nodes;
    } else {
        vf_NodeContainer_t *index = m_nodes->m_next;
        while( index->m_next ) {
            index = index->m_next;
        }
        index->m_next = nodes;
    }
    return;
} // vf_Graph::CreateNodes

/**
 * Gets graph node.
 */
vf_Node_t*
vf_Graph::GetNode( unsigned node_num )  // graph node number
{
    // get node
    assert( m_nodes );
    assert( node_num < m_nodenum );
    unsigned count = node_num;
    vf_NodeContainer_t* nodes = m_nodes;
    while( count > nodes->m_max ) {
        count -= nodes->m_max;
        nodes = nodes->m_next;
        assert(nodes);
    }
    return &nodes->m_node[count];
} // vf_Graph::GetNode

/**
 * Creates a new node and sets data to it.
 */
void
vf_Graph::NewNode( unsigned begin,      // begin code instruction of node
                   unsigned end,        // end code instruction of node
                   unsigned len)        // bytecode length of node
{
    // get node
    assert( m_nodes );
    unsigned count = m_nodenum;
    vf_NodeContainer_t* nodes = m_nodes;
    while( count > nodes->m_max ) {
        count -= nodes->m_max;
        nodes = nodes->m_next;
        assert(nodes);
    }

    // set node
    vf_Node_t* node = &nodes->m_node[count];
    node->m_start = begin;
    node->m_end = end;
    node->m_len = len;

    // increment nodes count
    m_nodenum++;
    nodes->m_used++;
    assert( nodes->m_used <= nodes->m_max );

    return;
} // vf_Graph::NewNode

/**
 * Function set data to graph node.
 */
void
vf_Graph::SetNode( unsigned num,        // graph node number
                   unsigned begin,      // begin code instruction of node
                   unsigned end,        // end code instruction of node
                   unsigned len)        // bytecode length of node
{
    vf_Node_t* node = GetNode( num );
    node->m_start = begin;
    node->m_end = end;
    node->m_len = len;
    return;
} // vf_Graph::SetNode

/**
 * Gets graph edge.
 */
vf_Edge_t*
vf_Graph::GetEdge( unsigned edge_num )  // graph edge number
{
    // get edge
    assert( m_edges );
    assert( edge_num < m_edgenum );
    assert( edge_num );             // zero edge is reserved
    unsigned count = edge_num;
    vf_EdgeContainer_t* edges = m_edges;
    while( count > edges->m_max ) {
        count -= edges->m_max;
        edges = edges->m_next;
        assert(edges);
    }
    return &edges->m_edge[count];
} // vf_Graph::GetEdge

/**
 * Creates a new edge for graph nodes.
 */
void
vf_Graph::NewEdge( unsigned start,   // start graph node of edge
                   unsigned end)     // end graph node of edge
{
    // check node numbers are in range
    assert( start < m_nodenum );
    assert( end < m_nodenum );

    // get edge
    assert( m_edges );
    unsigned count = m_edgenum;
    vf_EdgeContainer_t* edges = m_edges;
    while( count > edges->m_max ) {
        count -= edges->m_max;
        edges = edges->m_next;
        assert(edges);
    }

    // get a new edge and edge's nodes
    vf_Edge_t *edge = &edges->m_edge[count];
    vf_Node_t *node_start = GetNode( start );
    vf_Node_t *node_end = GetNode( end );

    // set a new edge
    edge->m_start = start;
    edge->m_end = end;
    edge->m_outnext = node_start->m_outedge;
    node_start->m_outedge = m_edgenum;
    node_start->m_outnum++;
    edge->m_innext = node_end->m_inedge;
    node_end->m_inedge = m_edgenum;
    node_end->m_innum++;

    // increment edge count
    m_edgenum++;
    edges->m_used++;
    assert( edges->m_used <= edges->m_max );

    return;
} // vf_Graph::NewEdge

/**
 * Function receive first code instruction of graph node.
 */
unsigned
vf_Graph::GetNodeFirstInstr( unsigned num )     // graph node number
{
    // check node number is in range.
    assert( num < m_nodenum );
    return GetNode( num )->m_start;
} // vf_Graph::GetNodeFirstInstr

/**
 * Function receive last code instruction of graph node.
 */
unsigned
vf_Graph::GetNodeLastInstr( unsigned num )      // graph node number
{
    // check node number is in range.
    assert( num < m_nodenum );
    return GetNode( num )->m_end;
} // vf_Graph::GetNodeLastInstr

/**
 * Function receive bytecode length of graph node instructions.
 */
inline unsigned
vf_Graph::GetNodeByteCodeLen( unsigned num )    // graph node number
{
    // check node number is in range.
    assert( num < m_nodenum );
    return GetNode( num )->m_len;
} // vf_Graph::GetNodeByteCodeLen

/**
 * Function receive stack modifier of graph.
 */
int
vf_Graph::GetNodeStackModifier( unsigned num )  // graph node number
{
    // check node number is in range.
    assert( num < m_nodenum );
    return GetNode( num )->m_stack;
} // vf_Graph::GetNodeStackModifier

/**
 * Function sets graph node stack modifier.
 */
inline void
vf_Graph::SetNodeStackModifier( unsigned num,   // graph node number
                                int stack)      // stack deep modifier
{
    // check node number is in range.
    assert( num < m_nodenum );
    GetNode( num )->m_stack = stack;
    return;
} // vf_Graph::SetNodeStackModifier

/**
 * Function returns number of graph nodes.
 */
unsigned
vf_Graph::GetNodeNumber()
{
    return m_nodenum;
} // vf_Graph::GetNodeNumber

/**
 * Function marks graph node.
 */
void
vf_Graph::SetNodeMark( unsigned num,    // graph node number
                       int mark)        // node mark value
{
    // check node number is in range.
    assert( num < m_nodenum );
    GetNode( num )->m_mark = mark;
    return;
} // vf_Graph::SetNodeMark

/**
 * Function returns graph node mark.
 */
int
vf_Graph::GetNodeMark( unsigned num )   // graph node number
{
    // check node number is in range.
    assert( num < m_nodenum );
    return GetNode( num )->m_mark;
} // vf_Graph::GetNodeMark

/**
 * Function checks if node is marked.
 */
inline bool
vf_Graph::IsNodeMarked( unsigned num )  // graph node number
{
    // check node number is in range.
    assert( num < m_nodenum );
    return (GetNode( num )->m_mark != 0);
} // vf_Graph::IsNodeMarked

/**
 * Function removes node mark.
 */
void
vf_Graph::CleanNodesMark()
{
    // clean node's mark
    assert( m_nodes );
    vf_NodeContainer_t* nodes = m_nodes;
    while( nodes != NULL ) {
        for( unsigned index = 0; index < nodes->m_used; index++ ) {
            nodes->m_node[index].m_mark = 0;
        }
        nodes = nodes->m_next;
    }
    return;
} // vf_Graph::CleanNodesMark

/**
 * Sets local variable reference initialization flag for node.
 */
void
vf_Graph::SetNodeInitFlag( unsigned num,    // graph node number
                           bool flag)       // node flag
{
    // check node number is in range.
    assert( num < m_nodenum );
    GetNode( num )->m_initialized = flag;
    return;
} // vf_Graph::SetNodeInitFlag

/**
 * Gets local variable reference initialization flag for node.
 */
bool
vf_Graph::GetNodeInitFlag( unsigned num )   // graph node number
{
    // check node number is in range.
    assert( num < m_nodenum );
    return GetNode( num )->m_initialized;
} // vf_Graph::GetNodeInitFlag

/**
 * Function receives IN data flow vector of node.
 */
vf_MapVector_t *
vf_Graph::GetNodeInVector( unsigned node_num )      // graph node number
{
    assert( node_num < m_nodenum );
    return &(GetNode( node_num )->m_invector);
} // vf_Graph::GetNodeInVector

/**
 * Function receives OUT data flow vector of node.
 */
vf_MapVector_t *
vf_Graph::GetNodeOutVector( unsigned node_num )     // graph node number
{
    assert( node_num <= m_nodenum );
    return &(GetNode( node_num )->m_outvector);
} // vf_Graph::GetNodeOutVector


/**
 * Function creates IN data flow vector of node.
 */
void
vf_Graph::SetNodeInVector( unsigned node_num,           // graph node number
                           vf_MapVector_t *example,     // current data flow vector
                           bool need_copy)              // copy flag
{
    assert( example );
    assert( node_num < m_nodenum );
    vf_MapVector_t *vector = GetNodeInVector( node_num );
    // create and set local vector
    if( example->m_maxlocal ) {
        vector->m_local = (vf_MapEntry_t*)AllocMemory(example->m_maxlocal
                                * sizeof(vf_MapEntry_t) );
        vector->m_number = example->m_number;
        vector->m_maxlocal = example->m_maxlocal;
    }
    // create and set stack vector
    if( example->m_maxstack ) {
        vector->m_stack = (vf_MapEntry_t*)AllocMemory( example->m_maxstack
                                * sizeof(vf_MapEntry_t) );
        vector->m_deep = example->m_deep;
        vector->m_maxstack = example->m_maxstack;
    }
    if( need_copy ) {
        unsigned index;
        for( index = 0; index < example->m_number; index++ ) {
            vector->m_local[index] = example->m_local[index];
        }
        for( index = 0; index < example->m_deep; index++ ) {
            vector->m_stack[index] = example->m_stack[index];
        }
    }
    return;
} // vf_Graph::SetNodeInVector

/**
 * Function creates OUT data flow vector of node.
 */
void
vf_Graph::SetNodeOutVector( unsigned node_num,          // graph node number
                            vf_MapVector_t *example,    // current data flow vector
                            bool need_copy)             // copy flag
{
    assert( example );
    assert( node_num < m_nodenum );
    vf_MapVector_t *vector = GetNodeOutVector( node_num );
    // create and set local vector
    if( example->m_maxlocal ) {
        vector->m_local = (vf_MapEntry_t*)AllocMemory( example->m_maxlocal
                                    * sizeof(vf_MapEntry_t) );
        vector->m_number = example->m_number;
        vector->m_maxlocal = example->m_maxlocal;
    }
    // create and set stack vector
    if( example->m_maxstack ) {
        vector->m_stack = (vf_MapEntry_t*)AllocMemory( example->m_maxstack
                                    * sizeof(vf_MapEntry_t) );
        vector->m_deep = example->m_deep;
        vector->m_maxstack = example->m_maxstack;
    }
    if( need_copy ) {
        unsigned index;
        for( index = 0; index < example->m_number; index++ ) {
            vector->m_local[index] = example->m_local[index];
        }
        for( index = 0; index < example->m_deep; index++ ) {
            vector->m_stack[index] = example->m_stack[index];
        }
    }
    return;
} // vf_Graph::SetNodeOutVector

/**
 * Function creates graph edges.
 */
void
vf_Graph::CreateEdges( unsigned number )        // number of edges
{
    assert(number > 0);
    vf_EdgeContainer_t* edges;
    edges = (vf_EdgeContainer_t*)AllocMemory( sizeof(vf_EdgeContainer_t)
                    + number * sizeof(vf_Edge_t) );
    edges->m_max = number + 1;  // zero edge is reserved
    if( m_edges == NULL ) {
        m_edges = edges;
    } else {
        vf_EdgeContainer_t *index = m_edges->m_next;
        while( index->m_next ) {
            index = index->m_next;
        }
        index->m_next = edges;
    }
    return;
} // vf_Graph::CreateEdges

/**
 * Function receives next IN edge of graph node.
 */
unsigned
vf_Graph::GetEdgeNextInEdge( unsigned num )     // graph node number
{
    // zero edge is reserved
    assert( num && num < m_edgenum );
    return GetEdge( num )->m_innext;
} // vf_Graph::GetEdgeNextInEdge

/**
 * Function receives next OUT edge of graph node.
 */
unsigned
vf_Graph::GetEdgeNextOutEdge( unsigned num )    // graph node number
{
    // zero edge is reserved
    assert( num && num < m_edgenum );
    return GetEdge( num )->m_outnext;
} // vf_Graph::GetEdgeNextOutEdge

/**
 * Function receives start graph node of edge.
 */
unsigned
vf_Graph::GetEdgeStartNode( unsigned num )      // graph node number
{
    // zero edge is reserved
    assert( num && num < m_edgenum );
    return GetEdge( num )->m_start;
} // vf_Graph::GetEdgeStartNode

/**
 * Function receives end graph node of edge.
 */
unsigned
vf_Graph::GetEdgeEndNode( unsigned num )        // graph node number
{
    // zero edge is reserved
    assert( num && num < m_edgenum );
    return GetEdge( num )->m_end;
} // vf_Graph::GetEdgeStartNode

/**
 * Function receives number of IN edges of graph node.
 */
unsigned
vf_Graph::GetNodeInEdgeNumber( unsigned num )   // graph node number
{
    assert( num < m_nodenum );
    return GetNode( num )->m_innum;
} // vf_Graph::GetNodeInEdgeNumber

/**
 * Function receives number of OUT edges of graph node.
 */
unsigned
vf_Graph::GetNodeOutEdgeNumber( unsigned num )  // graph node number
{
    assert( num < m_nodenum );
    return GetNode( num )->m_outnum;
} // vf_Graph::GetNodeOutEdgeNumber

/**
 * Function receives first IN edge of graph node.
 */
unsigned
vf_Graph::GetNodeFirstInEdge( unsigned num )    // graph node number
{
    assert( num < m_nodenum );
    return GetNode( num )->m_inedge;
} // vf_Graph::GetNodeFirstInEdge

/**
 * Function receives first OUT edge of graph node.
 */
unsigned
vf_Graph::GetNodeFirstOutEdge( unsigned num )   // graph node number
{
    assert( num < m_nodenum );
    return GetNode( num )->m_outedge;
} // vf_Graph::GetNodeFirstOutEdge

/**
 * Function allocates memory in graph memory pool.
 */
void *
vf_Graph::AllocMemory( unsigned size )      // memory block size
{
    assert(size);
    void *result = vf_alloc_pool_memory( m_pool, size );
    return result;
} // vf_Graph::AllocMemory

/**
 * Function cleans graph node enumeration, creates new graph
 * enumeration structure and sets first enumeration node.
 */
void
vf_Graph::SetStartCountNode( unsigned node_num )     // graph node number
{
    // check node number is in range
    assert( m_nodes );
    assert( node_num < m_nodenum );

    // create memory
    if( m_enummax < m_nodenum ) {
        m_enum = (unsigned*)AllocMemory( sizeof(unsigned) * m_nodenum );
    }

    // clean node enumeration
    vf_NodeContainer_t* nodes = m_nodes;
    unsigned count = 0;
    while( nodes != NULL ) {
        for( unsigned index = 0; index < nodes->m_used; index++, count++ ) {
            nodes->m_node[index].m_nodecount = ~0U;
            m_enum[count] = ~0U;
        }
        nodes = nodes->m_next;
    }
    assert( count == m_nodenum );

    // set enumeration first element;
    m_enum[0] = node_num;
    m_enumcount = 1;

    // set node enumeration number
    GetNode( node_num )->m_nodecount = 0;
    return;
} // vf_Graph::SetStartCountNode

/**
 * Function receives number of enumerated nodes.
 */
unsigned
vf_Graph::GetEnumCount()
{
    return m_enumcount;
} // vf_Graph::SetStartCountNode

/**
 * Function sets next enumeration element to graph enumeration structure.
 */
void
vf_Graph::SetNextCountNode( unsigned node_num )   // graph node number
{
    // check node number and enumeration count are in range
    assert( node_num < m_nodenum );
    assert( m_enumcount < m_nodenum );

    // set enumeration element for node
    m_enum[m_enumcount] = node_num;

    // set node enumeration number and increase number of enumerated nodes
    GetNode( node_num )->m_nodecount = m_enumcount++;
    return;
} // vf_Graph::SetNextCountNode

/**
 * Function receives first enumerated graph node.
 */
unsigned
vf_Graph::GetStartCountNode()
{
    // return first enumerated element
    return m_enum[0];
} // vf_Graph::GetStartCountNode

/**
 * Function receives graph node relevant to enumeration element.
 */
unsigned
vf_Graph::GetCountElementNode( unsigned count )       // graph node number
{
    // check element is in range.
    assert( count < m_nodenum );
    return m_enum[count];
} // vf_Graph::GetCountElementNode

/**
 * Function receives graph node enumeration count.
 */
unsigned
vf_Graph::GetNodeCountElement( unsigned node_num )     // graph node number
{
    // check node number is in range.
    assert( node_num < m_nodenum );
    return GetNode( node_num )->m_nodecount;
} // vf_Graph::GetNodeCountElement

/************************************************************
 **************** Debug Graph Implementation ****************
 ************************************************************/

/**
 * Function prints graph structure in stderr.
 */
void
vf_Graph::DumpGraph( vf_Context_t *ctex )   // verifier context
{
#if _VERIFY_DEBUG
    VERIFY_DEBUG( "Method: " << class_get_name( ctex->m_class ) << "::"
        << method_get_name( ctex->m_method )
        << method_get_descriptor( ctex->m_method ) << endl );
    VERIFY_DEBUG( "-- start --" );
    for( unsigned index = 0; index < GetNodeNumber(); index++ ) {
        DumpNode( index, ctex );
    }
#endif // _VERIFY_DEBUG
    return;
} // vf_Graph::DumpGraph

/**
 * Function prints graph node in stderr.
 */
void
vf_Graph::DumpNode( unsigned num,           // graph node number
                    vf_Context_t *ctex)     // verifier context
{
#if _VERIFY_DEBUG
    // print node incoming edges
    unsigned index;
    unsigned edge_num;
    for( index = 0, edge_num = GetNode( num )->m_inedge;
         index < GetNode( num )->m_innum;
         index++ )
    {
        vf_Edge_t *edge = GetEdge( edge_num );
        VERIFY_DEBUG( " [" << edge->m_start << "] -->" );
        edge_num = edge->m_innext;
    }

    // print node
    if( vf_is_instruction_has_flags( &ctex->m_code[GetNode( num )->m_start],
                                     VF_FLAG_START_ENTRY ) )
    { // start node
        VERIFY_DEBUG( "node[" << num << "]: " << GetNode( num )->m_start << "[-] start" );
    } else if( vf_is_instruction_has_flags( &ctex->m_code[GetNode( num )->m_start],
                                            VF_FLAG_END_ENTRY ) )
    { // end node
        VERIFY_DEBUG( "node[" << num << "]: " << GetNode( num )->m_start << "[-] end" );
        VERIFY_DEBUG( "-- end --" );
    } else if( vf_is_instruction_has_flags( &ctex->m_code[GetNode( num )->m_start],
                                            VF_FLAG_HANDLER ) )
    { // handler node
        VERIFY_DEBUG( "node[" << num << "]: " << num << "handler entry" );
    } else { // another nodes
        DumpNodeInternal( num, ctex );
    }

    // print node outcoming edges
    for( index = 0, edge_num = GetNode( num )->m_outedge;
         index < GetNode( num )->m_outnum;
         index++ )
    {
        vf_Edge_t *edge = GetEdge( edge_num );
        VERIFY_DEBUG( " --> [" << edge->m_end << "]" );
        edge_num = edge->m_outnext;
    }
    VERIFY_DEBUG( "" );
#endif // _VERIFY_DEBUG
    return;
} // vf_Graph::DumpNode

/**
 * Function prints graph node instruction in stream.
 */
void
vf_Graph::DumpNodeInternal( unsigned num,           // graph node number
                            vf_Context_t *ctex)     // verifier context
{
#if _VERIFY_DEBUG
    // print node header
    VERIFY_DEBUG( "Node #" << num );
    VERIFY_DEBUG( "Stack mod: " << GetNode( num )->m_stack );

    // get code instructions
    unsigned count = GetNode( num )->m_end - GetNode( num )->m_start + 1;
    vf_Code_t *instr = &( ctex->m_code[ GetNode( num )->m_start ] );

    // print node instructions
    for( unsigned index = 0; index < count; index++, instr++ ) {
        VERIFY_DEBUG( index << ": " << ((instr->m_stack < 0) ? "[" : "[ ")
            << instr->m_stack << "| " << instr->m_minstack << "] "
            << vf_opcode_names[*(instr->m_addr)] );
    }
#endif // _VERIFY_DEBUG
    return;
} // vf_Graph::DumpNodeInternal

/**
 * Function dumps graph node in file in DOT format.
 */
void
vf_Graph::DumpDotGraph( vf_Context_t *ctex )        // verifier context
{
#if _VERIFY_DEBUG
    unsigned index;

    // get class and method name
    const char *class_name = class_get_name( ctex->m_class );
    const char *method_name = method_get_name( ctex->m_method );
    const char *method_desc = method_get_descriptor( ctex->m_method );

    // create file name
    unsigned len = strlen(class_name) + strlen(method_name)
                        + strlen(method_desc) + 6;
    char *fname = (char*)STD_ALLOCA(len);
    sprintf( fname, "%s_%s%s.dot", class_name, method_name, method_desc );
    char* pointer = fname;
    while( pointer != NULL ) {
        switch(*pointer)
        {
        case '/': 
        case '*':
        case '<':
        case '>':
        case '(':
        case ')': 
        case '{':
        case '}':
        case ';':
            *pointer++ = '_';
            break;        
        case 0:
            pointer = NULL;
            break;
        default:    
            pointer++;
        }
    }

    // create .dot file
    ofstream fout( fname );
    if( fout.fail() ) {
        VERIFY_DEBUG( "vf_Graph::DumpDotGraph: error opening file: " << fname );
        vf_error();
    }
    // create name of graph
    sprintf( fname, "%s::%s%s", class_name, method_name, method_desc );

    // print graph to file
    DumpDotHeader( fname, fout );
    for( index = 0; index < m_nodenum; index++ ) {
        DumpDotNode( index, fout, ctex );
    }
    DumpDotEnd( fout );

    // close file
    fout.flush();
    fout.close();
#endif // _VERIFY_DEBUG
    return;
} // vf_Graph::DumpDotGraph

/**
 * Function dumps graph header in file in DOT format.
 */
void 
vf_Graph::DumpDotHeader( char *graph_name,      // graph name
                         ofstream &out)         // output file stream
{
#if _VERIFY_DEBUG
    out << "digraph dotgraph {" << endl
        << "center=TRUE;" << endl
        << "margin=\".2,.2\";" << endl
        << "ranksep=\".25\";" << endl
        << "nodesep=\".20\";" << endl
        << "page=\"8.5,11\";" << endl
        << "ratio=auto;" << endl
        << "node [color=lightblue2, style=filled, shape=record, "
                  << "fontname=\"Courier\", fontsize=9];" << endl
        << "label=\"" << graph_name << "\";" << endl;
#endif // _VERIFY_DEBUG
    return;
} // vf_Graph::DumpDotHeader

/**
 * Function dumps graph node in file in DOT format.
 */
void
vf_Graph::DumpDotNode( unsigned num,            // graph node number
                       ofstream &out,           // output file stream
                       vf_Context_t *ctex)      // verifier contex
{
#if _VERIFY_DEBUG
    // print node to dot file
    if( vf_is_instruction_has_flags( &ctex->m_code[GetNode( num )->m_start],
                                     VF_FLAG_START_ENTRY ) )
    { // start node
        out << "node" << num << " [label=\"START\", color=limegreen]" << endl;
    } else if( vf_is_instruction_has_flags( &ctex->m_code[GetNode( num )->m_start],
                                            VF_FLAG_END_ENTRY ) )
    { // end node
        out << "node" << num << " [label=\"END\", color=orangered]" << endl;
    } else if( vf_is_instruction_has_flags( &ctex->m_code[GetNode( num )->m_start],
                                            VF_FLAG_HANDLER ) )
    { // handler node
        out << "node" << num << " [label=\"Handler #"
            << num << "\\n---------\\n" << "Type: #" << GetNode( num )->m_len
            << "\", shape=ellipse, color=aquamarine]" << endl;
    } else { // another nodes
        out << "node" << num 
            << " [label=\"";
        DumpDotNodeInternal( num, "\\n---------\\n", "\\l", out, ctex );
        out << "\"]" << endl;
    }

    // print node outcoming edges to dot file
    unsigned index;
    unsigned edge_num;
    for( index = 0, edge_num = GetNode( num )->m_outedge;
         index < GetNode( num )->m_outnum;
         index++ )
    {
        vf_Edge_t *edge = GetEdge( edge_num );

        out << "node" << num << " -> " << "node" << edge->m_end;
        if( vf_is_instruction_has_flags( &ctex->m_code[GetNode( edge->m_end )->m_start],
                                         VF_FLAG_HANDLER ) )
        {
            out << "[color=red]" << endl;
        } else if( num + 1 != edge->m_end     // it's a subroutine call branch
            && vf_is_instruction_has_flags( &ctex->m_code[GetNode( num )->m_end],
                        VF_FLAG_SUBROUTINE ) )
        {
            out << "[color=blue]" << endl;
        }
        out << ";" << endl;
        edge_num = edge->m_outnext;
    }
#endif // _VERIFY_DEBUG
    return;
} // vf_Graph::DumpDotNode

/**
 * Function dumps graph node instruction in file stream in DOT format.
 */
void
vf_Graph::DumpDotNodeInternal( unsigned num,            // graph node number
                               char *next_node,         // separator between nodes in stream
                               char *next_instr,        // separator between instructions in stream
                               ofstream &out,           // output file stream
                               vf_Context_t *ctex)      // verifier contex
{
#if _VERIFY_DEBUG
    // print node header
    out << "Node " << num << next_node
        << "Stack mod: " << GetNode( num )->m_stack << next_node;

    // get code instructions
    unsigned count = GetNode( num )->m_end - GetNode( num )->m_start + 1;
    vf_Code_t *instr = &( ctex->m_code[ GetNode( num )->m_start ] );

    // print node instructions
    for( unsigned index = 0; index < count; index++, instr++ ) {
        out << index << ": " << ((instr->m_stack < 0) ? "[" : "[ ")
            << instr->m_stack << "\\| " << instr->m_minstack << "] "
            << vf_opcode_names[*(instr->m_addr)] << next_instr;
    }
#endif // _VERIFY_DEBUG
    return;
} // vf_Graph::DumpDotNodeInternal

/**
 * Function dumps graph end in file in DOT format.
 */
void
vf_Graph::DumpDotEnd( ofstream &out )   // output file stream
{
#if _VERIFY_DEBUG
    out << "}" << endl;
#endif // _VERIFY_DEBUG
    return;
} // vf_Graph::DumpDotEnd

/************************************************************
 ********************** Graph Creation **********************
 ************************************************************/

/**
 * Function creates bytecode control flow graph.
 */
Verifier_Result
vf_create_graph( vf_Context_t *ctex )   // verifier context
{
    /**
     * Create graph
     */
    ctex->m_graph = new vf_Graph( ctex->m_nodeNum, ctex->m_edgeNum, ctex->m_pool );

    /**
     * Create decoding array: code to node
     */
    unsigned* code2node = (unsigned*)vf_alloc_pool_memory( ctex->m_pool,
                            ctex->m_codeNum * sizeof(unsigned) );
    /** 
     * Create start-entry and handler nodes
     */
    unsigned index;
    unsigned short handlcount = method_get_exc_handler_number( ctex->m_method );
    ctex->m_graph->NewNode( 0, 0, 0 );
    for( index = 1; index < (unsigned)handlcount + 1; index++ ) {
        ctex->m_graph->NewNode( index, index, 0 );
        ctex->m_graph->SetNodeStackModifier( index, 1 );
    }

    /**
     * Create nodes
     * Node count begins from the first basic block after the last handler node.
     * Skip the first instruction, because we create the first node
     * at his end instruction.
     */
    unsigned len;
    unsigned last;
    unsigned nodeCount;
    for( last = nodeCount = 1 + handlcount, index = last + 1;
         index < ctex->m_codeNum - 1;
         index++ ) 
    {
        if( vf_is_begin_basic_block( &ctex->m_code[index] ) ) {
            // set graph nodes
            len = ctex->m_code[index].m_addr - ctex->m_code[last].m_addr;
            ctex->m_graph->NewNode( last, index - 1, len );
            ctex->m_graph->SetNodeStackModifier( nodeCount,
                        vf_get_node_stack_deep( &ctex->m_code[last], &ctex->m_code[index - 1] ) );
            code2node[last] = nodeCount++;
            last = index;
        }
    }
    // set last node with code segment
    unsigned char* code_end = method_get_bytecode( ctex->m_method )
                                 + method_get_code_length( ctex->m_method );
    len = code_end - ctex->m_code[last].m_addr;
    ctex->m_graph->NewNode( last, index - 1, len );
    ctex->m_graph->SetNodeStackModifier( nodeCount,
        vf_get_node_stack_deep( &ctex->m_code[last], &ctex->m_code[index - 1] ) );
    code2node[last] = nodeCount++;
    // set exit node
    ctex->m_graph->NewNode( ctex->m_codeNum - 1, 0, 0 );
    code2node[ctex->m_codeNum - 1] = nodeCount++;
    assert( ctex->m_nodeNum == nodeCount );

    /**
     * Create edges
     * First edge from start-entry node to first code node
     */
    ctex->m_graph->NewEdge( 0, handlcount + 1 );
    for( index = 1; index < nodeCount - 1; index++ ) {
        vf_Code_t* codeInstr = &ctex->m_code[ ctex->m_graph->GetNodeLastInstr( index ) ];
        // check correct branching
        if( codeInstr->m_addr && *codeInstr->m_addr == OPCODE_WIDE ) {
            // node ends in wide instruction
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                << ", method: " << method_get_name( ctex->m_method )
                << method_get_descriptor( ctex->m_method )
                << ") Illegal target of jump or branch" );
            return VER_ErrorBranch;
        }
        // set control flow edges
        if( codeInstr->m_offcount ) {
            for( unsigned count = 0; count < codeInstr->m_offcount; count++ ) {
#if _VERIFY_DEBUG
                if( code2node[ codeInstr->m_off[count] ] == 0 ) {
                    VERIFY_DEBUG( "vf_create_graph: error graph construction" );
                    vf_error();
                }
#endif // _VERIFY_DEBUG
                unsigned node = code2node[ codeInstr->m_off[count] ];
                ctex->m_graph->NewEdge( index, node );
                if( node < index ) {
                    // node has backward branch,
                    // thus the reference in local variables have to be initialized
                    ctex->m_graph->SetNodeInitFlag( node, true );
                }
            }
        } else {
            if( index + 1 == nodeCount - 1 ) {
                // set edge to end-entry node without return
                VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Falling off the end of the code" );
                return VER_ErrorBranch;
            }
            ctex->m_graph->NewEdge( index, index + 1 );
        }
        if( codeInstr->m_handler != NULL ) {
            // node is protected by exception handler,
            // thus the reference in local variables have to be initialized
            ctex->m_graph->SetNodeInitFlag( index, true );

            // set exception handler edges
            for( unsigned count = 0; count < handlcount; count++ ) {
                if( codeInstr->m_handler[count] ) {
                    // set edge to exception handler entry
                    ctex->m_graph->NewEdge( index, count + 1 );
                }
            }
        }
    }

#if _VERIFY_DEBUG
    if( ctex->m_dump.m_graph ) {
        ctex->m_graph->DumpGraph( ctex );
    }
    if( ctex->m_dump.m_dot_graph ) {
        ctex->m_graph->DumpDotGraph( ctex );
    }
#endif // _VERIFY_DEBUG

    return VER_OK;
} // vf_create_graph

/************************************************************
 *************** Graph Stack Deep Analysis ******************
 ************************************************************/

/**
 * Function evaluates stack deep of graph node.
 */
static int
vf_get_node_stack_deep( vf_Code_t *begin,   // begin code instruction of node
                        vf_Code_t *end)     // end code instruction of node
{
    int result = 0;
    vf_Code_t *pointer;

    /** 
     * For start, end and handler nodes
     */
    if( vf_is_instruction_has_flags( begin, 
            VF_FLAG_HANDLER | VF_FLAG_START_ENTRY | VF_FLAG_END_ENTRY ) )
    {
        return 0;
    }
#if _VERIFY_DEBUG
    if( begin > end ) {
        VERIFY_DEBUG( "vf_get_node_stack_deep: stack evaluation error" );
        vf_error();
    }
#endif // _VERIFY_DEBUG
    
    /**
     * Evaluate stack deep
     */
    for( pointer = begin; pointer <= end; pointer++ ) {
        result += pointer->m_stack;
    }
    return result;
} // vf_get_node_stack_deep

/**
 * Function provides some checks of control flow and data flow structures of graph.
 */
Verifier_Result
vf_graph_checks( vf_Context_t *ctex )   // verifier context
{
    unsigned count,
             inedge,
             innode;

    /**
     * Gem method max stack
     */
    vf_Graph_t *vGraph = ctex->m_graph;
    unsigned maxstack = method_get_max_stack( ctex->m_method );
    unsigned short handlcount = method_get_exc_handler_number( ctex->m_method );
    vf_Code_t *code = ctex->m_code;

    /**
     * Check stack deep correspondence
     */
    unsigned index = 1;
    Verifier_Result result = vf_check_stack_deep( 0, VERIFY_START_MARK,
        maxstack + VERIFY_START_MARK, &index, ctex );
    if( result != VER_OK ) {
        goto labelEnd_bypassGraphStructure;
    }
    assert( index <= vGraph->GetNodeNumber() );

    /**
     * Determine dead code nodes
     */
    index = vGraph->GetNodeNumber() - index; // number of dead code nodes

    /**
     * Override all dead nodes
     */
    if( index )
    {
        /** 
         * Identify dead code nodes and fill by nop instruction
         */
        for( index = handlcount + 1; index < vGraph->GetNodeNumber() - 1; index++ ) {
            if( !vGraph->IsNodeMarked( index ) ) {
                unsigned char *instr = code[ vGraph->GetNodeFirstInstr( index ) ].m_addr;
                for( count = 0; count < vGraph->GetNodeByteCodeLen( index ); count++ ) {
                    instr[count] = OPCODE_NOP;
                }
                vGraph->SetNodeStackModifier( index, 0 );
            }
        }
    }

#if _VERIFY_DEBUG
    if( ctex->m_dump.m_mod_graph ) {
        vGraph->DumpGraph( ctex );
    }
    if( ctex->m_dump.m_dot_mod_graph ) {
        vGraph->DumpDotGraph( ctex );
    }
#endif // _VERIFY_DEBUG

    /** 
     * Check code execution drops
     * Override all incoming edges to the end-entry node
     */
    for( inedge = vGraph->GetNodeFirstInEdge( vGraph->GetNodeNumber() - 1 );
         inedge;
         inedge = vGraph->GetEdgeNextInEdge( inedge ) )
    {
        // get incoming node
        innode = vGraph->GetEdgeStartNode( inedge );
        // check last node instruction, skip dead code nodes
        if( vGraph->IsNodeMarked( innode ) ) {
            // get node last instruction
            unsigned char *instr = code[ vGraph->GetNodeLastInstr( innode ) ].m_addr;
            if( !instr
               || !((*instr) >= OPCODE_IRETURN && (*instr) <= OPCODE_RETURN 
                            || (*instr) == OPCODE_ATHROW) )
            { // illegal instruction
                VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                    << ", method: " << method_get_name( ctex->m_method )
                    << method_get_descriptor( ctex->m_method )
                    << ") Falling off the end of the code" );
                result = VER_ErrorCodeEnd;
                goto labelEnd_bypassGraphStructure;
            }
        }
    }

    /**
     * Make data flow analysis
     */
    result = vf_check_graph_data_flow( ctex );

labelEnd_bypassGraphStructure:
    return result;
} // vf_graph_checks

/**
 * Function checks stack overflow of graph node instruction.
 */
static inline Verifier_Result
vf_check_node_stack_deep( unsigned nodenum,       // graph node number
                          int deep,               // initial stack deep
                          unsigned max_stack,     // maximal stack
                          vf_Context_t *ctex)     // verifier context
{
    /**
     * Get begin and end code instruction of graph node
     */
    unsigned begin = ctex->m_graph->GetNodeFirstInstr( nodenum );
    unsigned end = ctex->m_graph->GetNodeLastInstr( nodenum );
    assert( begin <= end );

    /** 
     * For start, end and handler nodes
     */
    if( vf_is_instruction_has_flags( &ctex->m_code[begin], 
            VF_FLAG_HANDLER | VF_FLAG_START_ENTRY | VF_FLAG_END_ENTRY ) )
    {
        return VER_OK;
    }
    
    /**
     * Evaluate stack deep
     */
    unsigned index;
    vf_Code_t *pointer;
    int stack_deep = 0;
    for( index = begin, pointer = &ctex->m_code[index]; index <= end; index++, pointer++ ) {
        if( pointer->m_minstack + VERIFY_START_MARK > stack_deep + deep ) {
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                << ", method: " << method_get_name( ctex->m_method )
                << method_get_descriptor( ctex->m_method )
                << ") Unable to pop operand off an empty stack" );
            return VER_ErrorStackOverflow;
        }
        stack_deep += pointer->m_stack;
        if( stack_deep + deep > (int)max_stack || stack_deep + deep < VERIFY_START_MARK ) {
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                << ", method: " << method_get_name( ctex->m_method )
                << method_get_descriptor( ctex->m_method )
                << ") Instruction stack overflow" );
            return VER_ErrorStackOverflow;
        }
    }
#if _VERIFY_DEBUG
    if( stack_deep != ctex->m_graph->GetNodeStackModifier( nodenum ) ) {
        VERIFY_DEBUG( "vf_check_node_stack_deep: error stack modifier calculate" );
        vf_error();
    }
#endif // _VERIFY_DEBUG

    return VER_OK;
} // vf_check_node_stack_deep

/**
 * Function checks graph nodes stack deep consistency. It's recursive function.
 * Function returns result of check.
 */
static Verifier_Result
vf_check_stack_deep( unsigned nodenum,       // graph node number
                     int stack_deep,         // initial stack deep of node
                     unsigned maxstack,      // maximal stack
                     unsigned *count,        // pointer to checked node count
                     vf_Context_t *ctex)     // verifier context
{
    int deep;
    unsigned outnode,
             outedge;
    Verifier_Result result = VER_OK;

    /**
     * Skip end-entry node
     */
    if( vf_is_instruction_has_flags( &ctex->m_code[ctex->m_graph->GetNodeFirstInstr( nodenum )],
                                     VF_FLAG_END_ENTRY) )
    {
        return VER_OK;
    }

    /**
     * Check handler node
     */
    if( vf_is_instruction_has_flags( &ctex->m_code[ctex->m_graph->GetNodeFirstInstr( nodenum )],
                                     VF_FLAG_HANDLER) )
    {
        // Reset stack for handler nodes
        stack_deep = VERIFY_START_MARK;
    }

    /**
     * Check node stack deep
     */
    deep = ctex->m_graph->GetNodeMark( nodenum );
    if( !deep ) {
        // stack deep don't set, mark node by his stack deep
        ctex->m_graph->SetNodeMark( nodenum, stack_deep );
        (*count)++;
    } else {
        if( stack_deep == deep ) {
            // consistent stack deep in graph
            return VER_OK;
        } else {
            // inconsistent stack deep in graph
            VERIFY_REPORT( ctex, "(class: " << class_get_name( ctex->m_class ) 
                << ", method: " << method_get_name( ctex->m_method )
                << method_get_descriptor( ctex->m_method )
                << ") Inconsistent stack deep: "
                << stack_deep - VERIFY_START_MARK << " != "
                << deep - VERIFY_START_MARK );
            return VER_ErrorStackDeep;
        }
    }

    /**
     * Check node stack overflow
     */
    result = vf_check_node_stack_deep( nodenum, stack_deep, maxstack, ctex );
    if( result != VER_OK ) {
        return result;
    }

    /** 
     * Override all out edges and set stack deep for out nodes
     */
    deep = stack_deep + ctex->m_graph->GetNodeStackModifier( nodenum );
    for( outedge = ctex->m_graph->GetNodeFirstOutEdge( nodenum );
         outedge;
         outedge = ctex->m_graph->GetEdgeNextOutEdge( outedge ) )
    {
        // get out node
        outnode = ctex->m_graph->GetEdgeEndNode( outedge );
        // mark out node with its out nodes
        result = vf_check_stack_deep( outnode, deep, maxstack, count, ctex );
        if( result != VER_OK ) {
            return result;
        }
    }
    return result;
} // vf_check_stack_deep

} // namescape Verifier
