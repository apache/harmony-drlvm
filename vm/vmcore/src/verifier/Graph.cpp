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

/************************************************************
 **************** Debug Graph Implementation ****************
 ************************************************************/

/**
 * Prints graph structure in stderr.
 */
void vf_Graph::DumpGraph()
{
#if _VF_DEBUG
    VF_DEBUG("Method: " << class_get_name(m_ctx->m_class) << "::"
             << m_ctx->m_name << m_ctx->m_descriptor << endl);
    VF_DEBUG("-- start --");
    ResetNodeIterator();
    while (HasMoreElements()) {
        DumpNode(GetNextNode());
    }
#endif // _VF_DEBUG
}                               // vf_Graph::DumpGraph

/**
 * Prints graph node in stderr.
 */
void vf_Graph::DumpNode(vf_NodeHandle node)     // a graph node
{
#if _VF_DEBUG
    // print node incoming edges
    vf_EdgeHandle edge;
    for (edge = node->m_inedge; edge; edge = edge->m_innext) {
        VF_DEBUG(" [" << GetNodeNum(edge->m_start) << "] -->");
    }

    switch (node->m_type) {
    case VF_NODE_START_ENTRY:
        VF_DEBUG("node[" << GetNodeNum(node) << "]: start node");
        break;
    case VF_NODE_END_ENTRY:
        VF_DEBUG("node[" << GetNodeNum(node) << "]: end node");
        break;
    case VF_NODE_HANDLER:
        VF_DEBUG("node[" << GetNodeNum(node) << "]: exception handler");
        break;
    default:
        DumpNodeInternal(node);
    }

    // print node outcoming edges
    for (edge = node->m_outedge; edge; edge = edge->m_outnext) {
        VF_DEBUG(" --> [" << GetNodeNum(edge->m_end) << "]");
    }
    VF_DEBUG("");
#endif // _VF_DEBUG
}                               // vf_Graph::DumpNode

/**
 * Prints graph node instruction in stream.
 */
void vf_Graph::DumpNodeInternal(vf_NodeHandle node)     // graph node number
{
#if _VF_DEBUG
    // print node header
    VF_DEBUG("Node #" << GetNodeNum(node));
    VF_DEBUG("Stack mod: " << node->m_stack);
    DumpSub(node->m_sub);

    // get code instructions
    unsigned count = node->m_end - node->m_start + 1;
    vf_InstrHandle instr = node->m_start;

    // print node instructions
    for (unsigned index = 0; index < count; index++, instr++) {
        VF_DEBUG(index << ": " << ((instr->m_stack < 0) ? "[" : "[ ")
                 << instr->m_stack << "| " << instr->m_minstack << "] "
                 << vf_opcode_names[*(instr->m_addr)]);
    }
#endif // _VF_DEBUG
}                               // vf_Graph::DumpNodeInternal


/**
 * Dumps graph node in file in DOT format.
 */
void vf_Graph::DumpDotGraph()
{
#if _VF_DEBUG
    /**
     * Graphviz has a hardcoded label length limit. Windows has a file
     * name length limit as well.
     */
    const int MAX_LABEL_LENGTH = 80;

    // get class and method name
    const char *class_name = class_get_name(m_ctx->m_class);

    // create file name
    size_t len = strlen(class_name) + strlen(m_ctx->m_name)
        + strlen(m_ctx->m_descriptor) + 6;
    char *fname = (char *) STD_ALLOCA(len);
    sprintf(fname, "%s_%s%s", class_name, m_ctx->m_name, m_ctx->m_descriptor);

    char *f_start;
    char *pointer;
    if (len > MAX_LABEL_LENGTH) {
        f_start = fname + len - MAX_LABEL_LENGTH;
        // shift to the start of the nearest lexem
        for (pointer = f_start;; pointer++) {
            if (isalnum(*pointer)) {
                continue;
            } else if (!*pointer) {     // end of the string
                break;
            } else {
                // record the first matching position
                f_start = pointer;
                break;
            }
        }
    } else {
        f_start = fname;
    }

    for (pointer = f_start;; pointer++) {
        if (isalnum(*pointer)) {
            continue;
        } else if (!*pointer) { // end of the string
            break;
        } else {
            *pointer = '_';
        }
    }
    // pointer currently points to the end of the string
    sprintf(pointer, ".dot");

    // create .dot file
    ofstream fout(f_start);
    if (fout.fail()) {
        VF_DEBUG("vf_Graph::DumpDotGraph: error opening file: " << f_start);
        return;
    }
    // create name of graph
    sprintf(fname, "%s.%s%s", class_name, m_ctx->m_name, m_ctx->m_descriptor);

    // print graph to file
    DumpDotHeader(f_start, fout);
    ResetNodeIterator();
    while (HasMoreElements()) {
        DumpDotNode(GetNextNode(), fout);
    }
    DumpDotEnd(fout);

    // close file
    fout.flush();
    fout.close();
#endif // _VF_DEBUG
}                               // vf_Graph::DumpDotGraph

