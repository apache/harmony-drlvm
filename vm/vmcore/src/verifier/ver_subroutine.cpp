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
#include "ver_subroutine.h"

#ifdef _VF_DEBUG
/**
 * Gets a subroutine number for a node. The function is slow and should be used for
 * debugging purposes only.
 * @param[in] sub  a given subroutine handle
 * @param[in] ctx  a verification context
 * @return a sequential subroutine number in a list
 */
unsigned vf_get_sub_num(vf_SubHandle sub, vf_ContextHandle ctx)
{
    unsigned index = 0;
    for (vf_SubHandle s = ctx->m_sub_ctx->m_sub; s; s = s->m_next) {
        if (s == sub) {         // subroutine found
            return index;
        }
        index++;
    }
    VF_DIE("vf_get_sub_num: Cannot find a subroutine " << sub <<
        " in a list");
    return 0;
}

void vf_Graph::DumpSub(vf_SubHandle sub)
{
    if (sub) {
        VF_DEBUG("Subroutine:");
        if (sub->m_entry) {
            VF_DEBUG("    Entry node: " << GetNodeNum(sub->m_entry));
        } else {
            VF_DEBUG("    Entry node undefined");
        }
        if (sub->m_ret) {
            VF_DEBUG("    Return node: " << GetNodeNum(sub->m_ret));
        } else {
            VF_DEBUG("    Return node undefined");
        }
    }
}                               // vf_Graph::DumpSub

static void DumpNodeStack(vf_Context *ctx)
{
    vf_NodeStackHandle p_element = ctx->m_sub_ctx->m_path_start;
    bool fork_found = false;
    unsigned count = 0;

    VF_DEBUG("Dumping path: ");
    for (; p_element; p_element = p_element->m_next, count++) {
        if (p_element == ctx->m_sub_ctx->m_path_fork) {
            fork_found = true;
            VF_DEBUG("Fork");
        }
        VF_DEBUG("  Node #" << ctx->m_graph->GetNodeNum(p_element->m_node));
    }
    assert(fork_found || !ctx->m_sub_ctx->m_path_fork);
    VF_DEBUG("Totally " << count << " nodes");
}                               // DumpNodeStack
#endif //  _VF_DEBUG


/**
 * Allocates memory for subroutine context.
 * @param ctx a verification context
 */
static void AllocateSubContext(vf_Context *ctx)
{
    vf_Pool *pool = ctx->m_pool;

    vf_SubContext *sub_ctx = (vf_SubContext *) vf_palloc(pool,
        sizeof(vf_SubContext));
    ctx->m_sub_ctx = sub_ctx;
    sub_ctx->m_pool = pool;
}                               // AllocateSubContext


/**
 * Adds a new subroutine into subroutine array.
 * @param[in] ret_node a node containing a corresponding <code>ret</code>
 * @param[in] sub_ctx  a subroutine verification context
 * @return a sub handle
 */
static vf_Sub *AddNewSub(vf_NodeHandle ret_node, vf_SubContext * sub_ctx)
{
    vf_Sub *sub = (vf_Sub *) vf_palloc(sub_ctx->m_pool, sizeof(vf_Sub));
    sub->m_next = sub_ctx->m_sub;
    sub_ctx->m_sub = sub;
    sub->m_ret = ret_node;
    return sub;
}                               // AddNewSub


/**
 * Adds a node to the subroutine.
 * @param[in] node a node which is a part of this subroutine
 */
static inline void AddSubNode(vf_Node *node)
{
    assert(node->m_mark);       // node is reachable
    vf_Sub *sub = (vf_Sub *) node->m_sub;
    sub->m_nodenum++;
    // a number of out edges for reachable node won't
    // change while removing a dead code
    sub->m_out_edgenum += node->m_outnum;
}                               // AddSubNode

