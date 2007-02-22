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
#include "ver_graph.h"

/**
 * Function evaluates stack depth of graph node.
 */
static int
vf_get_node_stack_depth( vf_Code_t *start,  // from start instruction
                         vf_Code_t *end );  // to end instruction

/**
 * Function checks graph nodes stack depth consistency. It's recursive function.
 * Function returns result of check.
 */
static Verifier_Result
vf_check_stack_depth( unsigned nodenum,       // graph node number
                      int stack_depth,        // initial stack depth of node
                      unsigned maxstack,      // maximal stack
                      unsigned *count,        // pointer to checked node count
                      vf_Context_t *ctex);    // verifier context

/************************************************************
 ******************* Graph Implementation *******************
 ************************************************************/

/**
 * Function create graph nodes.
 */
void
vf_Graph::CreateNodes( unsigned number )    // number of nodes
{
    assert( number > 0 );
    vf_NodeContainer* nodes =
        (vf_NodeContainer*) AllocMemory( sizeof(vf_NodeContainer)
            + (number - 1) * sizeof(vf_Node) );
    nodes->m_max = number;
    nodes->m_next = m_nodes;
    nodes->m_max = number;
    if( m_nodes == NULL ) {
        m_nodes = nodes;
    } else {
        vf_NodeContainer* index = m_nodes->m_next;
        while( index->m_next ) {
            index = index->m_next;
        }
        index->m_next = nodes;
    }
} // vf_Graph::CreateNodes

/**
 * Gets graph node.
 */
vf_NodeHandle
vf_Graph::GetNode( unsigned node_num )  // graph node number
{
    // get node
    assert( m_nodes );
    assert( node_num < m_nodenum );
    unsigned count = node_num;
    vf_NodeContainer* nodes = m_nodes;
    while( count > nodes->m_max ) {
        count -= nodes->m_max;
        nodes = nodes->m_next;
        assert( nodes );
    }
    return &nodes->m_node[count];
} // vf_Graph::GetNode

/**
 * Creates a new node of a specific type.
 */
vf_NodeHandle
vf_Graph::NewNode( vf_NodeType_t type,  // node type
                   int stack)           // a stack modifier
{
    // get node
    assert( m_nodes );
    unsigned count = m_nodenum;
    vf_NodeContainer* nodes = m_nodes;
    while( count > nodes->m_max ) {
        count -= nodes->m_max;
        nodes = nodes->m_next;
        assert( nodes );
    }

    // increment nodes count
    m_nodenum++;
    nodes->m_used++;
    assert( nodes->m_used <= nodes->m_max );

    // set node
    vf_Node* node = &nodes->m_node[count];
    node->m_type = type;
    node->m_stack = stack;
    return node;
} // vf_Graph::NewNode

/**
 * Gets graph edge.
 */