/**
 * Dumps graph header in file in DOT format.
 */
void vf_Graph::DumpDotHeader(const char *graph_name,    // graph name
                             ofstream &out)     // output file stream
{
#if _VF_DEBUG
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

#endif // _VF_DEBUG
}                               // vf_Graph::DumpDotHeader

/**
 * Dumps graph node in file in DOT format.
 */
void vf_Graph::DumpDotNode(vf_NodeHandle node,  // graph node number
                           ofstream &out)       // output file stream
{
#if _VF_DEBUG
    // print node to dot file
    out << "node" << GetNodeNum(node);
    if (VF_NODE_START_ENTRY == node->m_type) {  // start node
        out << " [label=\"START\", color=limegreen]" << endl;
    } else if (VF_NODE_END_ENTRY == node->m_type) {     // end node
        out << " [label=\"END\", color=orangered]" << endl;
    } else if (VF_NODE_HANDLER == node->m_type) {       // handler node
        out << " [label=\"Handler #"
            << GetNodeNum(node) << "\", shape=ellipse, color=";
        if (node->m_sub) {
            out << "\"#B7FFE" << hex
                << (vf_get_sub_num(node->m_sub, m_ctx) % 16) << dec << "\"";
        } else {
            out << "aquamarine";
        }
        out << "]" << endl;
    } else {                    // other nodes
        DumpDotNodeInternal(node, "\\n---------\\n", "\\l", out);
    }

    // print node outcoming edges to dot file
    for (vf_EdgeHandle edge = node->m_outedge; edge; edge = edge->m_outnext) {
        out << "node" << GetNodeNum(node) << " -> "
            << "node" << GetNodeNum(edge->m_end);
        if (VF_NODE_HANDLER == edge->m_end->m_type) {
            out << "[color=red]" << endl;
        } else if (vf_is_jsr_branch(edge, m_ctx)) {
            out << "[color=blue]" << endl;
        }
        out << ";" << endl;
    }
#endif // _VF_DEBUG
}                               // vf_Graph::DumpDotNode

/**
 * Dumps graph node instruction in file stream in DOT format.
 */
void vf_Graph::DumpDotNodeInternal(vf_NodeHandle node,  // graph node number
                                   char *next_node,     // separator between nodes in stream
                                   char *next_instr,    // separator between instructions in stream
                                   ofstream &out)       // output file stream
{
#if _VF_DEBUG
    // print node header
    out << " [label=\"";
    out << "Node " << GetNodeNum(node) << next_node
        << "Stack mod: " << node->m_stack << next_node;

    // get code instructions
    unsigned count = node->m_end - node->m_start + 1;
    vf_InstrHandle instr = node->m_start;

    // print node instructions
    for (unsigned index = 0; index < count; index++, instr++) {
        out << index << ": " << ((instr->m_stack < 0) ? "[" : "[ ")
            << instr->m_stack << "\\| " << instr->m_minstack << "] "
            << vf_opcode_names[*(instr->m_addr)] << next_instr;
    }
    out << "\"" << endl;

    if (node->m_sub) {
        out << ", color=\"#B2F" << hex
            << (vf_get_sub_num(node->m_sub, m_ctx) % 16) << dec << "EE\"";
    }
    out << "]" << endl;
#endif // _VF_DEBUG
}                               // vf_Graph::DumpDotNodeInternal

