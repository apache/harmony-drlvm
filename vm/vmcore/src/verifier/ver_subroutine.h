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
#include "ver_graph.h"

/**
 * @ingroup Handles
 * A handle of subroutine verification context.
 */
typedef const struct vf_SubContext *vf_SubContextHandle;
/**
 * @ingroup Handles
 * A handle of subroutine verification context.
 */
typedef const struct vf_NodeStack *vf_NodeStackHandle;

/**
 * Subroutine info.
 */
struct vf_Sub
{
    /**
     * A reference to the next subroutine.
     */
    vf_Sub *m_next;
    /**
     * A node which starts with subroutine entry point.
     */
    vf_NodeHandle m_entry;
    /**
     * A node which ends with ret instruction.
     */
    vf_NodeHandle m_ret;
    /**
     * A number of different <code>jsr</code> sequences to access
     * this subroutine.
     */
    unsigned m_dupcount;
    /**
     * A number of nodes for this subroutine.
     */
    unsigned m_nodenum;
    /**
     * Subroutine nodes.
     */
    vf_NodeHandle *m_nodes;
    /**
     * Subroutine node copies. For each of <code>m_nodenum</code> nodes
     * from <code>m_nodes</code> the (m_dupcount - 1) node copies follow
     * one by one starting from this pointer.
     * <code>NULL</code> when m_dupcount == 1.
     */
    vf_Node *m_copies;
    /**
     * For each of m_dupcount subroutine copies contain a return node for
     * this copy.
     */
    vf_Node **m_following_nodes;
    /**
     * A number of edges between subroutine nodes.
     */
    unsigned m_out_edgenum;
    /**
     * A current duplication index.
     */
    unsigned m_index;
};

/**
 * A stack of subsequent nodes representing a path traversing the graph, in
 * particular a path to a <code>ret</code> node.
 */
struct vf_NodeStack
{
    /**
     * A current node.
     */
    vf_Node *m_node;
    /**
     * An <i>out</i> stack depth of the node.
     */
    unsigned m_depth;
    /**
     * When a next stack element is created, points to the next stack element.
     */
    vf_NodeStackHandle m_next;

    void Set(vf_NodeHandle node, unsigned depth)
    {
        m_node = (vf_Node *) node;
        m_depth = depth;
        m_next = NULL;
    }
};


/**
 * Aggregated subroutine data.
 */
struct vf_SubContext
{
    /**
     * Dedicated memory pool.
     */
    vf_Pool *m_pool;
    /**
     * A head of a list of subroutine descriptors.
     */
    vf_Sub *m_sub;
    /**
     * A start of a path to the current <code>ret</code>.
     */
    vf_NodeStackHandle m_path_start;
    /**
     * After the <code>ret</code> is resolved a <code>ctx->m_map</code> contains a
     * correct input map for a node which follows <code>jsr</code> call. This is
     * a pointer to the corresponding node stack. If this pointer is
     * <code>NULL</code> we start from <code>m_path_start</code> stack entry.
     */
    vf_NodeStackHandle m_path_fork;
};
