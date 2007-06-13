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


#ifndef _VF_GRAPH_H_
#define _VF_GRAPH_H_
/**
 * @file
 * Control flow graph structures.
 */

#include "ver_real.h"

/**
 * Each node has a descriptive bitmap.
 */
enum vf_NodeType
{
    VF_NODE_CODE_RANGE = 0,
    VF_NODE_START_ENTRY = 1,
    VF_NODE_END_ENTRY = 2,
    VF_NODE_HANDLER = 3
};

const unsigned ALL_BITS_SET = ~0U;

/**
 * @ingroup Handles
 * A handle of a stack map vector.
 */
typedef const struct vf_MapVector *vf_MapVectorHandle;
/**
 * @ingroup Handles
 * A handle of a graph edge.
 */
typedef const struct vf_Edge *vf_EdgeHandle;
/**
 * @ingroup Handles
 * A handle of a container.
 */
typedef const struct vf_Container *vf_ContainerHandle;
/**
 * @ingroup Handles
 * A subroutine handle.
 */
typedef const struct vf_Sub *vf_SubHandle;

/**
 * Structure of stack map vector.
 */
struct vf_MapVector
{
    vf_MapEntry *m_stack;       ///< stack map vector
    vf_MapEntry *m_local;       ///< locals map vector
    unsigned short m_number;    ///< number of locals
    unsigned short m_depth;     ///< stack depth
};

/**
 * Node marks are greater or equal to this, zero mark means
 * unmarked node.
 */
const unsigned VF_START_MARK = 1;

/**
 * Graph node structure.
 */
struct vf_Node
{
    vf_MapVector m_inmap;       ///< stack map IN node vector
    vf_MapVector m_outmap;      ///< stack map OUT node vector
    vf_InstrHandle m_start;     ///< the first instruction
    vf_InstrHandle m_end;       ///< the last instruction
    vf_EdgeHandle m_inedge;     ///< the first incoming edge
    vf_EdgeHandle m_outedge;    ///< the first outcoming edge
    unsigned m_outnum;          ///< a number of outcoming edges
    unsigned m_nodecount;       ///< node count in enumeration
    ///< 0 for unreachable and start nodes
    int m_stack;                ///< stack depth modifier
    int m_mark;                 ///< node mark
    vf_SubHandle m_sub;         ///< if this is a subroutine node, this refers
    ///< to the corresponding subroutine descriptor
    vf_NodeType m_type;         ///< node type
    bool m_initialized:1;       ///< reference in a local variable 
    ///< should be initialized
};

/**
 * Graph edge structure.
 */
struct vf_Edge
{
    vf_NodeHandle m_start;      ///< start edge node
    vf_NodeHandle m_end;        ///< end edge node
    vf_EdgeHandle m_innext;     ///< next incoming edge
    vf_EdgeHandle m_outnext;    ///< next outcoming edge
};

/**
 * A generic container.
 */
struct vf_Container
{
    vf_ContainerHandle m_next;  ///< next container
    unsigned m_max;             ///< max number of nodes in container
    unsigned m_used;            ///< number of nodes in container
};

/**
 * Locates a container for an object by its number.
 * @param[in] container a container to start a search from
 * @param[in]  n an object number
 * @param[out] n an object index in a returned container
 * @return a container
 */
static inline vf_ContainerHandle vf_get_container(vf_ContainerHandle
                                                  container, unsigned &n)
{
    while (n >= container->m_max) {
        n -= container->m_max;
        container = container->m_next;
        assert(container);
        assert(container->m_max);
    }
    return container;
}

/**
 * Adds a new container to the container list.
 * @param[in, out] head the first list element
 * @param[in]  new_container a new container to be placed at the end of the list
 */
static inline void vf_add_container(vf_ContainerHandle head,
                                    vf_ContainerHandle new_container)
{
    while (head->m_next) {
        head = head->m_next;
    }
    ((vf_Container *) head)->m_next = new_container;
}


/**
 * Checks amount of used space for given containers.
 */
static inline unsigned vf_used_space(vf_ContainerHandle container)
{
    unsigned used = 0;

    for (; container; container = container->m_next) {
        used += container->m_used;
    }
    return used;
}                               // vf_used_space

/**
 * Checks amount of pre-allocated space for given containers.
 */
