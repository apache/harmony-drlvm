/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.6.22.4 $
 */

#ifndef _IA32_CODE_LAYOUT
#define _IA32_CODE_LAYOUT

#include "Ia32CFG.h"
#include "Ia32IRManager.h"
namespace Jitrino
{
namespace Ia32 {


    BEGIN_DECLARE_IRTRANSFORMER(Layouter, "layout", "Code layout")
        Layouter(IRManager& irm, const char * params=0): IRTransformer(irm, params){} 
        void runImpl();
        uint32 getSideEffects() const {return 0;}
        uint32 getNeedInfo()const{ return 0;}
    END_DECLARE_IRTRANSFORMER(Layouter) 

/**
 *  Base class for code layout 
 */
class Linearizer {
public:
    virtual ~Linearizer() {}
    enum LinearizerType { TOPOLOGICAL, TOPDOWN, BOTTOM_UP};
    static void doLayout(LinearizerType t, IRManager* irManager);
    static bool hasValidLayout(IRManager* irm);
    static bool hasValidFallthroughEdges(IRManager* irm);

protected:
    Linearizer(IRManager* irMgr);
    void linearizeCfg();
    virtual void linearizeCfgImpl() = 0;
    /** Fix branches to work with the code layout */
    void fixBranches();
    void fixBranches(BasicBlock* block);

    
    /** Returns true if edge can be converted to a fall-through edge (i.e. an edge
     * not requiring a branch) assuming the edge's head block is laid out after the tail block. 
     */
    bool canEdgeBeMadeToFallThrough(Edge *edge);
    
    /** checks if CFG has no BB nodes without layout successors*/
    bool isBlockLayoutDone();

    void ensureProfileIsValid() const;

    //  Fields
    IRManager* irManager;

private:
   /**  Add block containing jump instruction to the fallthrough successor
    *  after this block
    */
    BasicBlock * addJumpBlock(Edge * fallEdge);

    /**  Reverse branch predicate. We assume that branch is the last instruction
    *  in the node.
    */
    bool reverseBranchIfPossible(BasicBlock * bb);

};


/** 
 *   Reverse post-order (topological) code layout 
 */
class TopologicalLayout : public Linearizer {
    friend class Linearizer;
protected:
    TopologicalLayout(IRManager* irManager) : Linearizer(irManager){};
    virtual ~TopologicalLayout() {}
    void linearizeCfgImpl();
};

}} //namespace

#endif