/**
 * Dumps graph end in file in DOT format.
 */
void vf_Graph::DumpDotEnd(ofstream &out)        // output file stream
{
#if _VF_DEBUG
    out << "}" << endl;
#endif // _VF_DEBUG
}                               // vf_Graph::DumpDotEnd


/************************************************************
 ********************** Graph Creation **********************
 ************************************************************/
/**
 * Creates bytecode control flow graph.
 */
vf_Result vf_create_graph(vf_Context *ctx)      // verification context
{
    vf_BCode *bc = ctx->m_bc;

    /** 
     * Count edges from basic blocks from exception range to the
     * corresponding exception handlers.
     */
    unsigned edges = 0;
    unsigned short handler_count = ctx->m_handlers;
    for (unsigned short handler_index = 0; handler_index < handler_count;
         handler_index++) {
        unsigned short start_pc, end_pc, handler_pc, handler_cp_index, count;
        method_get_exc_handler_info(ctx->m_method, handler_index,
                                    &start_pc, &end_pc, &handler_pc,
                                    &handler_cp_index);

        // number of basic blocks in the exception range
        unsigned handler_edges = 0;
        for (count = start_pc; count < end_pc; count++) {
            if (bc[count].m_is_bb_start) {
                handler_edges++;
            }
        }

        edges += handler_edges;
    }

    /** 
     * Initialize a node counter with handler nodes
     * and 2 terminator nodes.
     */
    unsigned nodes = handler_count + 2;

    /**
     * Check instruction offsets, count basic blocks and edges.
     */
    for (unsigned index = 0; index < ctx->m_len; index++) {
        vf_InstrHandle instr = bc[index].m_instr;
        if (NULL == instr) {
            if (bc[index].m_is_bb_start) {
                VF_REPORT(ctx, "Illegal target of jump or branch");
                return VF_ErrorBranch;
            } else {
                continue;
            }
        }

        if (bc[index].m_is_bb_start) {
            ((vf_Instr *) instr)->m_is_bb_start = true;
            nodes++;
        }

        if (instr->m_offcount) {
            // basic block should start next, so we will
            // count one branch anyway
            edges += instr->m_offcount - 1;
        }
    }

    /**
     * Each node except the end node and <code>ret</code> nodes emits at least one
     * branch.
     */
    edges += nodes - 1 - ctx->m_retnum;

    // allocate memory for graph structure
    void *mem_graph = vf_palloc(ctx->m_pool, sizeof(vf_Graph));
    // for graph creation use pre-calculated numbers
    vf_Graph *graph = new(mem_graph) vf_Graph(nodes, edges, ctx);
    ctx->m_graph = graph;

    /**
     * Create nodes.
     */
    graph->NewNode(VF_NODE_START_ENTRY);

    // create handler nodes
    unsigned node_index;
    for (node_index = 1; node_index <= (unsigned) handler_count; node_index++) {
        graph->NewNode(VF_NODE_HANDLER);
    }

    // create code range nodes right after the last handler node
    // a new node correspond to a subsequent basic block of instructions
    for (vf_InstrHandle bb_start = ctx->m_instr;
         bb_start < ctx->m_last_instr; node_index++) {
        // find a basic block end
        vf_InstrHandle next_bb_start = bb_start + 1;
        while ((next_bb_start < ctx->m_last_instr)
               && (!next_bb_start->m_is_bb_start)) {
            next_bb_start++;
        }

        ((vf_Instr *) bb_start)->m_node =
            graph->NewNode(bb_start, next_bb_start - 1);
        bb_start = next_bb_start;
    }

    // create exit-entry node
    graph->AddEndNode();
    unsigned node_num = node_index + 1;
    assert(0 == graph->HasMoreNodeSpace());

    /**
     * Create edges.
     */
    vf_Node *node = (vf_Node *) graph->GetNode(handler_count + 1);

    // from start-entry node to the first code range node
    graph->NewEdge((vf_Node *) graph->GetStartNode(), node);

    // create code range edges
    for (; VF_NODE_CODE_RANGE == node->m_type; node++) {
        vf_InstrHandle instr = node->m_end;

        // set control flow edges
        if (instr->m_offcount) {
            for (unsigned count = 0; count < instr->m_offcount; count++) {
                int offset = vf_get_instr_branch(instr, count);
                vf_Node *next_node =
                    (vf_Node *) graph->GetNodeFromBytecodeIndex(offset);
                assert(node);

                graph->NewEdge(node, next_node);
                if (next_node < node) {
                    // node has a backward branch, thus any
                    // object on the stack should be initialized
                    node->m_initialized = true;
                }
            }
        } else if (VF_INSTR_RETURN == instr->m_type
                   || VF_INSTR_THROW == instr->m_type) {
            graph->NewEdge(node, (vf_Node *) graph->GetEndNode());
        } else if (VF_INSTR_RET == instr->m_type) {
            // no edges from ret
        } else if (VF_NODE_CODE_RANGE == (node + 1)->m_type) {
            graph->NewEdge(node, node + 1);
        } else {
            VF_REPORT(ctx, "Falling off the end of the code");
            return VF_ErrorBranch;
        }

    }
    assert(VF_NODE_END_ENTRY == node->m_type);

    // create OUT map vectors for handler nodes
    unsigned char *start_bc = method_get_bytecode(ctx->m_method);
    for (unsigned short handler_index = 0;
         handler_index < handler_count; handler_index++) {
        unsigned short start_pc, end_pc, handler_pc, handler_cp_index;
        method_get_exc_handler_info(ctx->m_method,
                                    handler_index, &start_pc, &end_pc,
                                    &handler_pc, &handler_cp_index);

        vf_ValidType *type = NULL;
        if (handler_cp_index) {
            const char *name = vf_get_cp_class_name(ctx->m_class,
                                                    handler_cp_index);
            assert(name);
            type = vf_create_class_valid_type(name, ctx);

            // set restriction for handler class
            if (ctx->m_vtype.m_throwable->string[0] != type->string[0]) {
                ctx->m_type->SetRestriction(ctx->m_vtype.m_throwable->
                                            string[0], type->string[0], 0,
                                            VF_CHECK_SUPER);
            }
        }

        /**
         * Create out vector for a handler node
         * Note:
         *    When the out stack map vector is merged with
         *    incoming map vector, local variables map vector will be
         *    created with during the process of merge.
         */
        vf_MapVector *p_outvector = (vf_MapVector *)
            &graph->GetNode(handler_index + 1)->m_outmap;
        p_outvector->m_stack =
            (vf_MapEntry *) graph->AllocMemory(sizeof(vf_MapEntry) *
                                               ctx->m_maxstack);
        p_outvector->m_depth = 1;
        vf_set_vector_stack_entry_ref(p_outvector->m_stack, 0, type);

        // outcoming handler edge
        vf_Node *handler_node = (vf_Node *) graph->GetNode(handler_index + 1);
        graph->NewEdge(handler_node,
                       (vf_Node *) graph->
                       GetNodeFromBytecodeIndex(handler_pc));

        // node range start
        node = (vf_Node *) graph->GetNodeFromBytecodeIndex(start_pc);

        vf_NodeHandle last_node = (end_pc == ctx->m_len)
            ? graph->GetEndNode()
            : graph->GetNodeFromBytecodeIndex(end_pc);

        for (; node < last_node; node++) {
            // node is protected by exception handler, thus the 
            // reference in local variables have to be initialized
            node->m_initialized = true;
            graph->NewEdge(node, handler_node);
        }
    }
    // one edge is reserved
    assert(0 == graph->HasMoreEdgeSpace());

    VF_DUMP(DUMP_GRAPH, graph->DumpGraph());
    VF_DUMP(DUMP_DOT, graph->DumpDotGraph());

    return VF_OK;
}                               // vf_create_graph