static inline unsigned vf_total_space(vf_ContainerHandle container)
{
    unsigned total_space = 0;

    for (; container; container = container->m_next) {
        total_space += container->m_max;
    }
    return total_space;
}                               // vf_total_space



/**
 * A linked list of containers containing arrays of nodes.
 */
struct vf_NodeContainer
{
    vf_Container container;     ///< a container
    vf_Node m_node[1];          ///< an array of nodes
};

/**
 * A linked list of containers containing arrays of edges.
 */
struct vf_EdgeContainer
{
    vf_Container container;     ///< a container
    vf_Edge m_edge[1];          ///< an array of edges
};

/**
 * An iterator over nodes.
 */
class vf_Iterator
{
    vf_ContainerHandle m_container;     ///< a container
    unsigned m_index;           ///< an index in this container

    /**
     * Go to the next container.
     */
    void SkipContainer()
    {
        m_container = m_container->m_next;
        m_index = 0;
    }

    /**
     * Advances iterator to the next object.
     */
    void Advance()
    {
        m_index++;
        if (m_index < m_container->m_max) {
            return;
        }
        SkipContainer();
    }
  public:
    /**
     * Empty constructor.
     */
    vf_Iterator ()
    {
    }

    /**
     * Creates an iterator for given containers.
     * @param[in] container the first container
     */
    vf_Iterator (vf_ContainerHandle container)
    {
        Reset(container);
    }

    /**
     * Resets an iterator for given containers.
     * @param[in] container the first container
     */
    void Reset(vf_ContainerHandle container)
    {
        m_container = container;
        m_index = 0;
    }

    /**
     * Gets a current node.
     */
    vf_NodeHandle GetNextNode()
    {
        vf_NodeHandle node =
            ((vf_NodeContainer *) m_container)->m_node + m_index;
        Advance();
        return node;
    }

    /**
     * @return <code>true</code> if the iterator can be advanced further.
     */
    bool HasMoreElements() const
    {
        return m_container;
    }
};                              // vf_Iterator

/**
 * Verifier control flow graph structure.
 */
class vf_Graph
{
    vf_NodeContainer *m_nodes;  ///< array of nodes
    vf_EdgeContainer *m_edges;  ///< array of edges
    vf_Pool *m_pool;            ///< graph memory pool
    vf_NodeHandle *m_enum;      ///< graph node enumeration
    unsigned m_nodenum;         ///< number of nodes
    unsigned m_edgenum;         ///< number of edges
    unsigned m_enumcount;       ///< number of enumerated elements
    vf_NodeHandle m_endnode;    ///< control flow end
    vf_ContextHandle m_ctx;     ///< a verification context
    vf_Iterator iterator;       ///< an embedded graph iterator
    bool m_free;                ///< need to free pool

    /**
     * Prints a graph node in <code>stderr</code>.
     * @param node a graph node
     */
    void DumpNode(vf_NodeHandle node);

    /**
     * Prints graph node instructions in <code>stderr</code>.
     * @param node_num   a graph node
     * @note Empty in release mode.
     */
    void DumpNodeInternal(vf_NodeHandle node);

#ifdef _VF_DEBUG
    /**
     * Prints subroutine information in <code>stderr</code>.
     * @param sub a subroutine handle
     */
    void DumpSub(vf_SubHandle sub);

    /**
     * Checks if node maps are allocated.
     * @param[in] vector to check
     */
    bool AreMapsAllocated(vf_MapVectorHandle vector) const
    {
        return (!m_ctx->m_maxlocal || vector->m_local)
            && (!m_ctx->m_maxstack || vector->m_stack);
    }
#endif                          // _VF_DEBUG

    /**
     * Dumps a graph header in a file in a DOT format.
     * @param graph_name    a graph name
     * @param fout          a file stream
     * @note Empty in release mode.
     */
    void DumpDotHeader(const char *graph_name, ofstream &fout);

    /**
     * Dumps a graph node in a file in DOT format.
     * @param node     a graph node
     * @param fout     a file stream
     * @note Empty in release mode.
     */
    void DumpDotNode(vf_NodeHandle node, ofstream &fout);

    /**
     * Dumps graph node instructions in a file stream in DOT format.
     * @param node       a graph node
     * @param next_node  a separator between nodes in stream
     * @param next_instr a separator between instructions in stream
     * @param fout       an output file stream
     * @note Empty in release mode.
     */
    void DumpDotNodeInternal(vf_NodeHandle node,
                             char *next_node, char *next_instr,
                             ofstream &fout);

