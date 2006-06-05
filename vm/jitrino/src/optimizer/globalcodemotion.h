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
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.13.20.4 $
 *
 */

#ifndef _GLOBAL_CODE_MOTION_H
#define _GLOBAL_CODE_MOTION_H

#include <iostream>
#include "open/types.h"
#include "Opcode.h"
#include "FlowGraph.h"
#include "Stl.h"
#include "optpass.h"
#include <utility>

namespace Jitrino {

class IRManager;
class FlowGraph;
class methodDesc;
class MemoryManager;
class InequalityGraph;
class DominatorNode;
class Dominator;
class JitrinoParameterTable;
class CFGNode;
class Opnd;
class CSEHashTable;
class Type;
class LoopTree;

/*
 * Implementation of Global Code Motion,
 * [C.Click. Global Code Motion, Global Value Numbering. PLDI 1995]
 */
 
DEFINE_OPTPASS(GlobalCodeMotionPass)

DEFINE_OPTPASS(GlobalValueNumberingPass)

class GlobalCodeMotion {
    IRManager& irManager;
    FlowGraph& fg;
    MethodDesc& methodDesc;
    MemoryManager &mm;
    DominatorTree& dominators;
    LoopTree *loopTree;
public:
    struct Flags {
        bool dry_run;
        bool sink_stvars;
        bool min_cut;
        bool sink_constants;
    };
private:
    static Flags *defaultFlags;
    Flags flags;
public:    
    static void readDefaultFlagsFromCommandLine(const JitrinoParameterTable *params);
    static void showFlagsFromCommandLine();

    GlobalCodeMotion(IRManager &irManager0, 
                     MemoryManager& memManager,
                     DominatorTree& dom0,
                     LoopTree *loopTree);
    
    ~GlobalCodeMotion();

    void runPass();

private:
    bool isPinned(Inst *i);
    StlHashMap<Inst*, DominatorNode *> earliest; // block for earliest placement
    StlHashMap<Inst*, DominatorNode *> latest;
    typedef StlHashSet<Inst *> VisitedSet;
    VisitedSet visited;
    typedef ::std::set<Inst *> UsesSet;
    typedef StlHashMap<Inst *, UsesSet> UsesMap;
    UsesMap uses;

    void scheduleAllEarly();
    void scheduleBlockEarly(CFGNode *n);
    void scheduleEarly(DominatorNode *domNode, Inst *i);

    void scheduleAllLate();
    void scheduleLate(DominatorNode *domNode, Inst *basei, Inst *i);
    void sinkAllConstants();
    void sinkConstants(Inst *i);

    // utils
    DominatorNode *leastCommonAncestor(DominatorNode *a, DominatorNode *b);

    void markAsVisited(Inst *i);
    bool alreadyVisited(Inst *i);
    void clearVisited();

    friend class GcmScheduleEarlyWalker;
    friend class GcmScheduleLateWalker;
    friend class GcmSinkConstantsWalker;
};

} //namespace Jitrino 

#endif // _GLOBAL_CODE_MOTION_H