static inline vf_Result ResolveSubroutineEntryPoint(vf_Node *node,
    vf_Context *ctx)
{
    //initialize vector
    vf_MapVector *map = ctx->m_map;

    // the path element node to iterate from
    vf_NodeStackHandle p_element = ctx->m_sub_ctx->m_path_fork;
    if (!p_element) {
        p_element = ctx->m_sub_ctx->m_path_start;
        map->m_number = ctx->m_maxlocal;
        unsigned index;
        for (index = 0; index < map->m_number; index++) {
            map->m_local[index].m_type = SM_ANY;
        }

        map->m_depth = p_element->m_node->m_inmap.m_depth;
        for (index = 0; index + 1 < map->m_depth; index++) {
            map->m_stack[index].m_type = SM_ANY;
        }
        map->m_stack[index].m_type = SM_RETURN_ADDR;
        map->m_stack[index].m_pc =
            (unsigned) (p_element->m_node->m_start->m_addr - ctx->m_bytes);
    }

    VF_DUMP(DUMP_NODESTACK, DumpNodeStack(ctx));
    for (; p_element; p_element = p_element->m_next) {
        vf_Result result =
            vf_set_node_out_vector(p_element->m_node, map, ctx);
        if (result != VER_OK) {
            return result;
        }
    }

    // since a copy flag is set for SM_RETURN_ADDR entries,
    // <code>ctx->m_buf</code> contains the last processed entry, i.&nbsp;e.
    // an entry for <code>ret</code>
    assert(ctx->m_buf->m_pc);
    assert(SM_RETURN_ADDR == ctx->m_buf->m_type);
    assert(node->m_outmap.m_depth == ctx->m_map->m_depth);
    ((vf_Sub *) node->m_sub)->m_entry =
        ctx->m_graph->GetNodeFromBytecodeIndex(ctx->m_buf->m_pc);
    return VER_OK;
}                               // ResolveSubroutineEntryPoint

static vf_Result MarkNode(vf_NodeStack &stack, vf_Context *ctx);

/**
 * Set a subroutine mark for the node and its predecessors. Stop recursive
 * processing if the node subroutine mark is already set.
 * @param[in] sub  the subroutine handle
 * @param[in] node a node which is a part of a subroutine
 * @param[in,out] ctx a verification context
 * @return if the mark of this node is already set and is not equal to 
 * <code>sub</code>, <code>VER_ErrorJsrMultipleRet</code> is reported
 */
static inline vf_Result
SetSubMarks(vf_SubHandle sub, vf_Node *node, vf_Context *ctx)
{
    assert(sub);
    VF_TRACE("sub.mark", "sub[" << ctx->m_graph->GetNodeNum(node)
        << "] := sub[" << ctx->m_graph->GetNodeNum(sub->m_ret)
        << "] (" << vf_get_sub_num(sub, ctx) << ")");
    if (node->m_sub == sub) {
        return VER_OK;          // already marked
    }

    if (NULL == node->m_sub) {
        if (node == ctx->m_graph->GetStartNode()) {
            VF_REPORT(ctx, "Reached ret not using jsr branches");
            return VER_ErrorJsrOther;
        }

        node->m_sub = sub;
        if (node->m_mark) {
            // add marked nodes now, others later
            AddSubNode(node);
        }
        for (vf_EdgeHandle inedge = node->m_inedge;
            inedge; inedge = inedge->m_innext) {
            if (vf_is_jsr_branch(inedge, ctx)) {
                continue;
            }
            vf_Result result =
                SetSubMarks(sub, (vf_Node *) inedge->m_start, ctx);
            if (result != VER_OK) {
                return result;
            }
        }
        return VER_OK;
    }

    assert(node->m_sub != sub);
    VF_REPORT(ctx,
        "A subroutine splits execution into several ret instructions");
    return VER_ErrorJsrMultipleRet;
}                               // SetSubMarks

/**
 * Marks a <code>jsr</code> edge end node. If the subroutine returns, marks a node, which
 * follows the edge start node.
 */
static inline vf_Result
FollowJsrEdge(vf_Node *node, vf_NodeStack &stack, vf_Context *ctx)
{
    vf_NodeStackHandle path_start = ctx->m_sub_ctx->m_path_start;
    if (!path_start) {
        ctx->m_sub_ctx->m_path_start = &stack;
    }

    vf_Result r = MarkNode(stack, ctx);
    if (VER_OK != r) {
        return r;
    }

    if (stack.m_node->m_sub->m_entry == stack.m_node) {
        // this jsr returns, and we need to process a node which
        // follows after edge start node
        ctx->m_sub_ctx->m_path_fork = &stack;
        // select and check the following node
        stack.Set(node + 1, stack.m_node->m_sub->m_ret->m_outmap.m_depth);
        if (VF_NODE_END_ENTRY == stack.m_node->m_type) {
            VF_REPORT(ctx, "Falling off the end of the code");
            return VER_ErrorBranch;
        }
        assert(VF_NODE_CODE_RANGE == stack.m_node->m_type);
        r = MarkNode(stack, ctx);
        if (VER_OK != r) {
            return r;
        }
        ctx->m_sub_ctx->m_path_fork = NULL;
    }

    if (stack.m_node->m_sub) {
        r = SetSubMarks(stack.m_node->m_sub, node, ctx);
        if (VER_OK != r) {
            return r;
        }
    }

    ctx->m_sub_ctx->m_path_start = path_start;
    return r;
}                               // FollowJsrEdge