/************************************************************
 *************** Graph Stack Depth Analysis *****************
 ************************************************************/
/**
 * Checks stack depth of a node.
 */
vf_Result vf_check_node_stack_depth(vf_Node *node,      // a graph node
                                    unsigned &depth,    // initial stack depth
                                    vf_Context *ctx)    // verification context
{
    if (node->m_mark) {
        if ((depth == node->m_inmap.m_depth)
            || (VF_NODE_CODE_RANGE != node->m_type)) {
            // consistent stack depth in the node
            return VF_OK;
        } else {
            // inconsistent stack depth
            VF_REPORT(ctx, "Inconsistent stack depth: "
                      << node->m_inmap.m_depth << " != " << depth);
            return VF_ErrorStackDepth;
        }
    }
    // mark the node
    node->m_mark = VF_START_MARK;

    // count the node
    vf_Graph *graph = ctx->m_graph;
    graph->IncrementReachableCount();
    node->m_inmap.m_depth = depth;
    VF_TRACE("node", "node[" << ctx->m_graph->GetNodeNum(node)
             << "].in.depth := " << node->m_inmap.m_depth);

    if (VF_NODE_CODE_RANGE != node->m_type) {
        if (VF_NODE_HANDLER == node->m_type) {
            depth = 1;
            node->m_stack = 1;
            node->m_outmap.m_depth = 1;
        } else if (VF_NODE_END_ENTRY == node->m_type) {
            return VF_OK;
        }
        return VF_Continue;
    }
    // calculate a stack depth after the last node instruction
    int stack_depth = 0;
    for (vf_InstrHandle instr = node->m_start; instr <= node->m_end; instr++) {
        if (instr->m_minstack > depth) {
            VF_REPORT(ctx,
                      "Unable to pop operands needed for an instruction");
            return VF_ErrorStackUnderflow;
        }
        stack_depth += instr->m_stack;
        depth += instr->m_stack;
        assert(depth >= 0);
        if (depth > ctx->m_maxstack) {
            VF_REPORT(ctx, "Instruction stack overflow");
            return VF_ErrorStackOverflow;
        }
    }

    node->m_stack = stack_depth;
    node->m_outmap.m_depth = depth;
    VF_TRACE("node", "node[" << ctx->m_graph->GetNodeNum(node)
             << "].out.depth := " << node->m_outmap.m_depth);
    return VF_Continue;
}                               // vf_check_node_stack_depth