vf_EdgeHandle
vf_Graph::GetEdge( unsigned edge_num )  // graph edge number
{
    // get edge
    assert( m_edges );
    assert( edge_num < m_edgenum );
    assert( edge_num );             // zero edge is reserved
    unsigned count = edge_num;
    vf_EdgeContainer* edges = m_edges;
    while( count > edges->m_max ) {
        count -= edges->m_max;
        edges = edges->m_next;
        assert( edges );
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
    vf_EdgeContainer* edges = m_edges;
    while( count > edges->m_max ) {
        count -= edges->m_max;
        edges = edges->m_next;
        assert( edges );
    }

    // get a new edge and edge's nodes
    vf_Edge* edge = &edges->m_edge[count];
    vf_Node* node_start = (vf_Node*) GetNode( start );
    vf_Node* node_end = (vf_Node*) GetNode( end );

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
 * Creates a data flow vector from the given example.
 */
void
vf_Graph::SetVector( vf_MapVectorHandle vector_handle,  // vector to set
                     vf_MapVectorHandle example,        // current data flow vector
                     bool need_copy)                    // copy flag
{
    assert( example );
    vf_MapVector* vector = (vf_MapVector*) vector_handle;

    // create and set local vector
    if( example->m_maxlocal ) {
        vector->m_local = (vf_MapEntry_t*) AllocMemory( example->m_maxlocal
            * sizeof( vf_MapEntry_t ) );
        assert( vector->m_local );
        vector->m_number = example->m_number;
        vector->m_maxlocal = example->m_maxlocal;
    }
    // create and set stack vector
    if( example->m_maxstack ) {
        vector->m_stack = (vf_MapEntry_t*) AllocMemory( example->m_maxstack
            * sizeof( vf_MapEntry_t ) );
        vector->m_depth = example->m_depth;
        vector->m_maxstack = example->m_maxstack;
    }
    if( need_copy ) {
        unsigned index;
        for( index = 0; index < example->m_number; index++ ) {
            vector->m_local[index] = example->m_local[index];
        }
        for( index = 0; index < example->m_depth; index++ ) {
            vector->m_stack[index] = example->m_stack[index];
        }
    }
} // vf_Graph::SetVector

/**
 * Function creates graph edges.
 */
void
vf_Graph::CreateEdges( unsigned number )        // number of edges
{
    assert( number > 0 );
    vf_EdgeContainer* edges = 
        (vf_EdgeContainer*) AllocMemory( sizeof(vf_EdgeContainer)
                    + number * sizeof(vf_Edge) );
    edges->m_max = number + 1;  // zero edge is reserved
    if( m_edges == NULL ) {
        m_edges = edges;
    } else {
        vf_EdgeContainer* index = m_edges->m_next;
        while( index->m_next ) {
            index = index->m_next;
        }
        index->m_next = edges;
    }
    return;
} // vf_Graph::CreateEdges

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
    vf_NodeContainer* nodes = m_nodes;
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
    vf_Node* node = (vf_Node*) GetNode( node_num );
    node->m_nodecount = 0;
    return;
} // vf_Graph::SetStartCountNode

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
    for( unsigned index = 0; index < GetNodeCount(); index++ ) {
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
        vf_EdgeHandle edge = GetEdge( edge_num );
        VERIFY_DEBUG( " [" << edge->m_start << "] -->" );
        edge_num = edge->m_innext;
    }

    vf_NodeHandle node = GetNode( num );
    // print node
    if( VF_TYPE_NODE_START_ENTRY == node->m_type)
    { // start node
        VERIFY_DEBUG( "node[" << num << "]: " << node->m_start << "[-] start" );
    } else if( VF_TYPE_NODE_END_ENTRY == node->m_type)
    { // end node
        VERIFY_DEBUG( "node[" << num << "]: " << node->m_start << "[-] end" );
        VERIFY_DEBUG( "-- end --" );
    } else if( VF_TYPE_NODE_HANDLER == node->m_type )
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
        vf_EdgeHandle edge = GetEdge( edge_num );
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
    size_t len = strlen( class_name ) + strlen( method_name )
                        + strlen( method_desc ) + 6;
    char *fname = (char*)STD_ALLOCA( len );
    sprintf( fname, "%s_%s%s.dot", class_name, method_name, method_desc );
    char* pointer = fname;
    while( pointer != NULL ) {
        switch( *pointer )
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
    vf_NodeHandle node = GetNode( num );

    // print node to dot file
    if( VF_TYPE_NODE_START_ENTRY == node->m_type )
    { // start node
        out << "node" << num << " [label=\"START\", color=limegreen]" << endl;
    } else if( VF_TYPE_NODE_END_ENTRY == node->m_type )
    { // end node
        out << "node" << num << " [label=\"END\", color=orangered]" << endl;
    } else if( VF_TYPE_NODE_HANDLER == node->m_type )
    { // handler node
        out << "node" << num << " [label=\"Handler #"
            << num << "\", shape=ellipse, color=aquamarine]" << endl;
    } else { // other nodes
        out << "node" << num 
            << " [label=\"";
        DumpDotNodeInternal( num, "\\n---------\\n", "\\l", out, ctex );
        out << "\"]" << endl;
    }

    // print node outcoming edges to dot file
    unsigned index;
    unsigned edge_num;
    for( index = 0, edge_num = node->m_outedge;
         index < node->m_outnum;
         index++ )
    {
        vf_EdgeHandle edge = GetEdge( edge_num );

        out << "node" << num << " -> " << "node" << edge->m_end;
        if( VF_TYPE_NODE_HANDLER == GetNode( edge->m_end )->m_type )
        {
            out << "[color=red]" << endl;
        } else if( ( VF_TYPE_NODE_CODE_RANGE == node->m_type )
            && ( VF_TYPE_INSTR_SUBROUTINE ==
                vf_get_last_instruction_type( ctex, edge->m_start ) ) )
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
 * Creates bytecode control flow graph.
 */
Verifier_Result
vf_create_graph( vf_Context_t* ctex )   // verifier context
{
    // allocate memory for graph structure
    void *mem_graph = vf_alloc_pool_memory(ctex->m_pool, sizeof(vf_Graph));

    // for creation of graph use numbers pre-calculated at vf_parse_bytecode
    ctex->m_graph = new(mem_graph) vf_Graph( ctex->m_nodeNum,
                        ctex->m_edgeNum, ctex->m_pool );
    vf_Graph* graph = ctex->m_graph;

    // the array contains a corresponding node for each instruction
    unsigned* code2node = (unsigned*)vf_alloc_pool_memory( ctex->m_pool,
                            ctex->m_codeNum * sizeof( unsigned ) );

    // create start-entry node
    graph->NewNode( VF_TYPE_NODE_START_ENTRY, 0 );

    // create handler nodes
    unsigned node_index;
    unsigned short handler_count = method_get_exc_handler_number( ctex->m_method );
    for (node_index = 1; node_index <= (unsigned) handler_count; node_index++) {
        graph->NewNode( VF_TYPE_NODE_HANDLER, 1 );
    }

    /**
     * Create code range nodes. New node correspond to the subsequent
     * basic blocks of instructions.
     * Note: code range nodes follow after the last handler node.
     */
    // adding a basic block which starts
    for( unsigned bb_start = 0;
         bb_start < ctex->m_codeNum;
         node_index++ )
    {
        // find a basic block end
        unsigned next_bb_start = bb_start + 1;
        while ((next_bb_start < ctex->m_codeNum)
            && (!ctex->m_code[next_bb_start].m_basic_block_start))
        {
            next_bb_start++;
        }

        int stack = vf_get_node_stack_depth( &ctex->m_code[bb_start],
            &ctex->m_code[next_bb_start - 1] );
        graph->NewNode( bb_start, next_bb_start - 1, stack );
        code2node[bb_start] = node_index;
        bb_start = next_bb_start;
    }

    // create exit-entry node
    graph->NewNode( VF_TYPE_NODE_END_ENTRY, 0 );
    unsigned node_num = node_index + 1;
    assert( ctex->m_nodeNum == node_num );

    /**
     * Create edges
     */
    
    // from start-entry node to the first code node
    node_index = handler_count + 1;
    graph->NewEdge( 0, node_index );

    // create code range edges
    for (; node_index < node_num - 1; node_index++) {
        vf_Code_t* code = &ctex->m_code[graph->GetNodeLastInstr( node_index )];
         
        // set control flow edges
        if( code->m_offcount ) {
            for( unsigned count = 0; count < code->m_offcount; count++ ) {
                int offset = vf_get_code_branch( code, count );
                unsigned node = code2node[ctex->m_bc[offset].m_instr - 1];
                assert( node );
                
                graph->NewEdge( node_index, node );
                if( node < node_index ) {
                    // node has a backward branch, thus any
                    // object on the stack should be initialized
                    graph->SetNodeInitFlag( node, true );
                }
            }
        } else if (code->m_type) {
            // FIXME compatibility issue - no need to
            // have these branches for VF_TYPE_INSTR_SUBROUTINE
            graph->NewEdge( node_index, node_num - 1 );
        } else if (node_index + 1 < node_num) {
            graph->NewEdge( node_index, node_index + 1 );
        } else {
            VERIFY_REPORT_METHOD( ctex, "Falling off the end of the code" );
            return VER_ErrorBranch;
        }

    }

    // create OUT map vectors for handler nodes
    unsigned char* start_bc = method_get_bytecode( ctex->m_method );
    unsigned bytecode_len = method_get_code_length( ctex->m_method );
    for (unsigned short handler_index = 0;
        handler_index < handler_count;
        handler_index++)
    {
        unsigned short start_pc, end_pc, handler_pc, handler_cp_index;
        method_get_exc_handler_info( ctex->m_method,
            handler_index, &start_pc, &end_pc,
            &handler_pc, &handler_cp_index );

        vf_ValidType_t *type = NULL;
        if (handler_cp_index) {
            const char* name = vf_get_cp_class_name( ctex->m_class,
                handler_cp_index );
            assert( name );
            type = vf_create_class_valid_type( name, ctex );

            // set restriction for handler class
            if( ctex->m_vtype.m_throwable->string[0] != type->string[0] ) {
                ctex->m_type->SetRestriction(
                    ctex->m_vtype.m_throwable->string[0],
                    type->string[0], 0, VF_CHECK_SUPER);
            }
        }

        /**
         * Create out vector for a handler node
         * Note:
         *    When the out stack map vector is merged with
         *    incoming map vector, local variables map vector will be
         *    created with during the process of merge.
         */
        vf_MapVector* p_outvector = (vf_MapVector*)
            graph->GetNodeOutVector( handler_index + 1 );
        p_outvector->m_stack = 
            (vf_MapEntry_t*) graph->AllocMemory(sizeof(vf_MapEntry_t));
        p_outvector->m_depth = 1;
        vf_set_vector_stack_entry_ref( p_outvector->m_stack, 0, type );

        // outcoming handler edge
        graph->NewEdge( handler_index + 1,
            code2node[ctex->m_bc[handler_pc].m_instr - 1] );
        
        // node range start
        node_index = code2node[ctex->m_bc[start_pc].m_instr - 1];
        
        unsigned last_node = (end_pc == bytecode_len)
            ? node_num - 1
            : code2node[ctex->m_bc[end_pc].m_instr - 1];

        for (; node_index < last_node; node_index++) {
            // node is protected by exception handler, thus the 
            // reference in local variables have to be initialized
            graph->SetNodeInitFlag( node_index, true );
            graph->NewEdge( node_index, handler_index + 1 );
        }
    }
    // one edge is reserved
    assert( graph->GetEdgeCount() == ctex->m_edgeNum ); 

#if _VERIFY_DEBUG
    if( ctex->m_dump.m_graph ) {
        graph->DumpGraph( ctex );
    }
    if( ctex->m_dump.m_dot_graph ) {
        graph->DumpDotGraph( ctex );
    }
#endif // _VERIFY_DEBUG

    return VER_OK;
} // vf_create_graph

/************************************************************
 *************** Graph Stack Deep Analysis ******************
 ************************************************************/

/**
 * Function evaluates stack depth of graph code range node.
 */
static int
vf_get_node_stack_depth( vf_Code_t *start,  // beginning instruction
                         vf_Code_t *end)    // ending instruction
{
    assert( start <= end );
    
    /**
     * Evaluate stack depth
     */
    int result = 0;
    for( vf_Code_t* pointer = start; pointer <= end; pointer++ ) {
        result += pointer->m_stack;
    }
    return result;
} // vf_get_node_stack_depth


/**
 * Function provides some checks of control flow and data flow structures of graph.
 */
Verifier_Result
vf_check_graph( vf_Context_t *ctex )   // verifier context
{
    unsigned count,
             inedge,
             innode;

    /**
     * Gem method max stack
     */
    vf_Graph* vGraph = ctex->m_graph;
    unsigned maxstack = method_get_max_stack( ctex->m_method );
    unsigned short handlcount = method_get_exc_handler_number( ctex->m_method );
    vf_Code_t *code = ctex->m_code;

    /**
     * Check stack depth correspondence
     */
    unsigned index = 1;
    Verifier_Result result = vf_check_stack_depth( 0, VERIFY_START_MARK,
        maxstack + VERIFY_START_MARK, &index, ctex );
    if( result != VER_OK ) {
        goto labelEnd_bypassGraphStructure;
    }
    assert( index <= vGraph->GetNodeCount() );

    /**
     * Determine dead code nodes
     */
    index = vGraph->GetNodeCount() - index; // number of dead code nodes

    /**
     * Override all dead nodes
     */
    if( index )
    {
        /** 
         * Identify dead code nodes and fill by nop instruction.
         */
        for( index = handlcount + 1; index < vGraph->GetNodeCount() - 1; index++ ) {
            if( !vGraph->IsNodeMarked( index ) ) {
                unsigned char *instr = code[ vGraph->GetNodeFirstInstr( index ) ].m_addr;
                unsigned len = vGraph->GetNodeBytecodeLen( ctex, vGraph->GetNode( index ) );
                for( count = 0; count < len; count++ ) {
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
     * Check that execution flow terminates with
     * return or athrow bytecodes.
     * Override all incoming edges to the end-entry node.
     */
    for( inedge = vGraph->GetNodeFirstInEdge( vGraph->GetNodeCount() - 1 );
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
                VERIFY_REPORT_METHOD( ctex, "Falling off the end of the code" );
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
} // vf_check_graph

/**
 * Frees memory allocated for graph, if any.
 */
void
vf_free_graph( vf_Context_t *context )   // verifier context
{
    if( context->m_graph ) {
        context->m_graph->~vf_Graph();
        context->m_graph = NULL;
    }
} // vf_free_graph

/**
 * Function checks stack overflow of graph node instruction.
 */
static inline Verifier_Result
vf_check_node_stack_depth( unsigned nodenum,       // graph node number
                           int depth,              // initial stack depth
                           unsigned max_stack,     // maximal stack
                           vf_Context_t *ctex)     // verifier context
{
    // get checked node
    vf_NodeHandle node = ctex->m_graph->GetNode( nodenum );

    /** 
     * For start, end and handler nodes
     */
    if( node->m_type != VF_TYPE_NODE_CODE_RANGE ) {
        return VER_OK;
    }
    
    /**
     * Get begin and end code instruction of graph node
     */
    unsigned start = node->m_start;
    unsigned end = node->m_end;
    assert( start <= end );
    
    /**
     * Evaluate stack depth
     */
    unsigned index;
    vf_Code_t *pointer;
    int stack_depth = 0;
    for( index = start, pointer = &ctex->m_code[index]; index <= end; index++, pointer++ ) {
        if( pointer->m_minstack + VERIFY_START_MARK > stack_depth + depth ) {
            VERIFY_REPORT_METHOD( ctex, "Unable to pop operand off an empty stack" );
            return VER_ErrorStackOverflow;
        }
        stack_depth += pointer->m_stack;
        if( stack_depth + depth > (int)max_stack || stack_depth + depth < VERIFY_START_MARK ) {
            VERIFY_REPORT_METHOD( ctex, "Instruction stack overflow" );
            return VER_ErrorStackOverflow;
        }
    }
#if _VERIFY_DEBUG
    if( stack_depth != ctex->m_graph->GetNodeStackModifier( nodenum ) ) {
        VERIFY_DEBUG( "vf_check_node_stack_depth: error stack modifier calculate" );
        vf_error();
    }
#endif // _VERIFY_DEBUG

    return VER_OK;
} // vf_check_node_stack_depth

/**
 * Function checks graph nodes stack depth consistency. It's recursive function.
 * Function returns result of check.
 */
static Verifier_Result
vf_check_stack_depth( unsigned nodenum,       // graph node number
                     int stack_depth,         // initial stack depth of node
                     unsigned maxstack,      // maximal stack
                     unsigned *count,        // pointer to checked node count
                     vf_Context_t *ctex)     // verifier context
{
    // get checked node
    vf_NodeHandle node = ctex->m_graph->GetNode( nodenum );

    /**
     * Skip end-entry node
     */
    if( VF_TYPE_NODE_END_ENTRY == node->m_type ) {
        return VER_OK;
    }

    /**
     * Check handler node
     */
    if( VF_TYPE_NODE_HANDLER == node->m_type ) {
        // Reset stack for handler nodes
        stack_depth = VERIFY_START_MARK;
    }

    /**
     * Check node stack depth
     */
    int depth = ctex->m_graph->GetNodeMark( nodenum );
    if( !depth ) {
        // stack depth don't set, mark node by his stack depth
        ctex->m_graph->SetNodeMark( nodenum, stack_depth );
        (*count)++;
    } else {
        if( stack_depth == depth ) {
            // consistent stack depth in graph
            return VER_OK;
        } else {
            // inconsistent stack depth in graph
            VERIFY_REPORT_METHOD( ctex, "Inconsistent stack depth: "
                << stack_depth - VERIFY_START_MARK << " != "
                << depth - VERIFY_START_MARK );
            return VER_ErrorStackDeep;
        }
    }

    /**
     * Check node stack overflow
     */
    Verifier_Result result = 
        vf_check_node_stack_depth( nodenum, stack_depth, maxstack, ctex );
    if( result != VER_OK ) {
        return result;
    }

    /** 
     * Override all out edges and set stack depth for out nodes
     */
    depth = stack_depth + ctex->m_graph->GetNodeStackModifier( nodenum );
    for( unsigned outedge = ctex->m_graph->GetNodeFirstOutEdge( nodenum );
         outedge;
         outedge = ctex->m_graph->GetEdgeNextOutEdge( outedge ) )
    {
        // get out node
        unsigned outnode = ctex->m_graph->GetEdgeEndNode( outedge );
        // mark out node with its out nodes
        result = vf_check_stack_depth( outnode, depth, maxstack, count, ctex );
        if( result != VER_OK ) {
            return result;
        }
    }
    return result;
} // vf_check_stack_depth