/**
 * Marks a given node and follows <i>out</i> edges.
 * @param[in] stack a reference to the stack element for the given node
 * @param[in] ctx   a verification context
 * @return VER_OK if marking was successful, an error code otherwise
 */
static vf_Result MarkNode(vf_NodeStack &stack, vf_Context *ctx)
{
    vf_Graph *graph = ctx->m_graph;
    vf_Result r;
    vf_Result result =
        vf_check_node_stack_depth(stack.m_node, stack.m_depth, ctx);
    if (result != VER_Continue) {
        return result;
    }
    VF_TRACE("sub.mark",
        "Processing node " << graph->GetNodeNum(stack.m_node));
    if (stack.m_node->m_sub) {
        AddSubNode(stack.m_node);
    }

    if ((VF_NODE_CODE_RANGE == stack.m_node->m_type)
        && (VF_INSTR_RET == stack.m_node->m_end->m_type)) {
        vf_Sub *sub = AddNewSub(stack.m_node, ctx->m_sub_ctx);
        r = SetSubMarks(sub, stack.m_node, ctx);
        if (VER_OK != r) {
            return r;
        }
        r = ResolveSubroutineEntryPoint(stack.m_node, ctx);
        if (VER_OK != r) {
            return r;
        }
    }

    vf_NodeStack next_stack;
    stack.m_next = &next_stack;
    for (vf_EdgeHandle outedge = stack.m_node->m_outedge;
        outedge; outedge = outedge->m_outnext) {
        next_stack.Set(outedge->m_end, stack.m_depth);
        if (!vf_is_jsr_branch(outedge, ctx)) {
            r = MarkNode(next_stack, ctx);
        } else {
            assert(VF_NODE_CODE_RANGE == outedge->m_end->m_type);
            assert(!outedge->m_outnext);
            r = FollowJsrEdge(stack.m_node, next_stack, ctx);
        }
        if (VER_OK != r) {
            return r;
        }
    }
    return VER_OK;
}                               // MarkNode


vf_Result vf_mark_subroutines(vf_Context *ctx)
{
    vf_Graph *graph = ctx->m_graph;
    AllocateSubContext(ctx);
    vf_NodeStack bottom = {
        (vf_Node *) ctx->m_graph->GetStartNode(), 0, NULL
    };
    vf_Result r = MarkNode(bottom, ctx);
    return r;
}                               // vf_mark_subroutines

static vf_Result AddDupCount(vf_Sub *sub, unsigned &count, vf_Context *ctx)
{
    VF_TRACE("dupcount",
        "Calculating a duplication count for a subroutine #" <<
        vf_get_sub_num(sub, ctx));
    if (ALL_BITS_SET == sub->m_dupcount) {
        VF_REPORT(ctx, "Found a recursive subroutine call sequence");
        return VER_ErrorJsrRecursive;
    }

    if (!sub->m_dupcount) {
        sub->m_dupcount = ALL_BITS_SET;
        unsigned sum = 0;
        for (vf_EdgeHandle inedge = sub->m_entry->m_inedge;
            inedge; inedge = inedge->m_innext) {
            vf_NodeHandle node = inedge->m_start;
            if (vf_is_jsr_branch(inedge, ctx)) {
                assert(!inedge->m_outnext);
                if (node->m_sub == NULL) {
                    sum++;
                } else if (node->m_sub != sub) {
                    AddDupCount((vf_Sub *) node->m_sub, sum, ctx);
                }               // else this call served as unconditional internal branch
            } else if (node->m_sub != sub) {
                VF_REPORT(ctx,
                    "A subroutine splits execution into "
                    "several ret instructions");
                return VER_ErrorJsrMultipleRet;
            }
        }
        sub->m_dupcount = sum;
    }

    VF_TRACE("dupcount",
        "A duplication count for a subroutine #" << vf_get_sub_num(sub,
            ctx) << " is " << sub->m_dupcount);
    count += sub->m_dupcount;
    return VER_OK;
}                               // AddSubDupCount