    /**
     * Dumps graph end in file in DOT format.
     * @param fout - output file stream
     * @note Empty in release mode.
     */
    void DumpDotEnd(ofstream &fout);

  public:
    /**
     * Constructs a control flow graph and allocates memory for
     * graph structure, nodes and edges.
     * @param nodenum a number of graph nodes
     * @param edgenum a number of graph edges
     * @param ctx     a verification context
     */
    vf_Graph(unsigned nodenum, unsigned edgenum,
             vf_ContextHandle ctx):m_nodes(NULL), m_edges(NULL),
        m_pool(ctx->m_pool), m_enum(NULL), m_nodenum(0), m_edgenum(0),
        m_enumcount(0), m_ctx(ctx), m_free(false)
    {
        CreateNodes(nodenum);
        CreateEdges(edgenum);
    }                           // vf_Graph

    /**
     * Control flow graph destructor.
     * @note Function release memory for graph structure, nodes and edges.
     */
    ~vf_Graph() {
        if (m_free) {
            vf_delete_pool(m_pool);
        }
    }                           // ~vf_Graph

    /**
     * Allocates memory for graph nodes.
     * @param count a number of nodes
     */
    void CreateNodes(unsigned count)
    {
        assert(count > 0);
        vf_NodeContainer *nodes =
            (vf_NodeContainer *) AllocMemory(sizeof(vf_NodeContainer)
                                             + (count - 1) * sizeof(vf_Node));

        nodes->container.m_max = count;
        if (m_nodes == NULL) {
            m_nodes = nodes;
        } else {
            vf_add_container(&m_nodes->container, &nodes->container);
        }
    }

    /**
     * Allocates a container for graph edges.
     * @param count a number of edges 
     */
    void CreateEdges(unsigned count)
    {
        assert(count > 0);
        vf_EdgeContainer *edges =
            (vf_EdgeContainer *) AllocMemory(sizeof(vf_EdgeContainer)
                                             + (count - 1) * sizeof(vf_Edge));
        edges->container.m_max = count;
        if (m_edges == NULL) {
            m_edges = edges;
        } else {
            vf_add_container(&m_edges->container, &edges->container);
        }
    }

    /**
     * Gets a graph node.
     * @param[in] node_num  a node number, should be in range
     * @return a handle of the node
     */
    vf_NodeHandle GetNode(unsigned node_num) const
    {
        // get node
        assert(m_nodes);
        assert(node_num < m_nodenum);

        vf_NodeContainer *nodes =
            (vf_NodeContainer *) vf_get_container(&m_nodes->container,
                                                  node_num);
        return nodes->m_node + node_num;
    }                           // GetNode

    /**
     * Gets a graph node from a program counter.
     *
     * @param[in] pc   a bytecode index at a node start
     * @return a handle of a node
     */
    vf_NodeHandle GetNodeFromBytecodeIndex(unsigned pc) const
    {
        vf_InstrHandle instr = m_ctx->m_bc[pc].m_instr;
        assert(instr);
        assert(m_ctx->m_bc[pc].m_instr->m_addr - m_ctx->m_bytes == pc);
        vf_NodeHandle node = instr->m_node;
        assert(GetNodeNum(node) > m_ctx->m_handlers);
        return node;
    }

    /**
     * Gets the start node.
     * @return the handle of the start node
     */
    vf_NodeHandle GetStartNode()
    {
        return m_nodes->m_node;
    }                           // GetStartNode

    /**
     * Adds the end node.
     * @return the handle of the end node
     */
    vf_NodeHandle AddEndNode()
    {
        m_endnode = NewNode(VF_NODE_END_ENTRY);
        return m_endnode;
    }                           // GetEndNode

    /**
     * Gets the end node.
     * @return the handle of the end node
     */
    vf_NodeHandle GetEndNode()
    {
        return m_endnode;
    }                           // GetEndNode

    /**
     * Creates a new node of a specific type.
     * A node array must have enough free space for a new element.
     * @param[in] type node type
     * @return a handle of the created node
     */
    vf_NodeHandle NewNode(vf_NodeType type)
    {
        // get node
        assert(m_nodes);
        unsigned count = m_nodenum;
        vf_NodeContainer *nodes =
            (vf_NodeContainer *) vf_get_container(&m_nodes->container, count);

        // increment nodes count
        m_nodenum++;
        nodes->container.m_used++;
        assert(nodes->container.m_used <= nodes->container.m_max);

        // set node
        vf_Node *node = nodes->m_node + count;
        node->m_type = type;
        return node;
    }                           // NewNode