/**
 * Checks graph nodes stack depth consistency recursively.
 * Returns result of a check.
 */
static vf_Result vf_check_stack_depth(vf_Node *node,    // a graph node
                                      unsigned stack_depth,     // initial stack depth of node
                                      vf_Context *ctx)  // verification context
{
    // check for node stack overflow
    vf_Result result = vf_check_node_stack_depth(node, stack_depth, ctx);
    if (VF_Continue != result) {
        return result;
    }
    // iterate over out edges and set stack depth for out nodes
    for (vf_EdgeHandle outedge = node->m_outedge;
         outedge; outedge = outedge->m_outnext) {
        // get out node
        vf_Node *outnode = (vf_Node *) outedge->m_end;
        // mark out node with its out nodes
        result = vf_check_stack_depth(outnode, stack_depth, ctx);
        if (VF_OK != result) {
            return result;
        }
    }
    return VF_OK;
}                               // vf_check_stack_depth

/**
 * Removes <i>in</i> edge from the list of the corresponding end node.
 */
static void vf_remove_inedge(vf_EdgeHandle edge)        // verification context
{
    vf_EdgeHandle *p_next_edge = (vf_EdgeHandle *) & edge->m_end->m_inedge;
    vf_Edge *inedge = (vf_Edge *) edge->m_end->m_inedge;
    while (inedge) {
        if (inedge == edge) {
            // remove the edge from the list
            *p_next_edge = inedge->m_innext;
            return;
        }
        p_next_edge = &inedge->m_innext;
        inedge = (vf_Edge *) inedge->m_innext;
    }
    VF_DIE("vf_remove_inedge: Cannot find an IN edge " << edge
           << " from the list of the node " << edge->m_end);
}

/**
 * Scans dead nodes, removes obsolete edges and fills corresponding bytecode by nop
 * instruction.
 */