static void InlineSubNodes(vf_ContextHandle ctx)
{
    vf_Graph *graph = ctx->m_graph;
    vf_Sub *sub;
    for (sub = ctx->m_sub_ctx->m_sub; sub; sub = sub->m_next) {
        sub->m_nodes =
            (vf_NodeHandle *) vf_palloc(ctx->m_sub_ctx->m_pool,
            sub->m_nodenum * sizeof(vf_NodeHandle));
    }

    // populate subroutine node get the next node after the start node
    vf_Node *node = (vf_Node *) graph->GetStartNode() + 1;
    for (; VF_NODE_END_ENTRY != node->m_type; node++) {
        if (node->m_sub) {
            vf_Sub *sub = (vf_Sub *) node->m_sub;
            node->m_nodecount = sub->m_index;
            sub->m_nodes[sub->m_index++] = node;
        }
    }

    for (sub = ctx->m_sub_ctx->m_sub; sub; sub = sub->m_next) {
        // clear sub indices
        assert(sub->m_nodenum == sub->m_index);
        sub->m_index = 0;
        // duplicate subroutine nodes
        assert(sub->m_dupcount);
        if (sub->m_dupcount == 1) {
            continue;
        }
        vf_NodeHandle *p_node = sub->m_nodes;
        vf_Node *new_node = sub->m_copies =
            graph->AddNodes((sub->m_dupcount - 1) * sub->m_nodenum);
        for (unsigned node_index = 0; node_index < sub->m_nodenum;
            node_index++, p_node++) {
            for (unsigned index = 1; index < sub->m_dupcount;
                index++, new_node++) {
                *new_node = **p_node;
                // edges are outdated, delete them
                new_node->m_inedge = new_node->m_outedge = NULL;
                new_node->m_outnum = 0;
                VF_TRACE("sub.inline",
                    "Copied node " << graph->GetNodeNum(*p_node)
                    << " to " << graph->GetNodeNum(new_node));
            }
        }
    }
    assert(0 == ctx->m_graph->HasMoreNodeSpace());
}                               // InlineSubNodes

/**
 * Redirects subroutine calls to separate subroutine copies.
 */
static void InlineInEdges(vf_ContextHandle ctx)
{
    vf_Graph *graph = ctx->m_graph;
    for (vf_SubHandle sub = ctx->m_sub_ctx->m_sub; sub; sub = sub->m_next) {
        ((vf_Sub *) sub)->m_following_nodes =
            (vf_Node **) vf_palloc(ctx->m_sub_ctx->m_pool,
            sub->m_dupcount * sizeof(vf_Node *));
        vf_NodeHandle sub_entry_copy =
            sub->m_copies + sub->m_entry->m_nodecount * (sub->m_dupcount -
            1) - 1;
        vf_EdgeHandle *p_next_edge =
            (vf_EdgeHandle *) & sub->m_entry->m_inedge;
        vf_Edge *inedge = (vf_Edge *) sub->m_entry->m_inedge;
        while (inedge) {
            if (vf_is_jsr_branch(inedge, ctx)) {
                if (sub->m_index) {
                    assert(sub->m_copies);
                    assert(sub->m_dupcount);
                    // remove the edge from the list
                    *p_next_edge = inedge->m_innext;
                    // redirect jsr edge to correspondent subroutine copy
                    inedge->m_end = sub_entry_copy + sub->m_index;
                }
                // store return point
                vf_Node *next_node = (vf_Node *) inedge->m_start + 1;
                assert(VF_NODE_CODE_RANGE == next_node->m_type);
                sub->m_following_nodes[((vf_Sub *) sub)->m_index++] =
                    next_node;
            }
            p_next_edge = &inedge->m_innext;
            inedge = (vf_Edge *) inedge->m_innext;
        }
        assert(sub->m_index);   // there was at least one branch
    }
}                               // InlineInEdges

/**
 * Subroutine calls are redirected to correspondent subroutine copies,
 * other edges are just duplicated.
 */