    /**
     * Creates a new node for a bytecode range.
     * A node array must have enough free space for a new element.
     * @param[in] start   the first instruction
     * @param[in] end     the last instruction
     * @return a handle of the created node
     */
    vf_NodeHandle NewNode(vf_InstrHandle start, vf_InstrHandle end)
    {
        // get node
        vf_Node *node = (vf_Node *) NewNode(VF_NODE_CODE_RANGE);
        node->m_start = start;
        node->m_end = end;
        return node;
    }                           // NewNode(start, end)

    /**
     * A quick interface for adding nodes to the container. Reserves a
     * given number of nodes by increasing <code>m_used</code> count
     * of the current container.
     * A node array must have enough free space for a new element.
     * @param[in] count a handle of an existing node
     * @return a reference to the first reserved node
     */
    vf_Node *AddNodes(unsigned count)
    {
        // get node
        assert(m_nodes);

        // find a container
        unsigned c = m_nodenum;
        vf_NodeContainer *nodes =
            (vf_NodeContainer *) vf_get_container(&m_nodes->container, c);

        // increment nodes count
        m_nodenum += count;
        nodes->container.m_used += count;
        assert(nodes->container.m_used <= nodes->container.m_max);

        // return a reference
        return nodes->m_node + c;
    }                           // AddNodes

    /**
     * Creates a new edge between graph nodes.
     *
     * @param start_node    an edge start
     * @param end_node      an edge end
     * @return a handle of a created edge
     */
    vf_EdgeHandle NewEdge(vf_Node *start_node, vf_Node *end_node)
    {
        // get edge
        assert(m_edges);

        VF_TRACE("graph",
                 "Creating a new edge from " << GetNodeNum(start_node)
                 << " to " << GetNodeNum(end_node));
        unsigned count = m_edgenum;
        vf_EdgeContainer *edges =
            (vf_EdgeContainer *) vf_get_container(&m_edges->container, count);

        // get a new edge and edge's nodes
        vf_Edge *edge = edges->m_edge + count;

        // set a new edge
        edge->m_start = start_node;
        edge->m_end = end_node;
        edge->m_outnext = start_node->m_outedge;
        start_node->m_outedge = edge;
        start_node->m_outnum++;
        edge->m_innext = end_node->m_inedge;
        end_node->m_inedge = edge;

        // increment edge count
        m_edgenum++;
        edges->container.m_used++;
        assert(edges->container.m_used <= edges->container.m_max);
        return edge;
    }                           // NewEdge

    /**
     * Resets an internal graph node iterator.
     */
    void ResetNodeIterator()
    {
        iterator.Reset(&m_nodes->container);
    }

    /**
     * Gets a next node.
     */
    vf_NodeHandle GetNextNode()
    {
        return iterator.GetNextNode();
    }

    /**
     * Checks if there are still elements to iterate.
     */
    bool HasMoreElements() const
    {
        return iterator.HasMoreElements();
    }

    /**
     * Counts graph nodes.
     * @return a number of graph nodes
     */
    unsigned GetNodeCount()
    {
        return m_nodenum;
    }                           // GetNodeCount

    /**
     * Counts graph edges excluding
     * the reserved edge.
     * @return a number of graph edges
     */
    unsigned GetEdgeCount() const
    {
        return m_edgenum;
    }                           // GetEdgeCount

    /**
     * Removes marks for all nodes.
     */
    void CleanNodeMarks()
    {
        assert(m_nodes);
        ResetNodeIterator();

        // clean node marks
        while (HasMoreElements()) {
            ((vf_Node *) GetNextNode())->m_mark = 0;
        }
    }                           // CleanNodeMarks

    /**
     * Allocates memory from the graph memory pool.
     * @param size a size of memory block
     * @return a pointer to allocated memory,
     *         or <code>NULL</code> if memory allocation failed.
     */
    void *AllocMemory(unsigned size)
    {
        assert(size);
        return vf_palloc(m_pool, size);
    }                           // AllocMemory