static void vf_nullify_unreachable_bytecode(vf_ContextHandle ctx)       // verification context
{
    vf_Node *node =
        (vf_Node *) ctx->m_graph->GetStartNode()->m_outedge->m_end;
    for (; node->m_type != VF_NODE_END_ENTRY; node++) {
        assert(VF_NODE_CODE_RANGE == node->m_type);
        if (!node->m_mark) {
            VF_TRACE("node",
                     "Cleaning unreachable node #" << ctx->m_graph->
                     GetNodeNum(node));
            unsigned char *instr = node->m_start->m_addr;
            const unsigned char *end = vf_get_instr_end(node->m_end, ctx);
            while (instr < end) {
                *(instr++) = OPCODE_NOP;
            }
            node->m_stack = 0;
            // we assume that java compiler generally doesn't generate a dead code,
            // so we rarely remove edges here is rare
            for (vf_EdgeHandle edge = node->m_outedge; edge;
                 edge = edge->m_outnext) {
                assert(edge->m_start == node);
                VF_TRACE("edge", "Cleaning unreachable edge #"
                         << ctx->m_graph->GetNodeNum(node) << " -> #"
                         << ctx->m_graph->GetNodeNum(edge->m_end));
                if (edge->m_end->m_mark) {
                    // remove edges to reachable nodes
                    vf_remove_inedge(edge);
                }
            }
            node->m_inedge = node->m_outedge = NULL;
            node->m_sub = NULL;
            node->m_outnum = 0;
        }
    }
}

/**
 * Provides checks of control flow and data flow structures of graph.
 */
vf_Result vf_check_graph(vf_Context *ctx)       // verification context
{
    vf_Graph *graph = ctx->m_graph;
    vf_InstrHandle instr = ctx->m_instr;

    vf_Result result;

    // allocate a current stack map vector
    ctx->m_map = (vf_MapVector *) vf_palloc(ctx->m_pool,
                                            sizeof(vf_MapVector));
    ctx->m_graph->AllocVector(ctx->m_map);

    // create a buf stack map vector (max 4 entries)
    ctx->m_buf = (vf_MapEntry *) vf_palloc(ctx->m_pool, sizeof(vf_MapEntry)
                                           * ctx->m_maxstack);

    if (ctx->m_retnum) {
        result = vf_mark_subroutines(ctx);
    } else {
        result =
            vf_check_stack_depth((vf_Node *) graph->GetStartNode(), 0, ctx);
    }
    if (VF_OK != result) {
        return result;
    }
    // there could be only <code>nop</code> opcodes in the bytecode, in this case 
    // the code is already checked during previous step
    if (ctx->m_maxstack == 0) {
        return VF_OK;
    }
    // override all dead nodes
    assert(graph->GetReachableNodeCount() <= graph->GetNodeCount());
    if (graph->GetReachableNodeCount() < graph->GetNodeCount()) {
        vf_nullify_unreachable_bytecode(ctx);
    }

    if (ctx->m_retnum) {
        result = vf_inline_subroutines(ctx);
        if (VF_OK != result) {
            return result;
        }
    }

    VF_DUMP(DUMP_MOD, graph->DumpGraph());
    VF_DUMP(DUMP_DOT_MOD, graph->DumpDotGraph());

    /** 
     * Check that execution flow terminates with
     * return or athrow bytecodes.
     * Override all incoming edges to the end-entry node.
     */
    for (vf_EdgeHandle inedge =
         graph->GetEndNode()->m_inedge; inedge; inedge = inedge->m_innext) {
        // get incoming node
        vf_NodeHandle innode = inedge->m_start;
        // get node last instruction
        vf_InstrType type = innode->m_end->m_type;

        if ((VF_INSTR_RETURN != type) && (VF_INSTR_THROW != type)) {
            // illegal instruction
            VF_REPORT(ctx, "Falling off the end of the code");
            return VF_ErrorCodeEnd;
        }
    }

    /**
     * Make data flow analysis
     */
    result = vf_check_graph_data_flow(ctx);

    return result;
}                               // vf_check_graph

/**
 * Frees memory allocated for graph, if any.
 */
void vf_free_graph(vf_Context *ctx)     // verification context
{
    if (ctx->m_graph) {
        VF_TRACE("method", VF_REPORT_CTX(ctx) << "method graph: "
                 << " nodes: " << ctx->m_graph->GetNodeCount()
                 << ", edges: " << ctx->m_graph->GetEdgeCount());
        ctx->m_graph->~vf_Graph();
        ctx->m_graph = NULL;
    }
}                               // vf_free_graph