static void InlineOutEdges(vf_ContextHandle ctx)
{
    vf_Graph *graph = ctx->m_graph;
    for (vf_SubHandle sub = ctx->m_sub_ctx->m_sub; sub; sub = sub->m_next) {
        if (!sub->m_copies) {
            continue;
        }
        vf_Node *copy = sub->m_copies;
        for (unsigned node_index = 0; node_index < sub->m_nodenum;
            node_index++, copy += sub->m_dupcount - 1) {
            vf_NodeHandle node = sub->m_nodes[node_index];
            for (vf_EdgeHandle outedge = node->m_outedge; outedge;
                outedge = outedge->m_outnext) {
                vf_NodeHandle end_node = outedge->m_end;
                vf_SubHandle end_sub = end_node->m_sub;
                vf_Node *c = copy;
                if (NULL == end_sub) {
                    for (unsigned index = 1; index < sub->m_dupcount; index++) {
                        graph->NewEdge(c++, (vf_Node *) end_node);
                    }
                } else if (node->m_sub == end_sub) {
                    vf_Node *e = sub->m_copies +
                        end_node->m_nodecount * (sub->m_dupcount - 1);
                    for (unsigned index = 1; index < sub->m_dupcount; index++) {
                        graph->NewEdge(c++, e++);
                    }
                } else {
                    assert(vf_is_jsr_branch(outedge, ctx));
                    assert(!outedge->m_outnext);
                    // an end node
                    vf_Node *e = end_sub->m_copies +
                        end_sub->m_entry->m_nodecount *
                        (end_sub->m_dupcount - 1) - 1;
                    // a node next to the start node
                    vf_Node *n = c + (sub->m_dupcount - 1);
                    for (unsigned index = 1;
                        index < node->m_sub->m_dupcount; index++) {
                        // direct jsr edge to correspondent subroutine copy
                        graph->NewEdge(c++, e + end_sub->m_index);
                        // store return point
                        assert(VF_NODE_CODE_RANGE == n->m_type);
                        end_sub->m_following_nodes[((vf_Sub *) end_sub)->
                            m_index++] = n++;
                    }
                }
            }
        }
    }
}                               // InlineOutEdges

/**
 * Adds a back branch from <code>ret</code> nodes to a node
 * which follows the correspondent <code>jsr</code> call node.
 */
static void InlineRetEdges(vf_ContextHandle ctx)
{
    vf_Graph *graph = ctx->m_graph;
    for (vf_SubHandle sub = ctx->m_sub_ctx->m_sub; sub; sub = sub->m_next) {

        vf_Node *ret_node = (vf_Node *) sub->m_ret;
        vf_Node **next_node = sub->m_following_nodes;
        graph->NewEdge(ret_node, *(next_node++));
        vf_Node *ret_node_copy =
            sub->m_copies + ret_node->m_nodecount * (sub->m_dupcount - 1);
        for (unsigned index = 1; index < sub->m_dupcount; index++) {
            // direct ret entry to the return point
            graph->NewEdge(ret_node_copy++, *(next_node++));
        }
    }
    assert(0 == ctx->m_graph->HasMoreEdgeSpace());
}                               // InlineRetEdges

/**
 * Checks that every subroutine copy gets its call and
 * all edges are created.
 */
static void CheckDupCounts(vf_ContextHandle ctx)
{
#if _VF_DEBUG
    for (vf_SubHandle sub = ctx->m_sub_ctx->m_sub; sub; sub = sub->m_next) {
        assert(sub->m_dupcount == sub->m_index);
    }
#endif // _VF_DEBUG
}

/**
 * Calculates a number of additional nodes and edges and adds them
 * to the graph.
 */
vf_Result vf_inline_subroutines(vf_Context *ctx)
{
    if (NULL == ctx->m_sub_ctx->m_sub) {
        // subroutines are not reachable
        return VER_OK;
    }

    unsigned count;
    unsigned nodes = 0, edges = 0;
    for (vf_Sub *sub = ctx->m_sub_ctx->m_sub; sub; sub = sub->m_next) {
        vf_Result r = AddDupCount(sub, count, ctx);
        if (VER_OK != r) {
            return r;
        }
        // one copy already present in the graph
        nodes += (sub->m_dupcount - 1) * sub->m_nodenum;
        edges += (sub->m_dupcount - 1) * sub->m_out_edgenum;
        // there is an additional edge from every ret
        edges += sub->m_dupcount;
    }
    ctx->m_graph->CreateEdges(edges);
    if (nodes) {
        ctx->m_graph->CreateNodes(nodes);
        InlineSubNodes(ctx);
    }
    InlineInEdges(ctx);
    if (nodes) {
        InlineOutEdges(ctx);
    }

    CheckDupCounts(ctx);
    InlineRetEdges(ctx);
    return VER_OK;
}                               // vf_inline_subroutines