    /**
     * Allocates a graph node enumeration sequence and
     * sets first enumeration element.
     * @param node a first enumeration node
     */
    void SetStartCountNode(vf_Node *node)
    {
        // check node number is in range
        assert(m_nodes);
        assert(node);

        // allocate memory
        m_enum = (vf_NodeHandle *) vf_palloc(m_pool, sizeof(vf_NodeHandle)
                                             * m_nodenum);

        // clean node enumeration
        ResetNodeIterator();
        while (HasMoreElements()) {
            ((vf_Node *) GetNextNode())->m_nodecount = ALL_BITS_SET;
        }

        // set node enumeration number
        node->m_nodecount = 0;

        // set enumeration first element;
        m_enum[0] = node;
        m_enumcount = 1;
    }

    /**
     * Receives number of enumerated nodes.
     * @return Function returns number of enumerated nodes.
     */
    unsigned GetReachableNodeCount()
    {
        return m_enumcount;
    }                           // SetStartCountNode

    /**
     * Sets a next enumeration element in a graph enumeration
     * and stores the index in the node.
     * @param node  a node handle
     */
    void AddReachableNode(vf_Node *node)
    {
        // check enumeration count is in range
        assert(m_enumcount < m_nodenum);

        // set enumeration element for node
        m_enum[m_enumcount] = node;

        // set node enumeration number and increase number of enumerated nodes
        node->m_nodecount = m_enumcount++;
    }                           // SetNextCountNode

    /**
     * Suits for counting reachable nodes.
     * @param node  a node handle
     */
    void IncrementReachableCount()
    {
        m_enumcount++;
    }                           // IncreaseReachableCount

    /**
     * Gets a graph node by enumeration index.
     * @param count given enumeration index
     * @return      a graph node handle
     */
    vf_NodeHandle GetReachableNode(unsigned count)
    {
        return m_enum[count];
    }                           // GetCountElementNode

    /**
     * Copies a data flow vector from a given source.
     * @param source   a handle of a source vector
     * @param dest     a pointer to a vector to set
     */
    void CopyVector(vf_MapVectorHandle source, vf_MapVector *dest)
    {
        assert(AreMapsAllocated(source));
        assert(AreMapsAllocated(dest));

        dest->m_number = source->m_number;
        dest->m_depth = source->m_depth;

        unsigned index;
        for (index = 0; index < source->m_number; index++) {
            dest->m_local[index] = source->m_local[index];
        }
        for (index = 0; index < source->m_depth; index++) {
            dest->m_stack[index] = source->m_stack[index];
        }
    }

    /**
     * Copies a data flow vector from a given source,
     * nullifies the rest of the destination.
     * @param source   a handle of a source vector
     * @param dest     a pointer to a vector to set
     */
    void CopyFullVector(vf_MapVectorHandle source, vf_MapVector *dest)
    {
        assert(AreMapsAllocated(source));
        assert(AreMapsAllocated(dest));

        unsigned index;
        vf_MapEntry zero = { 0 };

        dest->m_number = source->m_number;
        dest->m_depth = source->m_depth;

        // copy locals
        for (index = 0; index < source->m_number; index++) {
            dest->m_local[index] = source->m_local[index];
        }
        for (; index < m_ctx->m_maxlocal; index++) {
            dest->m_local[index] = zero;
        }
        // copy stack
        for (index = 0; index < source->m_depth; index++) {
            dest->m_stack[index] = source->m_stack[index];
        }
        for (; index < m_ctx->m_maxstack; index++) {
            dest->m_stack[index] = zero;
        }
    }

    /**
     * Allocates a vector with maximum storage capacity.
     * @param[out] vector map to allocate
     */
    void AllocVector(vf_MapVector *vector)
    {
        // create and set local vector
        if (m_ctx->m_maxlocal && !vector->m_local) {
            vector->m_local =
                (vf_MapEntry *) vf_palloc(m_pool,
                                          m_ctx->m_maxlocal *
                                          sizeof(vf_MapEntry));
        }
        // create and set stack vector
        if (m_ctx->m_maxstack && !vector->m_stack) {
            vector->m_stack =
                (vf_MapEntry *) vf_palloc(m_pool,
                                          m_ctx->m_maxstack *
                                          sizeof(vf_MapEntry));
        }
    }

    /**
     * Prints a graph structure in <code>stderr</code>.
     * @note Empty in release mode.
     */
    void DumpGraph();

    /**
     * Dumps a graph in a file in DOT format.
     * @note Empty in release mode.
     * @note File name is created from class and method names with .dot extension.
     */
    void DumpDotGraph();

#if _VF_DEBUG
    /**
     * Gets a graph node number for debugging purposes.
     *
     * @param[in] node a node handle
     * @return a subsequent node number
     */
    unsigned GetNodeNum(vf_NodeHandle node) const
    {
        assert(node);
        unsigned node_num = 0;
        vf_NodeContainer *nodes = m_nodes;
        while (nodes) {
            unsigned index = node - nodes->m_node;
            if (index < nodes->container.m_max) {
                node_num += index;
                assert(node_num < m_nodenum);
                return node_num;
            }
            node_num += nodes->container.m_max;
            nodes = (vf_NodeContainer *) nodes->container.m_next;
        }
        VF_DIE("vf_Graph::GetNodeNum: cannot find a node " << node);
        return 0;
    }

    /**
     * Checks amount of pre-allocated space for nodes in the graph.
     */
    unsigned HasMoreNodeSpace() const
    {
        unsigned used = vf_used_space(&m_nodes->container);
        assert(used == m_nodenum);
        return vf_total_space(&m_nodes->container) - used;
    }                           // HasMoreNodeSpace

    /**
     * Checks amount of pre-allocated space for edges in the graph.
     */
    unsigned HasMoreEdgeSpace() const
    {
        unsigned used = vf_used_space(&m_edges->container);
        assert(used == m_edgenum);
        return vf_total_space(&m_edges->container) - used;
    }                           // HasMoreEdgeSpace

#endif // _VF_DEBUG

};                              // vf_Graph

/**
 * Checks if a given edge is a subroutine call branch.
 * @param[in] edge    an edge handle
 * @param[in] ctx     a verification context
 * @return <code>true</code> if this branch is a jsr call branch
 */
static inline bool vf_is_jsr_branch(vf_EdgeHandle edge, vf_ContextHandle ctx)
{
    vf_NodeHandle node = edge->m_start;
    return (VF_NODE_CODE_RANGE == node->m_type)
        && (VF_INSTR_JSR == node->m_end->m_type)
        && (VF_NODE_CODE_RANGE == edge->m_end->m_type);
}

/**
 * Gets a subroutine number for a node. The function is slow and should be used for
 * debugging purposes only.
 * @param[in] sub  a given subroutine handle
 * @param[in] ctx  a verification context
 * @return a sequential subroutine number in a list
 */
unsigned vf_get_sub_num(vf_SubHandle sub, vf_ContextHandle ctx);

/**
 * Checks that a stack depth is the same as during last visit, calculates
 * a stack modifier, counts a number of visited nodes, for each instruction checks
 * that stack doesn't overflow.
 * @param[in, out] node a pointer to a node structure, <code>m_mark</code> is
 * modified during the call
 * @param[in, out] depth starts as node depth before the first node instruction and ends
 * as a node depth after the last node instruction
 * @param[in] ctx a verifier context
 * @return <code>VER_Continue</code> if this node wasn't visited before, and analysis of
 * subsequent nodes is needed, <code>VER_OK</code> if the stack depth is consistent, an
 * error code otherwise
 */
vf_Result vf_check_node_stack_depth(vf_Node *node,      // a graph node
                                    unsigned &depth,    // stack depth
                                    vf_Context *ctx);   // verification context

/**
 * Creates and sets graph node OUT vector.
 * @param[in] node     a graph node handle
 * @param[in, out] invector an IN vector, then OUT vector
 * @param[in] ctx     a verifier context
 * @return <code>VER_OK</code> if OUT vector wasn't changed,
 * <code>VER_Continue</code>
 * if the vector was changed successfully, an error code otherwise
 */
vf_Result
vf_set_node_out_vector(vf_NodeHandle node,
                       vf_MapVector *invector, vf_Context *ctx);

/**
 * Prints data flow vector into output stream.
 * @param vector a data flow vector
 * @param instr  a code instruction
 * @param stream an output stream (can be NULL)
 */
void vf_dump_vector(vf_MapVectorHandle vector,  // data flow vector
                    vf_InstrHandle instr,       // code instruction
                    ostream *stream);   // output stream (can be NULL)

#endif // _VF_GRAPH_H_
